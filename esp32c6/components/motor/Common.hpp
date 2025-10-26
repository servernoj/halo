#pragma once
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "StepMode.hpp"

namespace motor {

  static constexpr int STEPS_PER_REVOLUTION = 200;
  static constexpr uint32_t BASE_PERIOD_US = 3072;

  typedef uint32_t MotorCmdId;
  typedef struct {
      gpio_num_t stby, en;
      gpio_num_t m0, m1, m2, m3;
      gpio_num_t stop;
  } MotorPins;

  struct SegmentData {
      int32_t steps;
      uint32_t period_us;
  };

  enum class EndAction { HOLD, COAST };
  enum class MoveType { FIXED, FREE, STOP, HOLD, RELEASE };
  enum class MotorState { IDLE, DELAYED, STARTED, ERRORED };

  struct MotorCfg {
    public:
      StepMode stepMode;
      MotorPins pins;
  };

  struct Move {
    public:
      int32_t degrees {0}; // +CW, -CCW in degrees
      uint32_t delay_ms {0}; // pre-exec delay after dequeue
      EndAction end_action {EndAction::COAST};
      MoveType move_type {MoveType::FIXED};
  };

  const char *moveTypeToName(MoveType move);

} // namespace motor