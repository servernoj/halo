#pragma once
#include <driver/gpio_filter.h>
#include <driver/ledc.h>
#include <driver/pulse_cnt.h>
#include <vector>

#include "Common.hpp"

namespace motor {

  class MotorHal {
    public:
      esp_err_t init();
      esp_err_t startMove(Move &mv, MotorCmdId id);
      esp_err_t stopMove();
      bool nextSegment();
      esp_err_t holdOrRelease(bool doHold);
      void registerTaskHandle(TaskHandle_t h) { task_ = h; }
      void onStopISR();
      MotorHal(MotorCfg &);

    private:
      static constexpr const char *TAG = "MotorHal";
      MotorCfg &motor_cfg_;
      Move last_move_;
      TaskHandle_t task_ = nullptr;
      pcnt_unit_handle_t pcnt_unit_ = nullptr;
      pcnt_channel_handle_t pcnt_channel_ = nullptr;
      gpio_glitch_filter_handle_t stopper_filter_ = nullptr;
      ledc_mode_t ledc_mode_ = LEDC_LOW_SPEED_MODE;
      ledc_timer_t ledc_timer_ = LEDC_TIMER_0;
      ledc_channel_t ledc_channel_ = LEDC_CHANNEL_0;
      std::vector<SegmentData> segments_;
      uint16_t current_segment_index_ = 0;
      esp_err_t setupDirection(int32_t degrees);
      esp_err_t setupPCNT(int target_steps);
      esp_err_t setupLEDC(uint32_t period_us);
      esp_err_t setupStopper();
      esp_err_t setupMode();
      esp_err_t initGPIO();
      esp_err_t initPCNT();
      esp_err_t initStopper();
  };

} // namespace motor
