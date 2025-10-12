import express from 'express'
import * as tools from '@/tools/index.js'
import { validator } from '@/controller/mw/index.js'
import z from 'zod'
import { buzzer } from '@/tools/index.js'

const supportedNotes = Object.keys(buzzer.notes)

const router = express.Router()

router.post(
  '/profile',
  validator({
    body: z.object({
      profile: z.array(
        z.object({
          note: z.enum(supportedNotes),
          length: z.number().nonnegative()
        })
      )
    })
  }),
  async (req, res) => {
    const { profile } = res.locals.parsed.body
    await tools.buzzer.runProfile(profile)
    res.sendStatus(200)
  }
)

export default router