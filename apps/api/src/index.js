import { register } from "node:module";
import { Worker } from 'node:worker_threads'

const init = async () => {
  register("esm-module-alias/loader", import.meta.url);
  const { workerFileName } = await import('./worker.js')
  new Worker(workerFileName, {
    name: 'PIR',
  })
  await import('./server.js')

}

init()