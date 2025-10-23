#include <esp_app_format.h>
#include <esp_check.h>
#include <esp_log.h>

#include "Common.hpp"
#include "I2C.hpp"
#include "esp_err.h"
#include "ota_status.hpp"
#include <Motor.hpp>
#include <WiFi_DPP.hpp>
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
      .addr_bit_len = I2C_ADDR_BIT_LEN_7,
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
    enum class ReadState { WAITING_FOR_ADDR, SENT_LENGTH, SENDING_DATA };

    uint8_t lastAddr = 0;
    uint8_t dataLength = 0;
    uint8_t dataBuffer[BUF_SIZE];
    ReadState readState = ReadState::WAITING_FOR_ADDR;
    uint32_t bytesWritten;

    for (;;) {
      i2c_slave_event_t evt;
      if (xQueueReceive(event_queue_, &evt, 10) == pdTRUE) {
        if (evt.type == I2C_SLAVE_EVT_RX) {
          if (evt.data->length > 0) {
            lastAddr = evt.data->buffer[0];
            readState = ReadState::WAITING_FOR_ADDR; // Reset on new address
            if (evt.data->length > 1) {
              switch (lastAddr) {
                case REG_DEVICE_RESTART: {
                  ESP_LOGI(TAG, "System restart requested");
                  esp_restart();
                  break;
                }
                case I2C::REG_OTA_URL: {
                  // Write: Set OTA URL (data starts at buffer[1])
                  size_t urlLength = evt.data->length - 1;
                  if (urlLength > 0 && urlLength < BUF_SIZE) {
                    ((char *)evt.data->buffer)[evt.data->length] = '\0';
                    ESP_LOGI(TAG, "Set OTA URL: %s", (const char *)(evt.data->buffer + 1));
                    ota::OTA::instance().set_ota_url((char *)evt.data->buffer + 1);
                  } else {
                    ESP_LOGW(TAG, "OTA URL length invalid: %d", urlLength);
                  }
                  break;
                }
                case I2C::REG_OTA_TRIGGER: {
                  // Write: Trigger OTA update
                  ESP_LOGI(TAG, "Trigger OTA update");
                  esp_err_t ret = ota::OTA::instance().trigger_ota_update();
                  if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed: %s", esp_err_to_name(ret));
                  }
                  break;
                }
                case I2C::REG_OTA_RESET_STATUS: {
                  // Write: Reset OTA status
                  ESP_LOGI(TAG, "Reset OTA status");
                  ota::OTA::instance().reset_status();
                  break;
                }
                case I2C::REG_MOTOR_RESET: {
                  // Write: Stop motor
                  ESP_LOGI(TAG, "Motor task queue reset");
                  esp_err_t ret = Motor::instance().resetQueue();
                  if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed: %s", esp_err_to_name(ret));
                  }
                  break;
                }
                case I2C::REG_MOTOR_STOP: {
                  // Write: Stop motor
                  ESP_LOGI(TAG, "Motor stop");
                  esp_err_t ret = Motor::instance().submit(
                    Move {
                      .end_action = EndAction::COAST, //
                      .move_type = MoveType::STOP
                    }
                  );
                  if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed: %s", esp_err_to_name(ret));
                  }
                  break;
                }
                case I2C::REG_MOTOR_HOLD: {
                  // Write: Hold motor
                  ESP_LOGI(TAG, "Motor hold");
                  esp_err_t ret = Motor::instance().submit(
                    Move {
                      .move_type = MoveType::HOLD //
                    }
                  );
                  if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed: %s", esp_err_to_name(ret));
                  }
                  break;
                }
                case I2C::REG_MOTOR_RELEASE: {
                  // Write: Release motor
                  ESP_LOGI(TAG, "Motor release");
                  esp_err_t ret = Motor::instance().submit(
                    Move {
                      .move_type = MoveType::RELEASE //
                    }
                  );
                  if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed: %s", esp_err_to_name(ret));
                  }
                  break;
                }
                case I2C::REG_MOTOR_FREE_RUN: {
                  // Write: Free run motor (1 byte: direction)
                  int8_t dir = -1;
                  if (evt.data->length > 1) {
                    dir = *(int8_t *)(evt.data->buffer + 1) > 0 ? +1 : -1;
                  }
                  ESP_LOGI(TAG, "Motor free run, dir=%d", dir);
                  esp_err_t ret = Motor::instance().submit(
                    Move {
                      .degrees = dir,
                      .end_action = EndAction::HOLD,
                      .move_type = MoveType::FREE,
                    }
                  );
                  if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed: %s", esp_err_to_name(ret));
                  }
                  break;
                }
                case I2C::REG_MOTOR_PROFILE: {
                  // Write: Motor profile (array of {int16 steps, uint16 delay})
                  size_t dataLength = evt.data->length - 1;
                  if (dataLength % 6 == 0 && dataLength <= 60) {
                    int N = dataLength / 6;
                    uint16_t *offset = (uint16_t *)(evt.data->buffer + 1);
                    ESP_LOGI(TAG, "Motor profile: %d moves", N);
                    for (int i = 0; i < N; i++) {
                      int32_t degrees = *(int16_t *)(offset + i * 3);
                      uint32_t delay = *(offset + i * 3 + 1);
                      uint32_t rpm = *(offset + i * 3 + 2);
                      esp_err_t ret = Motor::instance().submit(
                        Move {
                          .degrees = degrees,
                          .rpm = rpm,
                          .delay_ms = delay,
                          .end_action = EndAction::COAST,
                          .move_type = MoveType::FIXED,
                        }
                      );
                      if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed step #%d: %s", i + 1, esp_err_to_name(ret));
                        break;
                      }
                    }
                  } else {
                    ESP_LOGW(TAG, "Invalid motor profile length: %d bytes", dataLength);
                  }
                  break;
                }
                case I2C::REG_WIFI_CREDENTIALS: {
                  // Write: Set WiFi credentials (SSID\0password\0)
                  // Format: [register_addr][ssid]['\0'][password]['\0']
                  size_t dataLength = evt.data->length - 1;
                  if (dataLength > 2 && dataLength < BUF_SIZE) {
                    const char *ssid = (const char *)(evt.data->buffer + 1);
                    size_t ssid_len = strnlen(ssid, dataLength);

                    if (ssid_len < dataLength - 1) {
                      const char *password = ssid + ssid_len + 1;
                      ESP_LOGI(TAG, "Set WiFi credentials: SSID=%s", ssid);
                      wifi::WiFi_DPP::instance().set_credentials(ssid, password);
                    } else {
                      ESP_LOGW(TAG, "WiFi credentials format invalid (missing password)");
                    }
                  } else {
                    ESP_LOGW(TAG, "WiFi credentials length invalid: %d bytes", dataLength);
                  }
                  break;
                }
                default:
                  ESP_LOGW(TAG, "Unknown write register: 0x%02X", lastAddr);
                  break;
              }
            }
          }
        } else if (evt.type == I2C_SLAVE_EVT_TX) {
          if (readState == ReadState::WAITING_FOR_ADDR) {
            // First TX: Send length byte
            ESP_LOGI(I2C::TAG, "Read from register 0x%02X - sending length", lastAddr);
            switch (lastAddr) {
              case I2C::REG_DEVICE_ID: {
                dataLength = 1;
                dataBuffer[0] = I2C::DEVICE_ID;
                break;
              }
              case I2C::REG_OTA_URL: {
                // Read: Get OTA URL
                size_t buf_len = sizeof(dataBuffer);
                esp_err_t err = ota::OTA::instance().get_ota_url((char *)dataBuffer, &buf_len);
                if (err == ESP_OK) {
                  dataLength = (uint8_t)buf_len;
                } else {
                  dataLength = 1;
                  dataBuffer[0] = '\0';
                }
                ESP_LOGI(
                  TAG, "Read OTA URL: len=%d, url=%s (err=%s)", dataLength, dataBuffer, esp_err_to_name(err)
                );
                break;
              }
              case I2C::REG_OTA_STATUS: {
                const auto &status = ota::OTA::instance().get_status();
                dataLength = sizeof(ota::OTAStatus);
                memcpy(dataBuffer, &status, dataLength);
                break;
              }
              case I2C::REG_MOTOR_STATE: {
                dataLength = 1;
                dataBuffer[0] = static_cast<uint8_t>(Motor::instance().getState());
                break;
              }
              case REG_FIRMWARE_INFO: {
                const esp_app_desc_t *app_desc = esp_app_get_description();
                memset(dataBuffer, 0, sizeof(dataBuffer));
                memcpy(dataBuffer, app_desc->version, strnlen(app_desc->version, 32));
                memcpy(dataBuffer + 32, app_desc->date, strnlen(app_desc->date, 16));
                memcpy(dataBuffer + 48, app_desc->time, strnlen(app_desc->time, 16));
                dataLength = 64;
                break;
              }
              default: {
                dataLength = 1;
                dataBuffer[0] = 0xFF; // Invalid register
                ESP_LOGW(TAG, "Unknown read register: 0x%02X", lastAddr);
                break;
              }
            }
            // Send length byte
            ESP_ERROR_CHECK(i2c_slave_write(i2c_slave_handle_, &dataLength, 1, &bytesWritten, 0));
            readState = ReadState::SENT_LENGTH;
          } else if (readState == ReadState::SENT_LENGTH) {
            // Second TX: Send actual data
            ESP_LOGI(I2C::TAG, "Read from register 0x%02X - sending data (len=%d)", lastAddr, dataLength);
            ESP_ERROR_CHECK(i2c_slave_write(i2c_slave_handle_, dataBuffer, dataLength, &bytesWritten, 0));
            readState = ReadState::WAITING_FOR_ADDR; // Reset for next read
            ESP_LOGI(I2C::TAG, "%d bytes written", bytesWritten);
          }
        }
      }
    }
    vTaskDelete(NULL);
  }
} // namespace i2c_slave