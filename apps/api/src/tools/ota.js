import i2c from '@/i2c-stub.js'

const deviceAddr = 0x42
const register = 0x11


/**
 * @param {string} url URL for the OTA update firmware file
 */
export const setUrl = async (url = '') => {
  if (
    !url ||
    !/^http[s]?:[/][/]\S+$/i.test(url)
  ) {
    console.error(`Invalid URL ${url}`)
    return
  }
  const bus = await i2c.openPromisified(1)
  const data = [0x11, 0x00, ...Buffer.from(url, 'utf8')]
  await bus.i2cWrite(deviceAddr, data.length, Buffer.from(data))
  await bus.close()
}

export const getUrl = async () => {
  const bus = await i2c.openPromisified(1)
  const buf = Buffer.alloc(64)
  const result = await bus.readI2cBlock(deviceAddr, 0x01, 32, buf)
  await bus.close()
  return result
}

