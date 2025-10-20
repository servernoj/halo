import express from 'express'
import { validator } from '@/controller/mw/index.js'
import z from 'zod'

const router = express.Router()

router.post(
  '/config',
  validator({
    body: z.object({
      interval: z.number().int().default(0),
      threshold: z.number().int().positive()
    })
  }),
  async (req, res) => {
    const { interval, threshold } = res.locals.parsed.body
    const { worker } = req.app.locals
    worker.postMessage({ result: { interval, threshold } })
    res.sendStatus(200)
  }
)

export default router