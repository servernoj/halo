#include <cstdlib>
#include <driver/gpio.h>
#include <driver/pulse_cnt.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>

#include "MotorHal.hpp"
#include "RampedMove.hpp"
#include "hal/ledc_types.h"

namespace motor {

  static bool IRAM_ATTR
  pcnt_on_reach_cb(pcnt_unit_handle_t pcnt, const pcnt_watch_event_data_t *ed, void *ctx) {
    MotorHal *self = reinterpret_cast<MotorHal *>(ctx);
    self->onStopISR();
    return false;
  }

  static void IRAM_ATTR gpio_stopper_cb(void *ctx) {
    MotorHal *self = reinterpret_cast<MotorHal *>(ctx);
    self->onStopISR();
  }

  MotorHal::MotorHal(MotorCfg &motorConfig) : motor_cfg_(motorConfig) {};

  esp_err_t MotorHal::init() {
    ESP_RETURN_ON_ERROR(initGPIO(), MotorHal::TAG, "GPIO initialization failed");
    ESP_RETURN_ON_ERROR(initStopper(), MotorHal::TAG, "Stopper initialization failed");
    ESP_RETURN_ON_ERROR(initPCNT(), MotorHal::TAG, "PCNT initialization failed");
    return ESP_OK;
  }

  esp_err_t MotorHal::initStopper() {
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

  esp_err_t MotorHal::initGPIO() {
    auto &pins = motor_cfg_.pins;

    // Configure STBY, EN, M0, M1, M2, M3 as pure outputs
    // M2 will be reconfigured to INPUT_OUTPUT after mode setup
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

    return ESP_OK;
  }

  esp_err_t MotorHal::initPCNT() {
    // -- Unit
    pcnt_unit_config_t pcnt_unit_cfg = {};
    pcnt_unit_cfg.low_limit = -1;
    pcnt_unit_cfg.high_limit = INT16_MAX;
    ESP_RETURN_ON_ERROR(pcnt_new_unit(&pcnt_unit_cfg, &pcnt_unit_), MotorHal::TAG, "pcnt_new_unit failed");
    // -- Channel
    pcnt_chan_config_t pcnt_chan_cfg = {};
    pcnt_chan_cfg.edge_gpio_num = motor_cfg_.pins.m2;
    pcnt_chan_cfg.level_gpio_num = -1;
    ESP_RETURN_ON_ERROR(
      pcnt_new_channel(pcnt_unit_, &pcnt_chan_cfg, &pcnt_channel_), //
      MotorHal::TAG, //
      "pcnt_new_channel failed"
    );
    ESP_RETURN_ON_ERROR(
      pcnt_channel_set_edge_action(
        pcnt_channel_, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD
      ), //
      MotorHal::TAG, //
      "pcnt_channel_set_edge_action"
    );
    pcnt_event_callbacks_t cbs {.on_reach = pcnt_on_reach_cb};
    ESP_RETURN_ON_ERROR(
      pcnt_unit_register_event_callbacks(pcnt_unit_, &cbs, this), //
      MotorHal::TAG, //
      "pcnt_unit_register_event_callbacks failed"
    );
    ESP_RETURN_ON_ERROR(
      pcnt_unit_enable(pcnt_unit_), //
      MotorHal::TAG, //
      "pcnt_unit_enable failed"
    );
    return ESP_OK;
  }

  esp_err_t MotorHal::setupDirection(int32_t degrees) {
    uint8_t direction = (degrees >= 0) ? 1 : 0;
    ESP_RETURN_ON_ERROR(
      gpio_set_level(motor_cfg_.pins.m3, direction), //
      MotorHal::TAG, //
      "gpio_set_level"
    );
    return ESP_OK;
  }

  esp_err_t MotorHal::setupPCNT(int steps) {
    ESP_RETURN_ON_ERROR(
      pcnt_unit_clear_count(pcnt_unit_), //
      MotorHal::TAG, //
      "pcnt_unit_clear_count failed"
    );
    ESP_RETURN_ON_ERROR(
      pcnt_unit_add_watch_point(pcnt_unit_, std::abs(steps)), //
      MotorHal::TAG, //
      "pcnt_unit_add_watch_point failed"
    );
    ESP_RETURN_ON_ERROR(
      pcnt_unit_start(pcnt_unit_), //
      MotorHal::TAG, //
      "pcnt_unit_start failed"
    );
    return ESP_OK;
  }

  esp_err_t MotorHal::setupLEDC(uint32_t period_us) {
    if (period_us == 0) {
      return ESP_ERR_INVALID_ARG;
    }

    const uint32_t freq_hz = 1000000UL / period_us;

    // Configure LEDC timer
    ledc_timer_config_t ledc_timer_cfg = {};
    ledc_timer_cfg.speed_mode = ledc_mode_;
    ledc_timer_cfg.duty_resolution = LEDC_TIMER_14_BIT;
    ledc_timer_cfg.timer_num = ledc_timer_;
    ledc_timer_cfg.freq_hz = freq_hz;
    ledc_timer_cfg.clk_cfg = LEDC_AUTO_CLK;

    ESP_RETURN_ON_ERROR(
      ledc_timer_config(&ledc_timer_cfg), //
      MotorHal::TAG, //
      "LEDC timer config failed"
    );

    // Configure LEDC channel on M2 (STEP pin)
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
      "LEDC channel config failed"
    );

    // Set duty cycle to 12.5%
    const uint32_t duty = 1U << (ledc_timer_cfg.duty_resolution - 3);
    ESP_ERROR_CHECK(ledc_set_duty(ledc_mode_, ledc_channel_, duty));
    ESP_ERROR_CHECK(ledc_update_duty(ledc_mode_, ledc_channel_));

    return ESP_OK;
  }

  esp_err_t MotorHal::setupMode() {
    auto &pins = motor_cfg_.pins;

    // Reconfigure M2 to pure output mode for mode configuration
    gpio_config_t m2_io = {
      .pin_bit_mask = (1ULL << pins.m2),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE
    };
    ESP_RETURN_ON_ERROR(gpio_config(&m2_io), MotorHal::TAG, "M2 reconfigure to output failed");

    // Explicitly drive M2 LOW to ensure clean state
    gpio_set_level(pins.m2, 0);

    // Put driver in standby with outputs disabled
    gpio_set_level(pins.stby, 0);
    gpio_set_level(pins.en, 0);
    esp_rom_delay_us(200);

    // Prepare MODE pins (Fixed mode) BEFORE releasing STBY
    const uint8_t stepModeBits = static_cast<uint8_t>(motor_cfg_.stepMode.getModeBits());
    gpio_set_level(pins.m0, (stepModeBits >> 0) & 1);
    gpio_set_level(pins.m1, (stepModeBits >> 1) & 1);
    gpio_set_level(pins.m2, (stepModeBits >> 2) & 1);
    gpio_set_level(pins.m3, (stepModeBits >> 3) & 1);

    // Delay before STBY edge
    esp_rom_delay_us(200);

    // Release Standby => latches control mode per MODE[3:0]
    gpio_set_level(pins.stby, 1);

    // Delay after STBY edge
    esp_rom_delay_us(500);

    // Now reconfigure M2 for dual LEDC output + PCNT input use
    m2_io.mode = GPIO_MODE_INPUT_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&m2_io), MotorHal::TAG, "M2 reconfigure for LEDC/PCNT failed");

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

    // 1. Setup step mode (configures M3:M0 and STBY sequence)
    ESP_RETURN_ON_ERROR(
      setupMode(), //
      MotorHal::TAG, //
      "setupMode failed"
    );

    uint16_t factor = motor_cfg_.stepMode.getFactor();

    // 2. Setup direction (M3 is now used as DIR after mode is latched)
    ESP_RETURN_ON_ERROR(
      setupDirection(mv.degrees), //
      MotorHal::TAG, //
      "setupDirection failed"
    );

    if (mv.move_type == MoveType::FIXED) {
      // Generate motion profile segments
      segments_ = RampedMove::generateSegments(mv.degrees, factor);
      // ESP_LOGI(TAG, "Segments: ");
      // for (SegmentData s : segments_) {
      //   ESP_LOGI(TAG, "{ .steps = %d, .period_us = %u }", s.steps, s.period_us);
      // }

      current_segment_index_ = 0;
      auto current_segment = segments_.at(current_segment_index_);

      // 3. Setup LEDC first (starts generating pulses on M2)
      ESP_RETURN_ON_ERROR(
        setupLEDC(current_segment.period_us),
        MotorHal::TAG, //
        "setupLEDC failed"
      );

      // 4. Setup PCNT (monitors pulses on M2)
      ESP_RETURN_ON_ERROR(
        setupPCNT(current_segment.steps),
        MotorHal::TAG, //
        "setupPCNT failed"
      );
    }

    if (mv.move_type == MoveType::FREE) {
      // Setup stopper interrupt
      ESP_RETURN_ON_ERROR(
        setupStopper(), //
        MotorHal::TAG, //
        "setupStopper failed"
      );

      // Setup LEDC with initial period
      ESP_RETURN_ON_ERROR(
        setupLEDC(BASE_PERIOD_US), //
        MotorHal::TAG, //
        "setupLEDC failed"
      );
    }

    // 5. Enable motor outputs
    gpio_set_level(motor_cfg_.pins.en, 1);

    return ESP_OK;
  }

  esp_err_t MotorHal::holdOrRelease(bool doHold) {
    ESP_RETURN_ON_ERROR(
      gpio_set_level(
        motor_cfg_.pins.en, //
        uint32_t(doHold)
      ),
      MotorHal::TAG, "gpio_set_level failed"
    );
    return ESP_OK;
  }

  esp_err_t MotorHal::stopMove() {
    // 1. Disable motor outputs first (stop motion immediately)
    if (last_move_.end_action == EndAction::COAST) {
      ESP_ERROR_CHECK(gpio_set_level(motor_cfg_.pins.en, 0));
    }
    // If HOLD, keep EN high to maintain holding torque

    // 2. Stop PCNT first (stop counting before stopping pulse generation)
    if (last_move_.move_type == MoveType::FIXED) {
      ESP_ERROR_CHECK(pcnt_unit_stop(pcnt_unit_));
      // Remove the watch point for the last segment
      ESP_ERROR_CHECK(pcnt_unit_remove_watch_point(pcnt_unit_, std::abs(segments_.back().steps)));
    }

    // 3. Stop and deconfigure LEDC (stop pulse generation)
    ESP_ERROR_CHECK(ledc_stop(ledc_mode_, ledc_channel_, 0));
    ESP_ERROR_CHECK(ledc_timer_pause(ledc_mode_, ledc_timer_));
    ESP_ERROR_CHECK(ledc_set_duty(ledc_mode_, ledc_channel_, 0));
    ESP_ERROR_CHECK(ledc_update_duty(ledc_mode_, ledc_channel_));

    ledc_channel_config_t ledc_chan_cfg = {};
    ledc_chan_cfg.deconfigure = true;
    ESP_RETURN_ON_ERROR(
      ledc_channel_config(&ledc_chan_cfg), //
      MotorHal::TAG, //
      "LEDC channel deconfig failed"
    );

    ledc_timer_config_t ledc_timer_cfg = {};
    ledc_timer_cfg.deconfigure = true;
    ledc_timer_cfg.timer_num = ledc_timer_;
    ledc_timer_cfg.speed_mode = ledc_mode_;
    ESP_RETURN_ON_ERROR(
      ledc_timer_config(&ledc_timer_cfg), //
      MotorHal::TAG, //
      "LEDC timer deconfig failed"
    );

    // 4. Cleanup move-specific resources
    if (last_move_.move_type == MoveType::FREE) {
      ESP_ERROR_CHECK(gpio_intr_disable(motor_cfg_.pins.stop));
      ESP_ERROR_CHECK(gpio_isr_handler_remove(motor_cfg_.pins.stop));
    }

    return ESP_OK;
  }

  /**
   * @brief Transition to the next segment in a ramped move
   * @return true if move is complete (no more segments), false if continuing
   */
  bool MotorHal::nextSegment() {
    if (current_segment_index_ < (segments_.size() - 1)) {
      auto finished_segment = segments_.at(current_segment_index_++);
      auto current_segment = segments_.at(current_segment_index_);

      // 1. Pause pulse generation
      ledc_timer_pause(ledc_mode_, ledc_timer_);

      // 2. Reconfigure pulse counter for next segment
      pcnt_unit_stop(pcnt_unit_);
      pcnt_unit_clear_count(pcnt_unit_);
      pcnt_unit_remove_watch_point(pcnt_unit_, std::abs(finished_segment.steps));
      pcnt_unit_add_watch_point(pcnt_unit_, std::abs(current_segment.steps));
      pcnt_unit_start(pcnt_unit_);

      // 3. Update pulse frequency and resume generation
      ledc_set_freq(ledc_mode_, ledc_timer_, 1000000 / current_segment.period_us);
      ledc_timer_resume(ledc_mode_, ledc_timer_);
      return false; // More segments remain
    }

    ESP_LOGI(TAG, "Move complete");
    return true; // All segments done
  }

  void IRAM_ATTR MotorHal::onStopISR() {
    BaseType_t hp = pdFALSE;
    if (task_) {
      vTaskNotifyGiveFromISR(task_, &hp);
      portYIELD_FROM_ISR(hp);
    }
  }

} // namespace motor
