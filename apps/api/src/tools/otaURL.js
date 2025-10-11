import i2c from '@/i2c-stub.js'

const deviceAddr = 0x42
const register = 0x11
const command = 0x00

/**
 * @param {string} url URL for the OTA update firmware file
 */
export default async (url = '') => {
  if (
    !url ||
    !/^http[s]?:[/][/]\S+$/i.test(url)
  ) {
    console.error(`Invalid URL ${url}`)
    return
  }
  const bus = await i2c.openPromisified(1)
  const data = [register, command, ...Buffer.from(url, 'utf8')]
  await bus.i2cWrite(deviceAddr, data.length, Buffer.from(data))
  await bus.close()
}

