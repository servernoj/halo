import express from 'express'
import { Worker } from 'node:worker_threads'
import { workerFileName } from './worker.js'

new Worker(workerFileName, {
  name: 'PIR',
})

const app = express()

app.get('/health', (req, res) => res.json({ status: 'ok' }))
const PORT = 3000
app.listen(PORT, () => console.log(`Server listening on http://localhost:${PORT}`))
