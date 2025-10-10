import i2c from 'i2c-bus'

const BUS = 1
const ADDR = 0x12
const REG = {
  DET_Q_STATUS: 0x07,
  DET_Q_FRONT: 0x08,
  DET_Q_BACK: 0x0C,
  REM_Q_STATUS: 0x10,
  REM_Q_FRONT: 0x11,
  REM_Q_BACK: 0x15
}
const BIT = {
  Q_POP: 0x01,
  Q_EMPTY: 0x02,
  Q_FULL: 0x04
}
const POLL_MS = 75
const CONFIRM_MS = 3000        // “occupied” only if detect is ≥ 3s old
const VACATE_HOLDOFF_MS = 2000 // optional grace after a newer remove

const bus = await i2c.openPromisified(BUS)
const sleep = (ms) => (new Promise(
  resolve => {
    const timer = setTimeout(
      () => {
        clearTimeout(timer)
        resolve(null)
      },
      ms
    )
  }
))

async function readU32LE(reg) {
  const b = Buffer.alloc(4)
  await bus.readI2cBlock(ADDR, reg, 4, b)
  return b.readUInt32LE(0)
}
async function readStatus(reg) {
  return bus.readByte(ADDR, reg)
}
async function pop(reg) {
  await bus.writeByte(ADDR, reg, BIT.Q_POP)
}

// Keep only the newest element in a queue (so FRONT == BACK)
// Pop oldest items until backAge <= frontAge (+ small jitter)
async function condenseToLatest(statusReg, frontReg, backReg) {
  const JITTER_MS = 5
  while (true) {
    const st = await readStatus(statusReg)
    if (st & BIT.Q_EMPTY) break
    const front = await readU32LE(frontReg)
    const back = await readU32LE(backReg)
    if (back <= front + JITTER_MS) break // single element (newest only)
    await pop(statusReg) // drop oldest, keep newest
  }
}

// Snapshot “most recent” ages (non-destructive)
async function frontSnapshot() {
  const now = Date.now()
  const detSt = await readStatus(REG.DET_Q_STATUS)
  const remSt = await readStatus(REG.REM_Q_STATUS)

  const detEmpty = !!(detSt & BIT.Q_EMPTY)
  const remEmpty = !!(remSt & BIT.Q_EMPTY)

  const detAge = detEmpty ? null : await readU32LE(REG.DET_Q_FRONT) // ms since newest detect
  const remAge = remEmpty ? null : await readU32LE(REG.REM_Q_FRONT) // ms since newest remove

  return {
    now,
    det: { empty: detEmpty, ageMs: detAge, full: !!(detSt & BIT.Q_FULL) },
    rem: { empty: remEmpty, ageMs: remAge, full: !!(remSt & BIT.Q_FULL) }
  }
}

// App-level state
let occupied = false
function onOccupied(t) {
  console.log(`[${new Date(t).toISOString()}] occupied`)
}
function onVacant(t) {
  console.log(`[${new Date(t).toISOString()}] vacant`)
}

setInterval(async () => {
  try {
    // 1) Read newest ages from both queues
    const snap = await frontSnapshot()

    // 2) If a queue is FULL, condense it to keep only the newest element
    if (snap.det.full) await condenseToLatest(REG.DET_Q_STATUS, REG.DET_Q_FRONT, REG.DET_Q_BACK)
    if (snap.rem.full) await condenseToLatest(REG.REM_Q_STATUS, REG.REM_Q_FRONT, REG.REM_Q_BACK)

    // 3) Decide current “latest event” ordering using FRONT ages (smaller age = more recent)
    const hasDet = !snap.det.empty
    const hasRem = !snap.rem.empty
    const detIsNewer = hasDet && (!hasRem || snap.det.ageMs < snap.rem.ageMs)
    const remIsNewer = hasRem && (!hasDet || snap.rem.ageMs < snap.det.ageMs)

    // 4) Confirm occupancy ONLY if the newest DETECT is ≥ CONFIRM_MS old and newer than newest REMOVE
    if (!occupied && detIsNewer && snap.det.ageMs >= CONFIRM_MS) {
      occupied = true
      // Anchor the emit time at the confirmation boundary (purely cosmetic)
      onOccupied(snap.now - (snap.det.ageMs - CONFIRM_MS))
    }

    // 5) Vacate when the newest REMOVE is newer than the newest DETECT and has aged past a small hold-off
    if (occupied && remIsNewer && snap.rem.ageMs >= VACATE_HOLDOFF_MS) {
      occupied = false
      onVacant(snap.now - (snap.rem.ageMs - VACATE_HOLDOFF_MS))
    }

    // Note: We never POP during normal operation, so FRONT always reflects the latest event.
    // We only POP when a queue is FULL, trimming oldest entries while preserving the newest.
  } catch (e) {
    console.error('queue/front loop error:', e)
  }
}, POLL_MS)

// Optional: key controls — [d] condense DET to newest, [r] condense REM to newest, [q] quit
process.stdin.setRawMode?.(true); process.stdin.resume()
console.log('Controls: [d]=trim DET to newest, [r]=trim REM to newest, [q]/Ctrl-C=quit')
process.stdin.on('data', async (b) => {
  const ch = String(b)
  if (ch === 'd') await condenseToLatest(REG.DET_Q_STATUS, REG.DET_Q_FRONT, REG.DET_Q_BACK)
  else if (ch === 'r') await condenseToLatest(REG.REM_Q_STATUS, REG.REM_Q_FRONT, REG.REM_Q_BACK)
  else if (ch === 'q' || ch === '\u0003') { await bus.close(); process.exit(0) }
})
