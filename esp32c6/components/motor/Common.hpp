#pragma once
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace motor {

  static constexpr int STEPS_PER_REVOLUTION = 200;
  static constexpr uint32_t START_PERIOD_US = 4096;

  typedef uint32_t MotorCmdId;
  typedef struct {
      gpio_num_t stby, en;
      gpio_num_t m0, m1, m2, m3;
      gpio_num_t stop;
  } motor_pins_t;

  struct SegmentData {
      int32_t steps;
      uint32_t period_us;
  };

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
  enum class MoveType { FIXED, FREE, STOP, HOLD, RELEASE };
  enum class MotorState { IDLE, DELAYED, STARTED, ERRORED };

  struct MotorCfg {
    public:
      motor_pins_t pins;
  };

  struct Move {
    public:
      int32_t degrees {0}; // +CW, -CCW in degrees
      uint32_t delay_ms {0}; // pre-exec delay after dequeue
      EndAction end_action {EndAction::COAST};
      MoveType move_type {MoveType::FIXED};
  };

} // namespace motor