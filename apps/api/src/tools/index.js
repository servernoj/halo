import * as motor from './motor.js'
import * as ota from './ota.js'
import * as buzzer from './buzzer.js'
import * as relay from './relay.js'

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

export {
  sleep,
  motor,
  ota,
  buzzer,
  relay
}