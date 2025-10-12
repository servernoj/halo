import express from 'express'
import morgan from 'morgan'
import { queryTypes, fallback, errorHandler } from '@/controller/mw/index.js'
import tools from '@/controller/tools.js'
import firmware from '@/controller/firmware.js'

const app = express()
app.use(morgan("dev"))
app.use(express.json(), queryTypes);

app.get('/health', (req, res) => res.sendStatus(200))
app.use('/tools', tools)
app.use('/firmware', firmware)

app.use(fallback);
app.use(errorHandler);

app.listen(3000, () => console.log('Server started'))

export default app

