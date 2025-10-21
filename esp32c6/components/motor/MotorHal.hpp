#pragma once
#include <driver/gpio_filter.h>
#include <driver/ledc.h>
#include <driver/pulse_cnt.h>
#include <vector>

#include "Common.hpp"

namespace motor {

  struct SegmentData {
      uint32_t steps;
      uint32_t period_us;
  };

  class MotorHal {
    public:
      esp_err_t init(MotorCfg &hw);
      esp_err_t startMove(Move &mv, MotorCmdId id);
      esp_err_t stopMove();
      esp_err_t holdOrRelease(bool doHold);
      void registerTaskHandle(TaskHandle_t h) { task_ = h; }
      void onStopISR();

    private:
      static constexpr const char *TAG = "MotorHal";
      MotorCfg motor_cfg_ {};
      Move last_move_;
      TaskHandle_t task_ = nullptr;
      pcnt_unit_handle_t pcnt_ = nullptr;
      gpio_glitch_filter_handle_t stopper_filter_ = nullptr;
      ledc_mode_t ledc_mode_ = LEDC_LOW_SPEED_MODE;
      ledc_timer_t ledc_timer_ = LEDC_TIMER_0;
      ledc_channel_t ledc_channel_ = LEDC_CHANNEL_0;
      std::vector<SegmentData> segments_;
      uint16_t current_segment_index_ = 0;
      esp_err_t setupDirection(int32_t steps);
      esp_err_t initPCNT();
      esp_err_t initDriverGPIO();
      esp_err_t initLEDC();
      esp_err_t initStopperGPIO();
      esp_err_t setupPCNT(int target_steps);
      esp_err_t setupLEDC(uint32_t period_us, uint32_t high_us);
      esp_err_t setupStopper();
  };

} // namespace motor
