import { readRegister } from './i2c-util.js'

export const getVersion = async () => {
  const buffer = await readRegister(0x30)
  return buffer.toString('utf8').split('\0')[0]
}

export const getBuildDate = async () => {
  const buffer = await readRegister(0x31)
  return buffer.toString('utf8').split('\0')[0]
}

export const getBuildTime = async () => {
  const buffer = await readRegister(0x32)
  return buffer.toString('utf8').split('\0')[0]
}

export const getInfo = async () => {
  const buffer = await readRegister(0x33)
  const version = buffer.toString('utf8', 0, 32).split('\0')[0]
  const buildDate = buffer.toString('utf8', 32, 48).split('\0')[0]
  const buildTime = buffer.toString('utf8', 48).split('\0')[0]
  return {
    version,
    buildDate,
    buildTime
  }
}
