import express from 'express'
import { validator } from '@/controller/mw/index.js'
import z from 'zod'

const router = express.Router()

router.post(
  '/config',
  validator({
    body: z.object({
      interval: z.number().int().default(1000)
    })
  }),
  async (req, res) => {
    const { interval } = res.locals.parsed.body
    const { worker } = req.app.locals
    worker.postMessage(interval)
    res.sendStatus(200)
  }
)

export default router