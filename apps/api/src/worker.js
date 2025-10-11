import { isMainThread } from 'node:worker_threads'
export const workerFileName = new URL(import.meta.url)

if (!isMainThread) {
  let cnt = 0
  setInterval(() => {
    console.log(cnt++)
  }, 10000)
  console.log('Worker started')
}