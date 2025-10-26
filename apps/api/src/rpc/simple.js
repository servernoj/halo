const ADDR = 0x12

const REGS = {
  STATUS: 0x03,
}
const BITS = {
  ST_AVAILABLE: 0x02,
  ST_REM: 0x04,
  ST_DET: 0x08,
  CLEAR: 0x0E
}

export default (rpcRequest, { threshold = 5 }) => {
  const onActive = async () => {
    const motor = async () => {
      await rpcRequest({
        target: 'motor',
        method: 'release',
        args: []
      })
      await rpcRequest({
        target: 'tools',
        method: 'sleep',
        args: [4_000]
      })
      await rpcRequest({
        target: 'motor',
        method: 'runProfile',
        args: [
          [
            { degrees: 90, delay: 0 },
            { degrees: -90, delay: 0 }
          ],
          4
        ]
      })
      await rpcRequest({
        target: 'motor',
        method: 'freeRun',
        args: [-1]
      })
    }
    const delay = async () => {
      await rpcRequest({
        target: 'tools',
        method: 'sleep',
        args: [Math.random() * 4_000 + 4_000]
      })
    }
    console.log('>>>')
    motor()
    await delay()
    await rpcRequest({
      target: 'relay',
      method: 'on_off',
      args: []
    })
    await rpcRequest({
      target: 'tools',
      method: 'sleep',
      args: [60_000]
    })
  }
  const readByte = async (reg) => {
    const result = await rpcRequest({
      target: 'i2c',
      method: 'readByte',
      args: [ADDR, reg]
    })
    return result
  }
  const readU32LE = async (reg) => {
    const b = await rpcRequest({
      target: 'i2c',
      method: '_readI2cBlock',
      args: [ADDR, reg]
    })
    return Buffer.from(b).readUInt32LE()
  }
  const writeByte = async (reg, byte) => {
    await rpcRequest({
      target: 'i2c',
      method: 'writeByte',
      args: [ADDR, reg, byte]
    })
  }
  let isDetPrev = 0
  let cnt = 0
  let isActive = false
  return async () => {
    if (isActive) {
      return
    }
    const status = await readByte(REGS.STATUS)
    const [isDet, isAvailable] = [
      status & BITS.ST_DET ? 1 : 0,
      status & BITS.ST_AVAILABLE ? 1 : 0
    ]
    if (isDet && isDetPrev) {
      cnt++
    }
    if (!isDet && isDetPrev) {
      // -- falling edge on isDet
      if (cnt > threshold) {
        isActive = true
        onActive().then(
          () => {
            isActive = false
            console.log('<<<')
          }
        )
      }
      console.log('+++', cnt)
      cnt = 0
    }
    if (isAvailable) {
      await writeByte(REGS.STATUS, status & (~BITS.CLEAR))
    }
    isDetPrev = isDet
  }
}