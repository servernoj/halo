import { register } from "node:module";
import { Worker } from 'node:worker_threads'

const init = async () => {
  register("esm-module-alias/loader", import.meta.url);
  const { workerFileName } = await import('./worker.js')
  const worker = new Worker(workerFileName, {
    name: 'PIR',
  })
  const { init: initServer } = await import('./server.js')
  initServer(worker)
}

init()