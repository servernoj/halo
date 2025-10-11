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
  const cmdBuffer = Buffer.from([0x01])
  const dataBuffer = Buffer.alloc(64)
  await bus.i2cWrite(deviceAddr, cmdBuffer.length, cmdBuffer)
  await bus.i2cRead(deviceAddr, dataBuffer.length, dataBuffer)
  await bus.close()
  const str = dataBuffer.toString('utf8')
  return str.slice(0, str.indexOf('\0'))
}

