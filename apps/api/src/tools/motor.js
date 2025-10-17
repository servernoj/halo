import { writeRegister } from './i2c-util.js'

/**
 * @param {Array<{steps: number, delay: number}>} profile A sequence of motor steps to run in FIXED + COAST mode
 */
export const runProfile = async (profile = []) => {
  // Each move is 4 bytes (int16 + uint16)
  // Max 15 moves per I2C write (60 bytes)
  const MAX_MOVES_PER_WRITE = 15

  for (let i = 0; i < profile.length; i += MAX_MOVES_PER_WRITE) {
    const chunk = profile.slice(i, i + MAX_MOVES_PER_WRITE)
    const buffer = Buffer.alloc(chunk.length * 4)

    chunk.forEach((move, idx) => {
      buffer.writeInt16LE(move.steps, idx * 4)
      buffer.writeUInt16LE(move.delay, idx * 4 + 2)
    })

    await writeRegister(0x23, buffer)
  }
}

/**
 * @param {number} dir Direction (+1/-1) of the free run
 */
export const freeRun = async (dir = -1) => {
  const dirByte = dir > 0 ? 0x01 : 0xFF
  await writeRegister(0x22, Buffer.from([dirByte]))
}

export const stop = async () => {
  await writeRegister(0x21, Buffer.from([0x01]))
}

export const hold = async () => {
  await writeRegister(0x24, Buffer.from([0x01]))
}

export const release = async () => {
  await writeRegister(0x25, Buffer.from([0x01]))
}
