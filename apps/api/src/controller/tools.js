import express from 'express'
import * as tools from '@/tools/index.js'
import { validator } from '@/controller/mw/index.js'
import z from 'zod'

const router = express.Router()

router.post(
  '/motor/profile',
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
  '/ota/update',
  async (req, res) => {
    await tools.ota.triggerUpdate()
    res.sendStatus(200)
  }
)

router.post(
  '/ota/url',
  validator({
    body: z.object({
      url: z.string()
    })
  }),
  async (req, res) => {
    const { url } = res.locals.parsed.body
    await tools.ota.setUrl(url)
    res.sendStatus(200)
  }
)

router.get(
  '/ota/url',
  async (req, res) => {
    const url = await tools.ota.getUrl()
    res.json({ url })
  }
)


export default router