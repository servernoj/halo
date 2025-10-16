#include <esp_app_format.h>
#include <esp_check.h>
#include <esp_log.h>

#include "Common.hpp"
#include "I2C.hpp"
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
            switch (lastAddr) {
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
                ota::OTA::instance().trigger_ota_update();
                break;
              }
              case I2C::REG_OTA_RESET_STATUS: {
                // Write: Reset OTA status
                ESP_LOGI(TAG, "Reset OTA status");
                ota::OTA::instance().reset_status();
                break;
              }
              case I2C::REG_MOTOR_STOP: {
                // Write: Stop motor
                ESP_LOGI(TAG, "Motor stop");
                Motor::instance().submit(Move {.end_action = EndAction::COAST, .move_type = MoveType::STOP});
                break;
              }
              case I2C::REG_MOTOR_FREE_RUN: {
                // Write: Free run motor (1 byte: direction)
                int8_t dir = -1;
                if (evt.data->length > 1) {
                  dir = *(int8_t *)(evt.data->buffer + 1) > 0 ? +1 : -1;
                }
                ESP_LOGI(TAG, "Motor free run, dir=%d", dir);
                Motor::instance().submit(
                  Move {
                    .steps = dir,
                    .end_action = EndAction::HOLD,
                    .move_type = MoveType::FREE,
                  }
                );
                break;
              }
              case I2C::REG_MOTOR_PROFILE: {
                // Write: Motor profile (array of {int16 steps, uint16 delay})
                size_t dataLength = evt.data->length - 1;
                if (dataLength % 4 == 0 && dataLength <= 60) {
                  int N = dataLength / 4;
                  uint16_t *offset = (uint16_t *)(evt.data->buffer + 1);
                  ESP_LOGI(TAG, "Motor profile: %d moves", N);
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
          } else {
            isAddressSet = false;
          }
        } else if (evt.type == I2C_SLAVE_EVT_TX) {
          uint8_t tx[BUF_SIZE];
          memset(tx, 0, BUF_SIZE);
          size_t tx_len = 1;

          if (isAddressSet) {
            ESP_LOGI(I2C::TAG, "Read from register 0x%02X", lastAddr);

            switch (lastAddr) {
              case I2C::REG_DEVICE_ID: {
                tx[0] = I2C::DEVICE_ID;
                tx_len = 1;
                break;
              }
              case I2C::REG_OTA_URL: {
                // Read: Get OTA URL
                size_t buf_len = BUF_SIZE;
                ota::OTA::instance().get_ota_url((char *)tx, &buf_len);
                tx_len = buf_len;
                ESP_LOGI(TAG, "Read OTA URL: %s", tx);
                break;
              }
              case I2C::REG_OTA_STATE: {
                const auto &status = ota::OTA::instance().get_status();
                tx[0] = static_cast<uint8_t>(status.state);
                tx_len = 1;
                break;
              }
              case I2C::REG_OTA_PROGRESS: {
                const auto &status = ota::OTA::instance().get_status();
                tx[0] = status.progress;
                tx_len = 1;
                break;
              }
              case I2C::REG_OTA_ERROR_CODE: {
                const auto &status = ota::OTA::instance().get_status();
                memcpy(tx, &status.error_code, 4); // Little Endian (int32_t)
                tx_len = 4;
                break;
              }
              case I2C::REG_OTA_BYTES_DOWNLOADED: {
                const auto &status = ota::OTA::instance().get_status();
                memcpy(tx, &status.bytes_downloaded, 4); // Little Endian (uint32_t)
                tx_len = 4;
                break;
              }
              case I2C::REG_OTA_ERROR_FUNCTION: {
                const auto &status = ota::OTA::instance().get_status();
                strncpy((char *)tx, status.error_function, 32);
                tx[31] = '\0';
                tx_len = strnlen((char *)tx, 32) + 1; // Include null terminator
                break;
              }
              case I2C::REG_MOTOR_STATE: {
                tx[0] = static_cast<uint8_t>(Motor::instance().getState());
                tx_len = 1;
                break;
              }
              case I2C::REG_FIRMWARE_VERSION: {
                const esp_app_desc_t *app_desc = esp_app_get_description();
                strncpy((char *)tx, app_desc->version, BUF_SIZE - 1);
                tx[BUF_SIZE - 1] = '\0';
                tx_len = strnlen((char *)tx, BUF_SIZE) + 1; // Include null terminator
                ESP_LOGI(TAG, "Firmware version: %s", tx);
                break;
              }
              case I2C::REG_FIRMWARE_BUILD_DATE: {
                const esp_app_desc_t *app_desc = esp_app_get_description();
                strncpy((char *)tx, app_desc->date, BUF_SIZE - 1);
                tx[BUF_SIZE - 1] = '\0';
                tx_len = strnlen((char *)tx, BUF_SIZE) + 1;
                break;
              }
              case I2C::REG_FIRMWARE_BUILD_TIME: {
                const esp_app_desc_t *app_desc = esp_app_get_description();
                strncpy((char *)tx, app_desc->time, BUF_SIZE - 1);
                tx[BUF_SIZE - 1] = '\0';
                tx_len = strnlen((char *)tx, BUF_SIZE) + 1;
                break;
              }
              default: {
                tx[0] = 0xFF; // Invalid register
                tx_len = 1;
                ESP_LOGW(TAG, "Unknown read register: 0x%02X", lastAddr);
                break;
              }
            }
          } else {
            tx[0] = 0xFF;
            tx_len = 1;
          }

          ESP_ERROR_CHECK(i2c_slave_write(i2c_slave_handle_, tx, tx_len, &bytesWritten, 0));
          isAddressSet = false;
        }
      }
    }
    vTaskDelete(NULL);
  }
} // namespace i2c_slave