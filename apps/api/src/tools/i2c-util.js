import i2c from '@/i2c-stub.js'

const deviceAddr = 0x42

/**
 * Read from an I2C register
 * @param {number} regAddr Register address (0x00-0xFF)
 * @param {number} length Number of bytes to read
 * @returns {Promise<Buffer>} Buffer containing the read data
 */
export const readRegister = async (regAddr, length) => {
  const bus = await i2c.openPromisified(1)
  const cmdBuffer = Buffer.from([regAddr])
  const dataBuffer = Buffer.alloc(length)
  await bus.i2cWrite(deviceAddr, cmdBuffer.length, cmdBuffer)
  await bus.i2cRead(deviceAddr, dataBuffer.length, dataBuffer)
  await bus.close()
  return dataBuffer
}

/**
 * Write to an I2C register
 * @param {number} regAddr Register address (0x00-0xFF)
 * @param {Buffer} data Data to write (Buffer or array of bytes)
 * @returns {Promise<void>}
 */
export const writeRegister = async (regAddr, data) => {
  const bus = await i2c.openPromisified(1)
  const buffer = Buffer.from([regAddr, ...data])
  await bus.i2cWrite(deviceAddr, buffer.length, buffer)
  await bus.close()
}
