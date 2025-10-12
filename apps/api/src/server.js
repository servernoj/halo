import express from 'express'
import morgan from 'morgan'
import { queryTypes, fallback, errorHandler } from '@/controller/mw/index.js'
import motor from '@/controller/motor.js'
import ota from '@/controller/ota.js'
import pir from '@/controller/pir.js'
import buzzer from '@/controller/buzzer.js'
import firmware from '@/controller/firmware.js'

const app = express()
app.use(morgan('dev'))
app.use(express.json(), queryTypes)

app.get('/health', (req, res) => res.sendStatus(200))
app.use('/motor', motor)
app.use('/ota', ota)
app.use('/pir', pir)
app.use('/firmware', firmware)
app.use('/buzzer', buzzer)

app.use(fallback)
app.use(errorHandler)

app.listen(3000, () => console.log('Server started'))

export const init = (worker) => {
  app.locals.worker = worker
}

