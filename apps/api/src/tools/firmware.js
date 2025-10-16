import { readRegister } from './i2c-util.js'

export const getVersion = async () => {
  const buffer = await readRegister(0x30, 64)
  return buffer.toString('utf8').split('\0')[0]
}

export const getBuildDate = async () => {
  const buffer = await readRegister(0x31, 64)
  return buffer.toString('utf8').split('\0')[0]
}

export const getBuildTime = async () => {
  const buffer = await readRegister(0x32, 64)
  return buffer.toString('utf8').split('\0')[0]
}
