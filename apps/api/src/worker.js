import { isMainThread, parentPort } from 'node:worker_threads'
export const workerFileName = new URL(import.meta.url)

const start = (interval) => {
  let cnt = 0

  const timer = setInterval(() => {
    console.log(cnt++)
  }, interval)
  return timer
}

if (!isMainThread) {
  let timer = start(10000)
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