import { isMainThread, parentPort } from 'node:worker_threads'
import { randomUUID } from 'node:crypto'

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


if (!isMainThread) {
  const pending = new Map()
  let timer
  let active = false

  const rpcRequest = ({ target, method, args }) => new Promise(
    (resolve, reject) => {
      const id = randomUUID()
      pending.set(id, { resolve, reject })
      parentPort.postMessage({ id, target, method, args })
    }
  )

  const onActive = async () => {
    console.log('>>>')
    await rpcRequest({
      target: 'buzzer',
      method: 'runProfile',
      args: [[{ note: 'E5', length: 250 }], false]
    })
    await rpcRequest({
      target: 'tools',
      method: 'sleep',
      args: [3000]
    })
  }

  parentPort.on('message', (msg) => {
    const { id, ok, result, error } = msg
    if (id) {
      const entry = pending.get(id)
      if (!entry) {
        return
      }
      pending.delete(id)
      ok
        ? entry.resolve(result)
        : entry.reject(new Error(error))
    } else {
      if (timer) {
        clearInterval(timer)
      }
      if (result > 0) {
        console.log(`Polling interval set to ${result} ms`)
        timer = taskLoop(result)
      } else {
        console.log('Polling stopped')
      }
    }
  })

  const taskLoop = (interval) => {
    const timer = setInterval(
      async () => {
        const status = await rpcRequest({
          target: 'i2c',
          method: 'readByte',
          args: [DEVICE_ADDR, STATUS_REG]
        })
        if (status & STATUS_BITS.AVAILABLE) {
          if (!active && (status & STATUS_BITS.DET)) {
            active = true
            onActive().then(
              () => {
                active = false
              }
            )
          }
          await rpcRequest({
            target: 'i2c',
            method: 'writeByte',
            args: [DEVICE_ADDR, STATUS_REG, status & (~MASKS.CLEAR)]
          })
        }
      },
      interval
    )
    return timer
  }
  console.log('Worker started')
}

export const workerFileName = new URL(import.meta.url)