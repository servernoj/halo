#pragma once
#include <driver/i2c_slave.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>

namespace i2c_slave {
  using i2c_slave_event_type_t = enum { I2C_SLAVE_EVT_RX, I2C_SLAVE_EVT_TX };
  using i2c_slave_event_t = struct {
      i2c_slave_event_type_t type;
      const i2c_slave_rx_done_event_data_t *data;
  };

  class I2C {
    public:
      static I2C &instance() {
        static I2C inst;
        return inst;
      }

      esp_err_t init();
      QueueHandle_t getEventQueue() { return event_queue_; }

    private:
      static void taskTrampoline(void *arg);
      bool initialized_ = false;
      static constexpr const char *TAG = "I2C";
      static constexpr uint8_t BUF_SIZE = 64;
      static constexpr uint8_t DEVICE_ID = 0xA5;
      static constexpr uint8_t DEVICE_ADDR = 0x42;

      // OTA Registers (0x10 - 0x1F)
      static constexpr uint8_t REG_OTA_URL = 0x10;
      static constexpr uint8_t REG_OTA_TRIGGER = 0x11;
      static constexpr uint8_t REG_OTA_STATUS = 0x12;
      static constexpr uint8_t REG_OTA_RESET_STATUS = 0x13;

      // Motor Registers (0x20 - 0x2F)
      static constexpr uint8_t REG_MOTOR_CONFIG = 0x20;
      static constexpr uint8_t REG_MOTOR_STOP = 0x21;
      static constexpr uint8_t REG_MOTOR_FREE_RUN = 0x22;
      static constexpr uint8_t REG_MOTOR_PROFILE = 0x23;
      static constexpr uint8_t REG_MOTOR_HOLD = 0x24;
      static constexpr uint8_t REG_MOTOR_RELEASE = 0x25;
      static constexpr uint8_t REG_MOTOR_RESET = 0x26;

      // Firmware Info Registers (0x30 - 0x3F)
      static constexpr uint8_t REG_FIRMWARE_INFO = 0x30;

      // WiFi Registers (0x40 - 0x4F)
      static constexpr uint8_t REG_WIFI_CREDENTIALS = 0x40;

      // System Registers (0xF0 - 0xFF)
      static constexpr uint8_t REG_DEVICE_ID = 0xF0;
      static constexpr uint8_t REG_DEVICE_RESTART = 0xF1;

      I2C() = default;
      I2C(const I2C &) = delete;
      I2C &operator=(const I2C &) = delete;
      i2c_slave_dev_handle_t i2c_slave_handle_ = nullptr;
      TaskHandle_t task_ = nullptr;
      QueueHandle_t event_queue_ = nullptr;
      void taskLoop();
  };
} // namespace i2c_slave