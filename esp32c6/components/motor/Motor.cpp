#include <esp_check.h>

#include "Motor.hpp"
#include "MotorHal.hpp"
#include "esp_log.h"

namespace motor {

  MotorState Motor::getState() { return motor_state_.load(std::memory_order_acquire); }

  esp_err_t Motor::init() {
    if (initialized_) {
      ESP_LOGW(TAG, "Already initialized");
      return ESP_OK;
    }
    MotorCfg motor_cfg {
      .en_active_level = 1,
      .step_mode = StepMode::FixedEighth,
      .dir_cw_level = 1,
      .pins = {
        .stby = GPIO_NUM_5,
        .en = GPIO_NUM_4,
        .m0 = GPIO_NUM_19,
        .m1 = GPIO_NUM_18,
        .m2 = GPIO_NUM_17,
        .m3 = GPIO_NUM_16,
        .stop = GPIO_NUM_3
      }
    };
    if (!cmd_q_) {
      cmd_q_ = xQueueCreate(Motor::kQueueDepth, sizeof(QueuedCmd));
      if (!cmd_q_) {
        return ESP_ERR_NO_MEM;
      }
    }
    if (!hal_) {
      hal_ = std::make_unique<MotorHal>();
    }
    ESP_RETURN_ON_ERROR(hal_->init(motor_cfg), Motor::TAG, "hw init failed");

    if (!task_) {
      xTaskCreate(Motor::taskTrampoline, "motor_task", 4096, this, 7, &task_);
      hal_->registerTaskHandle(task_);
    }
    initialized_ = true;
    ESP_LOGI(TAG, "Motor initialized");
    return ESP_OK;
  }

  esp_err_t Motor::submit(const Move &mv) {
    if (!cmd_q_) {
      return ESP_ERR_INVALID_STATE;
    }
    if (mv.period_us == 0 || mv.high_us == 0 || mv.high_us >= mv.period_us) {
      return ESP_ERR_INVALID_ARG;
    }

    if (mv.move_type == MoveType::STOP) {
      xQueueReset(cmd_q_);
      if (motor_state_.load(std::memory_order_acquire) == MotorState::STARTED) {
        xTaskNotifyGive(task_);
      }
      return ESP_OK;
    }
    QueuedCmd q {mv, next_id_++};
    if (xQueueSend(cmd_q_, &q, 0) != pdTRUE) {
      return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
  }

  void Motor::taskTrampoline(void *arg) { static_cast<Motor *>(arg)->taskLoop(); }

  void Motor::taskLoop() {
    for (;;) {
      motor_state_.store(MotorState::IDLE, std::memory_order_release);
      QueuedCmd c;
      // -- waiting for new move submission
      if (xQueueReceive(cmd_q_, &c, portMAX_DELAY) != pdTRUE) {
        continue;
      }
      // -- optional pre-move delay
      if (c.mv.delay_ms) {
        motor_state_.store(MotorState::DELAYED, std::memory_order_release);
        vTaskDelay(pdMS_TO_TICKS(c.mv.delay_ms));
      }
      // -- starting the move by sending details to HAL
      if (hal_->startMove(c.mv, c.id) != ESP_OK) {
        motor_state_.store(MotorState::ERRORED, std::memory_order_release);
        continue;
      }
      // -- clear up possible "stale" notifications
      ulTaskNotifyTake(pdTRUE, 0);
      // -- wait for the doorbell indicating end of current move
      motor_state_.store(MotorState::STARTED, std::memory_order_release);
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

      if (hal_->stopMove() != ESP_OK) {
        motor_state_.store(MotorState::ERRORED, std::memory_order_release);
      };
    }
  }

} // namespace motor
