#include <esp_check.h>
#include <esp_log.h>

#include "I2C.hpp"
#include <Motor.hpp>
#include <ota.hpp>

using namespace motor;

namespace i2c_slave {
  static bool IRAM_ATTR i2c_slave_request_cb(
    i2c_slave_dev_handle_t i2c_slave, //
    const i2c_slave_request_event_data_t *evt_data, //
    void *ctx
  ) {
    I2C *self = reinterpret_cast<I2C *>(ctx);
    i2c_slave_event_t evt = {.type = I2C_SLAVE_EVT_TX, .data = nullptr};
    BaseType_t xTaskWoken = 0;
    xQueueSendFromISR(self->getEventQueue(), &evt, &xTaskWoken);
    return xTaskWoken;
  }
  static bool IRAM_ATTR i2c_slave_receive_cb(
    i2c_slave_dev_handle_t i2c_slave, //
    const i2c_slave_rx_done_event_data_t *evt_data, //
    void *ctx
  ) {
    I2C *self = reinterpret_cast<I2C *>(ctx);
    i2c_slave_event_t evt = {.type = I2C_SLAVE_EVT_RX, .data = evt_data};
    BaseType_t xTaskWoken = 0;
    xQueueSendFromISR(self->getEventQueue(), &evt, &xTaskWoken);
    return xTaskWoken;
  }

  esp_err_t I2C::init() {
    if (initialized_) {
      ESP_LOGW(TAG, "Already initialized");
      return ESP_OK;
    }
    event_queue_ = xQueueCreate(16, sizeof(i2c_slave_event_t));
    if (!event_queue_) {
      return ESP_ERR_NO_MEM;
    }

    i2c_slave_config_t i2c_slv_config = {
      .i2c_port = -1,
      .sda_io_num = GPIO_NUM_6,
      .scl_io_num = GPIO_NUM_7,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .send_buf_depth = I2C::BUF_SIZE,
      .receive_buf_depth = I2C::BUF_SIZE,
      .slave_addr = I2C::DEVICE_ADDR,
      .flags = {.allow_pd = 0, .enable_internal_pullup = false, .broadcast_en = 0}
    };
    ESP_RETURN_ON_ERROR(
      i2c_new_slave_device(&i2c_slv_config, &i2c_slave_handle_), //
      I2C::TAG,
      "i2c_new_slave_device failed" //
    );
    i2c_slave_event_callbacks_t cbs = {
      .on_request = i2c_slave_request_cb, //
      .on_receive = i2c_slave_receive_cb
    };
    i2c_slave_register_event_callbacks(i2c_slave_handle_, &cbs, this);

    if (!task_) {
      xTaskCreate(I2C::taskTrampoline, "i2c_task", 4096, this, 7, &task_);
    }
    initialized_ = true;
    ESP_LOGI(TAG, "I2C slave initialized");
    return ESP_OK;
  }

  void I2C::taskTrampoline(void *arg) {
    auto self = static_cast<I2C *>(arg);
    self->taskLoop();
  }

  void I2C::taskLoop() {
    uint8_t lastAddr;
    bool isAddressSet = false;
    uint32_t bytesWritten;
    for (;;) {
      i2c_slave_event_t evt;
      if (xQueueReceive(event_queue_, &evt, 10) == pdTRUE) {
        if (evt.type == I2C_SLAVE_EVT_RX) {
          if (evt.data->length > 0) {
            lastAddr = evt.data->buffer[0];
            isAddressSet = true;
            if (evt.data->length >= 2) {
              size_t rx_len = strnlen((char *)evt.data->buffer + 2, BUF_SIZE - 2);
              ESP_LOGI(
                I2C::TAG, "Received command [0x%02X]: 0x%02X",
                lastAddr, //
                evt.data->buffer[1] //
              );
              if (rx_len > 0 && rx_len < BUF_SIZE - 2) {
                evt.data->buffer[rx_len + 2] = '\0';
              }
              if (lastAddr == I2C::REG_CONTROL_MOTOR) {
                uint8_t trajectory = evt.data->buffer[1];
                switch (trajectory) {
                  case 0: {
                    // stop
                    Motor::instance().submit(
                      Move {
                        .move_type = MoveType::STOP,
                      }
                    );
                    break;
                  }
                  case 1: {
                    // return
                    Motor::instance().submit(
                      Move {
                        .steps = -1,
                        .move_type = MoveType::FREE,
                      }
                    );
                    break;
                  }
                  case 2: {
                    // define profile
                    if ((evt.data->length - 2) % 4 == 0) {
                      int N = (evt.data->length - 2) / 4;
                      uint16_t *offset = (uint16_t *)(evt.data->buffer + 2);
                      for (int i = 0; i < N; i++) {
                        int32_t steps = *(int16_t *)(offset + i * 2);
                        uint32_t delay = *(offset + i * 2 + 1);
                        Motor::instance().submit(
                          Move {
                            .steps = steps,
                            .delay_ms = delay,
                            .move_type = MoveType::FIXED,
                          }
                        );
                      }
                    }
                    break;
                  }
                }
              } else if (lastAddr == I2C::REG_CONTROL_OTA) {
                uint8_t cmd = evt.data->buffer[1];
                switch (cmd) {
                  case 0: {
                    // Set OTA URL
                    size_t urlLength = evt.data->length - 2;
                    if (urlLength > 0) {
                      ((char *)evt.data->buffer)[evt.data->length] = '\0';
                      ESP_LOGI(TAG, "Set OTA URL: %s", (const char *)(evt.data->buffer + 2));
                      ota::OTA::instance().set_ota_url((char *)evt.data->buffer + 2);
                    }
                    break;
                  }
                  case 1: {
                    // Trigger OTA update
                    ota::OTA::instance().trigger_ota_update();
                    break;
                  }
                }
              }
            }
          } else {
            isAddressSet = false;
          }
        } else if (evt.type == I2C_SLAVE_EVT_TX) {
          uint8_t tx[BUF_SIZE];
          tx[0] = I2C::DEVICE_ID;
          size_t tx_len = 1;
          if (isAddressSet) {
            switch (lastAddr) {
              case I2C::REG_STATUS_MOTOR: {
                tx[0] = static_cast<uint8_t>(Motor::instance().getState());
                break;
              }
              case I2C::REG_STATUS_OTA: {
                tx_len = BUF_SIZE;
                memset(tx, 0, tx_len);
                ota::OTA::instance().get_ota_url((char *)tx, &tx_len);
                ESP_LOGI(TAG, "Stored OTA URL '%s'", tx);
                break;
              }
              default: {
                tx[0] = 0xFF;
                break;
              }
            }
          }
          ESP_ERROR_CHECK(i2c_slave_write(i2c_slave_handle_, tx, tx_len, &bytesWritten, 0));
          isAddressSet = false;
        }
      }
    }
    vTaskDelete(NULL);
  }
} // namespace i2c_slave