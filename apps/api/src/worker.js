import { isMainThread, parentPort } from 'node:worker_threads'
import { register } from 'node:module'

const STATUS_BITS = {
  RAW: 0x01,
  AVAILABLE: 0x02,
  REM: 0x04,
  DET: 0x08
}
const MASKS = {
  CLEAR: 0x0E
}
const DEVICE_ADDR = 0x12
const STATUS_REG = 0x03

const init = async () => {
  register('esm-module-alias/loader', import.meta.url)
  const i2c = await import('@/i2c-stub.js')
  const { sleep, buzzer } = await import('@/tools/index.js')
  const bus = await i2c.default.openPromisified(1)

  let active = false

  const onActive = async () => {
    await buzzer.runProfile([{ note: 'E5', length: 250 }], false)
    await sleep(3000)
  }

  const start = (interval) => {
    const timer = setInterval(
      async () => {
        const status = await bus.readByte(DEVICE_ADDR, STATUS_REG)
        if (status & STATUS_BITS.AVAILABLE) {
          if (!active && (status & STATUS_BITS.DET)) {
            active = true
            onActive().then(
              () => {
                active = false
              }
            )
          }
          await bus.writeByte(DEVICE_ADDR, STATUS_REG, status & (~MASKS.CLEAR))
        }
      },
      interval
    )
    return timer
  }

  let timer = start(100)

  parentPort.on('message', (message) => {
    if (timer) {
      clearInterval(timer)
      if (message > 0) {
        console.log(`Polling interval set to ${message} ms`)
        timer = start(message)
      } else {
        console.log('Polling stopped')
      }
    }
  })
  console.log('Worker started')
}

if (!isMainThread) {
  init()
}

export const workerFileName = new URL(import.meta.url)