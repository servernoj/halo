import i2c from 'i2c-bus'

export default async (...args: any) => {
  const bus = await i2c.openPromisified(1)
  const deviceAddr = 0x42
  const register = 0x10
  const command = 0x02

  const profile = [
    { steps: 512, delay: 0 }
  ]
  const buf = Buffer.alloc(2)
  const payload = profile.reduce(
    (acc, move) => {
      buf.writeInt16LE(move.steps)
      acc.push(...buf)
      buf.writeUInt16LE(move.delay)
      acc.push(...buf)
      return acc
    },
    [] as Array<number>
  )
  const data = [register, command, ...payload]

  // Send data
  await bus.i2cWrite(deviceAddr, data.length, Buffer.from(data))
  await bus.close()
}

