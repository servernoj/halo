import i2c from 'i2c-bus'

export default async (...args: any) => {
  const url = args[0]
  const bus = await i2c.openPromisified(1)
  const deviceAddr = 0x42
  const register = 0x11
  const command = 0x00

  // Prepare data: register + command + URL
  const data = [register, command, ...Buffer.from(url, 'utf8')]

  // Send data
  await bus.i2cWrite(deviceAddr, data.length, Buffer.from(data))
  await bus.close()
}

