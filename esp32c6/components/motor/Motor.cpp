#include <esp_check.h>

#include "Common.hpp"
#include "Motor.hpp"
#include "MotorHal.hpp"
#include "esp_err.h"
#include "esp_log.h"

namespace motor {

  const char *moveTypeToName(MoveType move) {
    switch (move) {
      case MoveType::FREE:
        return "FREE";
      case MoveType::FIXED:
        return "FIXED";
      case MoveType::STOP:
        return "STOP";
      case MoveType::HOLD:
        return "HOLD";
      case MoveType::RELEASE:
        return "RELEASE";
    }
    return NULL;
  };

  MotorState Motor::getState() { return motor_state_.load(std::memory_order_acquire); }

  esp_err_t Motor::init() {
    if (initialized_) {
      ESP_LOGW(TAG, "Already initialized");
      return ESP_OK;
    }
    MotorCfg motor_cfg {
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

  esp_err_t Motor::resetQueue() {
    if (!cmd_q_) {
      return ESP_ERR_INVALID_STATE;
    }
    return xQueueReset(cmd_q_);
  }

  esp_err_t Motor::submit(const Move &mv) {
    bool isIdle = motor_state_.load(std::memory_order_acquire) == MotorState::IDLE;
    if (mv.move_type == MoveType::HOLD) {
      return isIdle ? hal_->holdOrRelease(true) : ESP_ERR_INVALID_STATE;
    }
    if (mv.move_type == MoveType::RELEASE) {
      return isIdle ? hal_->holdOrRelease(false) : ESP_ERR_INVALID_STATE;
    }
    if (!cmd_q_) {
      return ESP_ERR_INVALID_STATE;
    }

    if (mv.move_type == MoveType::STOP) {
      xQueueReset(cmd_q_);
      if (motor_state_.load(std::memory_order_acquire) == MotorState::STARTED) {
        stop_requested_ = true;
        xTaskNotifyGive(task_);
      }
      return ESP_OK;
    }
    QueuedCmd q {mv, next_id_++};
    if (xQueueSend(cmd_q_, &q, 0) != pdTRUE) {
      return ESP_ERR_NO_MEM;
    }
    if (mv.move_type == MoveType::FREE) {
      int dir = mv.degrees > 0 ? +1 : -1;
      submit(
        Move {
          .degrees = -dir * 180,
          .end_action = EndAction::HOLD,
          .move_type = MoveType::FIXED,
        }
      );
    }
    stop_requested_ = false;
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
      ESP_LOGI(TAG, "Processing %s move type", moveTypeToName(c.mv.move_type));
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
      // -- wait for the end of current move
      motor_state_.store(MotorState::STARTED, std::memory_order_release);
      for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if ( //
          stop_requested_ || 
          c.mv.move_type != MoveType::FIXED || 
          hal_->nextSegment()
        ) {
          break;
        }
      }
      ESP_LOGW(TAG, "Done");
      if (hal_->stopMove() != ESP_OK) {
        motor_state_.store(MotorState::ERRORED, std::memory_order_release);
      };
    }
  }
} // namespace motor
