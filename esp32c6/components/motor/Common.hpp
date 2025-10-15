#pragma once
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace motor {

  typedef uint32_t MotorCmdId;
  typedef struct {
      gpio_num_t stby, en;
      gpio_num_t m0, m1, m2, m3;
      gpio_num_t stop;
  } motor_pins_t;

  enum class StepMode : uint8_t {
    FixedFull = 0b1000,
    FixedHalf = 0b1001,
    FixedQuarter = 0b1010,
    FixedEighth = 0b1011,
    FixedSixteenth = 0b1100,
    FixedThirtySecond = 0b1101,
    FixedSixtyFourth = 0b1110,
    FixedOneTwentyEighth = 0b1111,
  };
  enum class EndAction { HOLD, COAST };
  enum class MoveType { FIXED, FREE, STOP };
  enum class MotorState { IDLE, DELAYED, STARTED, ERRORED };

  struct MotorCfg {
    public:
      uint8_t en_active_level; // 0/1
      StepMode step_mode;
      uint8_t dir_cw_level; // logic level that means CW on pin_m3
      motor_pins_t pins;
  };

  struct Move {
    public:
      int32_t steps {0}; // +CW, -CCW
      uint32_t period_us {500}; // >0
      uint32_t high_us {50}; // >0 and < period_us
      uint32_t delay_ms {0}; // pre-exec delay after dequeue
      EndAction end_action {EndAction::COAST};
      MoveType move_type {MoveType::FIXED};
  };

} // namespace motor