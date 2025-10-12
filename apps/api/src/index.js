import { register } from 'node:module'
// import { Worker } from 'node:worker_threads'
// import { workerFileName } from './worker.js'

const init = async () => {
  register('esm-module-alias/loader', import.meta.url)
  // const worker = new Worker(workerFileName, {
  //   name: 'PIR',
  // })
  const { init: initServer } = await import('./server.js')
  // const { init: initWorker } = await import('./worker.js')
  // initWorker()
  initServer(null)
}

init()