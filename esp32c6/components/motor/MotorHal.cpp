#include <algorithm>
#include <cmath>
#include <esp_check.h>
#include <esp_err.h>
#include <numeric>

#include "MotorHal.hpp"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "hal/gpio_types.h"
#include "motor_profiles.h"

namespace motor {

  uint16_t getMicrostepFactor(StepMode mode) {
    switch (mode) {
      case StepMode::FixedFull:
        return 1;
      case StepMode::FixedHalf:
        return 2;
      case StepMode::FixedQuarter:
        return 4;
      case StepMode::FixedEighth:
        return 8;
      case StepMode::FixedSixteenth:
        return 16;
      case StepMode::FixedThirtySecond:
        return 32;
      case StepMode::FixedSixtyFourth:
        return 64;
      case StepMode::FixedOneTwentyEighth:
        return 128;
      default:
        return 1;
    }
  }

  static bool IRAM_ATTR
  pcnt_on_reach_cb(pcnt_unit_handle_t pcnt_, const pcnt_watch_event_data_t *ed, void *ctx) {
    MotorHal *self = reinterpret_cast<MotorHal *>(ctx);
    self->onStopISR();
    return false;
  }

  static void IRAM_ATTR gpio_stopper_cb(void *ctx) {
    MotorHal *self = reinterpret_cast<MotorHal *>(ctx);
    self->onStopISR();
  }

  esp_err_t MotorHal::init(MotorCfg &motor_cfg) {
    motor_cfg_ = motor_cfg;
    ESP_RETURN_ON_ERROR(initDriverGPIO(), MotorHal::TAG, "Driver GPIO initialization failed");
    ESP_RETURN_ON_ERROR(initStopperGPIO(), MotorHal::TAG, "Stopper GPIO initialization failed");
    ESP_RETURN_ON_ERROR(initPCNT(), MotorHal::TAG, "PCNT initialization failed");
    return ESP_OK;
  }

  esp_err_t MotorHal::initStopperGPIO() {
    // -- GPIO config
    gpio_config_t io = {
      .pin_bit_mask = 1ULL << motor_cfg_.pins.stop,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_NEGEDGE
    };
    ESP_RETURN_ON_ERROR(
      gpio_config(&io), //
      MotorHal::TAG, //
      "gpio_config failed" //
    );
    // De-glitcher
    gpio_pin_glitch_filter_config_t stopper_filter_cfg = {
      .clk_src = GLITCH_FILTER_CLK_SRC_DEFAULT,
      .gpio_num = motor_cfg_.pins.stop,
    };
    ESP_RETURN_ON_ERROR(
      gpio_new_pin_glitch_filter(&stopper_filter_cfg, &stopper_filter_),
      MotorHal::TAG, //
      "gpio_new_glitch_filter failed" //
    );
    ESP_RETURN_ON_ERROR(
      gpio_glitch_filter_enable(stopper_filter_),
      MotorHal::TAG, //
      "gpio_glitch_filter_enable failed" //
    );
    // -- ISR
    ESP_RETURN_ON_ERROR(
      gpio_install_isr_service(ESP_INTR_FLAG_IRAM), //
      MotorHal::TAG, //
      "gpio_install_isr_service failed" //
    );
    return ESP_OK;
  }

  esp_err_t MotorHal::initDriverGPIO() {

    motor_pins_t &pins = motor_cfg_.pins;

    gpio_config_t io = {
      .pin_bit_mask = (1ULL << pins.stby) | (1ULL << pins.en) | (1ULL << pins.m0) | (1ULL << pins.m1)
                      | (1ULL << pins.m2) | (1ULL << pins.m3),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE
    };

    ESP_RETURN_ON_ERROR(
      gpio_config(&io), //
      MotorHal::TAG, //
      "gpio_config failed" //
    );

    // HOLD in Standby and outputs disabled
    gpio_set_level(pins.stby, 0);
    gpio_set_level(pins.en, !motor_cfg_.en_active_level);
    // Prepare MODE pins (Fixed mode) BEFORE releasing STBY
    const uint8_t stepModeBits = static_cast<uint8_t>(motor_cfg_.step_mode);
    gpio_set_level(pins.m0, (stepModeBits >> 0) & 1);
    gpio_set_level(pins.m1, (stepModeBits >> 1) & 1);
    gpio_set_level(pins.m2, (stepModeBits >> 2) & 1);
    gpio_set_level(pins.m3, (stepModeBits >> 3) & 1);
    // Delay before STBY edge
    esp_rom_delay_us(100);
    // Release Standby => latches control mode per MODE[3:0]
    gpio_set_level(pins.stby, 1);
    // Delay after STBY edge
    esp_rom_delay_us(200);
    // After STBY, MODE3 pin is CW/CCW; now it's safe to set a default dir
    gpio_set_level(pins.m3, motor_cfg_.dir_cw_level);
    return ESP_OK;
  }

  esp_err_t MotorHal::initPCNT() {
    // -- Unit
    pcnt_unit_config_t pcnt_unit_cfg = {};
    pcnt_unit_cfg.low_limit = -1;
    pcnt_unit_cfg.high_limit = INT16_MAX;
    ESP_RETURN_ON_ERROR(pcnt_new_unit(&pcnt_unit_cfg, &pcnt_), MotorHal::TAG, "pcnt_new_unit failed");
    // -- Channel
    pcnt_channel_handle_t ch;
    pcnt_chan_config_t pcnt_chan_cfg = {};
    pcnt_chan_cfg.edge_gpio_num = motor_cfg_.pins.m2;
    pcnt_chan_cfg.level_gpio_num = -1;
    ESP_RETURN_ON_ERROR(
      pcnt_new_channel(pcnt_, &pcnt_chan_cfg, &ch), //
      MotorHal::TAG, //
      "pcnt_new_channel failed"
    );
    ESP_RETURN_ON_ERROR(
      pcnt_channel_set_edge_action(ch, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD), //
      MotorHal::TAG, //
      "pcnt_channel_set_edge_action"
    );
    pcnt_event_callbacks_t cbs {.on_reach = pcnt_on_reach_cb};
    ESP_RETURN_ON_ERROR(
      pcnt_unit_register_event_callbacks(pcnt_, &cbs, this), //
      MotorHal::TAG, //
      "pcnt_unit_register_event_callbacks failed"
    );
    return ESP_OK;
  }

  esp_err_t MotorHal::setupDirection(int32_t degrees) {
    uint8_t direction = (degrees >= 0) ? motor_cfg_.dir_cw_level : !motor_cfg_.dir_cw_level;
    ESP_RETURN_ON_ERROR(
      gpio_set_level(motor_cfg_.pins.m3, direction), //
      MotorHal::TAG, //
      "gpio_set_level"
    );
    return ESP_OK;
  }

  esp_err_t MotorHal::setupPCNT(int steps) {
    int target = (steps >= 0) ? (int)steps : (int)(-steps);
    ESP_RETURN_ON_ERROR(
      pcnt_unit_clear_count(pcnt_), //
      MotorHal::TAG, //
      "pcnt_unit_clear_count failed"
    );
    ESP_RETURN_ON_ERROR(
      pcnt_unit_add_watch_point(pcnt_, target), //
      MotorHal::TAG, //
      "pcnt_unit_add_watch_point failed"
    );
    ESP_RETURN_ON_ERROR(
      pcnt_unit_enable(pcnt_), //
      MotorHal::TAG, //
      "pcnt_unit_enable failed"
    );
    ESP_RETURN_ON_ERROR(
      pcnt_unit_start(pcnt_), //
      MotorHal::TAG, //
      "pcnt_unit_start failed"
    );
    return ESP_OK;
  }

  esp_err_t MotorHal::setupLEDC(uint32_t period_us, uint32_t high_us) {
    if (period_us == 0 || high_us == 0 || high_us >= period_us) {
      return ESP_ERR_INVALID_ARG;
    }
    const uint32_t freq_hz = 1000000UL / period_us;
    // -- Timer
    ledc_timer_config_t ledc_timer_cfg = {};
    ledc_timer_cfg.speed_mode = ledc_mode_;
    ledc_timer_cfg.duty_resolution = LEDC_TIMER_14_BIT;
    ledc_timer_cfg.timer_num = ledc_timer_;
    ledc_timer_cfg.freq_hz = freq_hz;
    ledc_timer_cfg.clk_cfg = LEDC_AUTO_CLK;
    ESP_RETURN_ON_ERROR(
      ledc_timer_config(&ledc_timer_cfg), //
      MotorHal::TAG, //
      "timer cfg failed"
    );
    // -- Channel
    ledc_channel_config_t ledc_chan_cfg = {};
    ledc_chan_cfg.gpio_num = motor_cfg_.pins.m2;
    ledc_chan_cfg.speed_mode = ledc_mode_;
    ledc_chan_cfg.channel = ledc_channel_;
    ledc_chan_cfg.timer_sel = ledc_timer_;
    ledc_chan_cfg.duty = 0;
    ledc_chan_cfg.hpoint = 0;
    ESP_RETURN_ON_ERROR(
      ledc_channel_config(&ledc_chan_cfg), //
      MotorHal::TAG, //
      "ledc_channel_config failed"
    );
    // -- Duty
    const uint32_t period_ticks = 1U << 14;
    uint32_t duty_ticks = (uint64_t)high_us * period_ticks / period_us;
    if (duty_ticks == 0) {
      duty_ticks = 1;
    }
    if (duty_ticks >= period_ticks) {
      duty_ticks = period_ticks - 1;
    }

    ESP_ERROR_CHECK(ledc_set_duty(ledc_mode_, ledc_channel_, duty_ticks));
    ESP_ERROR_CHECK(ledc_update_duty(ledc_mode_, ledc_channel_));
    return ESP_OK;
  }

  esp_err_t MotorHal::setupStopper() {
    ESP_RETURN_ON_ERROR(
      gpio_isr_handler_add(motor_cfg_.pins.stop, gpio_stopper_cb, this), //
      MotorHal::TAG, //
      "gpio_isr_handler_add failed" //
    );
    ESP_RETURN_ON_ERROR(
      gpio_intr_enable(motor_cfg_.pins.stop), //
      MotorHal::TAG, //
      "gpio_intr_enable failed" //
    );
    return ESP_OK;
  }

  esp_err_t MotorHal::startMove(Move &mv, MotorCmdId) {
    last_move_ = mv;
    uint16_t microstep_factor = getMicrostepFactor(motor_cfg_.step_mode);
    ESP_RETURN_ON_ERROR(
      setupDirection(mv.degrees), //
      MotorHal::TAG, //
      "setupDirection failed"
    );

    if (mv.move_type == MoveType::FIXED) {
      // Target & covered times
      uint32_t total_steps = mv.degrees * STEPS_PER_REVOLUTION * microstep_factor / 360;
      uint64_t total_time_us = mv.degrees * 1000000 / mv.rpm / 6;
      ESP_LOGI(TAG, "Totals: steps = %d, time_us = %d", total_steps, total_time_us);
      // Profile
      auto &raw_profile = PROFILE_LINEAR;
      std::vector<float> profile;
      profile.reserve(raw_profile.size());
      float scale = raw_profile.front();
      std::transform(
        raw_profile.begin(), //
        raw_profile.end(), //
        std::back_inserter(profile), //
        [scale](float x) { return x / scale; }
      );
      if (!segments_.capacity()) {
        segments_.reserve(profile.size());
      }
      uint32_t time_segment = total_time_us / profile.size();
      ESP_LOGI(TAG, "Segment time_us: %d", time_segment);
      ESP_LOGI(TAG, "Microstep factor: %d", microstep_factor);
      float S0_f = (float)time_segment * microstep_factor / START_PERIOD_US;
      // -- 1st segment is configured with "safe to start" period_us equivalent to START_PERIOD_US
      segments_.clear();
      segments_.push_back(
        SegmentData {
          .steps = static_cast<uint32_t>(std::round(S0_f)),
          .period_us = START_PERIOD_US / microstep_factor
        }
      );
      if (segments_.back().steps >= total_steps) {
        segments_.back().steps = total_steps;
        ESP_LOGW(TAG, "Forced to use a single-segment ramp");
      } else {
        float inv_sum = std::accumulate(
          profile.begin() + 1, //
          profile.end(), //
          0, //
          [](float acc, float x) { return acc + 1 / x; }
        );
        float alpha = (S0_f * inv_sum) / (total_steps - S0_f);
        ESP_LOGI(TAG, "Inverted sum: %f, Alpha: %f", inv_sum, alpha);
        std::transform(
          profile.begin() + 1, profile.end(), std::back_inserter(segments_),
          [microstep_factor, alpha, time_segment](float g_i) {
            uint32_t T_i = static_cast<uint32_t>(
              std::round((float)START_PERIOD_US / microstep_factor * g_i * alpha) //
            );
            return SegmentData {
              .steps = time_segment / T_i, //
              .period_us = T_i
            };
          }
        );
      }
      ESP_LOGI(TAG, "Profile:");
      for (float p : profile) {
        ESP_LOGI(TAG, "%5.2f", p);
      }
      ESP_LOGI(TAG, "Segments:");
      for (SegmentData s : segments_) {
        ESP_LOGI(TAG, "{ .period_us = %d, .steps = %d }", s.period_us, s.steps);
      }

      // auto current_segment = segments_.front();
      // // Set initial LEDC
      // ESP_RETURN_ON_ERROR(
      //   setupLEDC(
      //     current_segment.period_us,
      //     current_segment.period_us / 8 //
      //   ),
      //   MotorHal::TAG, //
      //   "setupLEDC failed"
      // );

      // // Set PCNT for first segment
      // ESP_RETURN_ON_ERROR(
      //   setupPCNT(
      //     current_segment.steps //
      //   ),
      //   MotorHal::TAG, //
      //   "setupPCNT failed"
      // );
    }
    if (mv.move_type == MoveType::FREE) {
      ESP_RETURN_ON_ERROR(
        setupStopper(), //
        MotorHal::TAG, //
        "setupStopper failed"
      );
      // Adjust period based on step_mode: 400us for 1/8, scaled for others
      uint32_t period_us = START_PERIOD_US / microstep_factor;
      uint32_t high_us = period_us / 8;
      ESP_RETURN_ON_ERROR(
        setupLEDC(period_us, high_us), //
        MotorHal::TAG, //
        "ledc setup failed"
      );
    }
    gpio_set_level(motor_cfg_.pins.en, motor_cfg_.en_active_level);
    return ESP_OK;
  }

  esp_err_t MotorHal::holdOrRelease(bool doHold) {
    ESP_RETURN_ON_ERROR(
      gpio_set_level(
        motor_cfg_.pins.en, //
        doHold ? motor_cfg_.en_active_level : !motor_cfg_.en_active_level
      ),
      MotorHal::TAG, "gpio_set_level failed"
    );
    return ESP_OK;
  }

  esp_err_t MotorHal::stopMove() {
    ESP_ERROR_CHECK(ledc_timer_pause(ledc_mode_, ledc_timer_));
    ESP_ERROR_CHECK(ledc_set_duty(ledc_mode_, ledc_channel_, 0));
    ESP_ERROR_CHECK(ledc_update_duty(ledc_mode_, ledc_channel_));
    if (last_move_.end_action == EndAction::COAST) {
      ESP_ERROR_CHECK(gpio_set_level(motor_cfg_.pins.en, !motor_cfg_.en_active_level));
    }
    if (last_move_.move_type == MoveType::FIXED) {
      ESP_ERROR_CHECK(pcnt_unit_stop(pcnt_));
      ESP_ERROR_CHECK(pcnt_unit_disable(pcnt_));
      // Remove the last watch point
      ESP_ERROR_CHECK(pcnt_unit_remove_watch_point(pcnt_, segments_.back().steps));
    }
    if (last_move_.move_type == MoveType::FREE) {
      ESP_ERROR_CHECK(gpio_intr_disable(motor_cfg_.pins.stop));
      ESP_ERROR_CHECK(gpio_isr_handler_remove(motor_cfg_.pins.stop));
    }
    return ESP_OK;
  }

  /**
   * @return isDone
   */
  bool MotorHal::nextSegment() {
    auto finished_segment = segments_.at(current_segment_index_++);
    if (current_segment_index_ < segments_.size()) {
      auto current_segment = segments_.at(current_segment_index_);
      // Set new period for this segment
      ledc_set_freq(
        ledc_mode_, //
        ledc_timer_, //
        1000000 / current_segment.period_us //
      );
      ledc_set_duty(
        ledc_mode_, //
        ledc_channel_, //
        current_segment.period_us / 8 //
      );
      ledc_update_duty(ledc_mode_, ledc_channel_);
      // Set next watch
      pcnt_unit_stop(pcnt_);
      pcnt_unit_remove_watch_point(pcnt_, finished_segment.steps);
      pcnt_unit_add_watch_point(pcnt_, current_segment.steps);
      pcnt_unit_start(pcnt_);
      return false;
    }
    return true;
  }

  void IRAM_ATTR MotorHal::onStopISR() {
    BaseType_t hp = pdFALSE;
    if (task_) {
      vTaskNotifyGiveFromISR(task_, &hp);
      portYIELD_FROM_ISR(hp);
    }
  }

} // namespace motor
