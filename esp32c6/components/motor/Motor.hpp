#pragma once

#include <atomic>
#include <memory>

#include "Common.hpp"
#include "MotorHal.hpp"

namespace motor {
  struct QueuedCmd {
      Move mv;
      MotorCmdId id;
  };
  class Motor {
    public:
      static Motor &instance() {
        static Motor s;
        return s;
      }
      esp_err_t init();
      esp_err_t submit(const Move &mv);
      MotorState getState();

    private:
      static void taskTrampoline(void *arg);
      bool initialized_ = false;
      static constexpr const char *TAG = "Motor";
      static constexpr int kQueueDepth = 8;
      std::atomic<MotorState> motor_state_ {MotorState::IDLE};
      void taskLoop();
      std::unique_ptr<MotorHal> hal_;
      QueueHandle_t cmd_q_ = nullptr;
      QueueHandle_t done_q_ = nullptr;
      TaskHandle_t task_ = nullptr;
      MotorCmdId next_id_ = 1;
      bool stop_requested_ = false;
      // Constructor business
      Motor() = default;
      Motor(const Motor &) = delete;
      Motor &operator=(const Motor &) = delete;
  };

} // namespace motor
