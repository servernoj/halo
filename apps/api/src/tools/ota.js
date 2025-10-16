import { readRegister, writeRegister } from './i2c-util.js'

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
  // Limit URL to 63 bytes (leave room for register address)
  const urlBuffer = Buffer.from(url, 'utf8')
  if (urlBuffer.length > 63) {
    console.error(`URL too long: ${urlBuffer.length} bytes, max 63`)
    return
  }
  await writeRegister(0x10, urlBuffer)
}

export const getUrl = async () => {
  const buffer = await readRegister(0x10, 64)
  const str = buffer.toString('utf8')
  return str.slice(0, str.indexOf('\0'))
}

export const triggerUpdate = async () => {
  await writeRegister(0x11, Buffer.from([0x01]))
}

export const getStatus = async () => {
  const state = (await readRegister(0x12, 1))[0]
  const progress = (await readRegister(0x13, 1))[0]
  const errorCodeBuf = await readRegister(0x14, 4)
  const errorCode = errorCodeBuf.readInt32LE(0)
  const bytesBuf = await readRegister(0x15, 4)
  const bytesDownloaded = bytesBuf.readUInt32LE(0)
  const errorFuncBuf = await readRegister(0x16, 32)
  const errorFunction = errorFuncBuf.toString('utf8').split('\0')[0]

  const STATE_NAMES = {
    0: 'UNKNOWN',
    1: 'IDLE',
    2: 'DOWNLOADING',
    3: 'VERIFYING',
    4: 'FLASHING',
    5: 'COMPLETE',
    6: 'ERROR'
  }

  return {
    state,
    stateName: STATE_NAMES[state] || 'UNKNOWN',
    progress,
    errorCode,
    bytesDownloaded,
    errorFunction
  }
}

export const resetStatus = async () => {
  await writeRegister(0x17, Buffer.from([0x01]))
}
