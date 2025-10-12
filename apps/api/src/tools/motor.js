import i2c from '@/i2c-stub.js'

const deviceAddr = 0x42


/**
 * @param {Array<{steps: number, delay: number}>} profile A sequence of motor steps to run in FIXED + COAST mode
 */
export const runProfile = async (profile = []) => {
  const bus = await i2c.openPromisified(1)
  const buf = Buffer.alloc(2)
  const payload = profile.slice(0, 16).reduce(
    (acc, move) => {
      buf.writeInt16LE(move.steps)
      acc.push(...buf)
      buf.writeUInt16LE(move.delay)
      acc.push(...buf)
      return acc
    },
    []
  )
  const data = [0x10, 0x02, ...payload]
  await bus.i2cWrite(deviceAddr, data.length, Buffer.from(data))
  await bus.close()
}

/**
 * @param {number} dir Direction (+1/-1) of the free run
 */
export const freeRun = async (dir = -1) => {
  const bus = await i2c.openPromisified(1)
  const data = [0x10, 0x01, dir > 0 ? 0x01 : 0xFF]
  await bus.i2cWrite(deviceAddr, data.length, Buffer.from(data))
  await bus.close()
}

export const stop = async () => {
  const bus = await i2c.openPromisified(1)
  const data = [0x10, 0x00]
  await bus.i2cWrite(deviceAddr, data.length, Buffer.from(data))
  await bus.close()
}