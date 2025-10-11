import i2c from '@/i2c-stub.js'

const deviceAddr = 0x42
const register = 0x10
const command = 0x02

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
  const data = [register, command, ...payload]
  await bus.i2cWrite(deviceAddr, data.length, Buffer.from(data))
  await bus.close()
}
