import express from 'express'
import * as tools from '@/tools/index.js'
import { validator } from '@/controller/mw/index.js'
import z from 'zod'

const router = express.Router()

router.post(
  '/motor-profile',
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
    await tools.motorProfile(profile)
    res.sendStatus(200)
  }
)

router.post(
  '/set-ota-url',
  validator({
    body: z.object({
      url: z.string()
    })
  }),
  async (req, res) => {
    const { url } = res.locals.parsed.body
    await tools.otaURL(url)
    res.sendStatus(200)
  }
)


export default router