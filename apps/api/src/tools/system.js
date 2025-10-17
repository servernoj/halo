import { readRegister, writeRegister } from './i2c-util.js'

export const getDeviceID = async () => {
  const buffer = await readRegister(0xF0)
  return `0x${buffer.readUInt8().toString(16)}`
}

export const requestRestart = async () => {
  await writeRegister(0xF1, null)
}

export const resetI2C = async () => {
  await writeRegister(0xF2, null)
}

