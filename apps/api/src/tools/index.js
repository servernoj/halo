import * as motor from './motor.js'
import * as ota from './ota.js'
import * as buzzer from './buzzer.js'
import * as relay from './relay.js'
import i2c from '@/i2c-stub.js'

const sleep = (ms) => new Promise(
  resolve => {
    const timer = setTimeout(
      () => {
        clearTimeout(timer)
        resolve(null)
      },
      ms
    )
  }
)

const bus = await i2c.openPromisified(1)

const messageProcessor = async ({ target, method, args }) => {
  switch (target) {
    case 'buzzer': {
      const handler = buzzer?.[method]
      if (typeof handler === 'function') {
        await handler(...args)
      }
      break
    }
    case 'i2c': {
      const handler = bus?.[method]
      if (typeof handler === 'function') {
        const result = await handler.call(bus, ...args)
        return result
      }
      break
    }
    case 'tools': {
      if (method === 'sleep') {
        await sleep(args[0])
      }
      break
    }
    default:
      throw new Error(`Unknown target in request: ${target}`)
  }
}

export {
  messageProcessor,
  sleep,
  motor,
  ota,
  buzzer,
  relay
}