import express from 'express'
import * as tools from '@/tools/index.js'
import { validator } from '@/controller/mw/index.js'
import z from 'zod'

const router = express.Router()

router.post(
  '/profile',
  validator({
    body: z.object({
      profile: z.array(
        z.object({
          steps: z.number().int(),
          delay: z.number().nonnegative()
        })
      )
    })
  }),
  async (req, res) => {
    const { profile } = res.locals.parsed.body
    await tools.motor.runProfile(profile)
    res.sendStatus(200)
  }
)

router.post(
  '/free',
  validator({
    body: z.object({
      dir: z.number().int().default(-1)
    })
  }),
  async (req, res) => {
    const { dir } = res.locals.parsed.body
    await tools.motor.freeRun(dir)
    res.sendStatus(200)
  }
)

router.post(
  '/stop',
  async (req, res) => {
    await tools.motor.stop()
    res.sendStatus(200)
  }
)
router.post(
  '/hold',
  async (req, res) => {
    await tools.motor.hold()
    res.sendStatus(200)
  }
)
router.post(
  '/release',
  async (req, res) => {
    await tools.motor.release()
    res.sendStatus(200)
  }
)




export default router