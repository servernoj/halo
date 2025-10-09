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
      // -- statuses
      static constexpr uint8_t REG_STATUS_MOTOR = 0x00;
      static constexpr uint8_t REG_STATUS_OTA = 0x01;
      // -- controls
      static constexpr uint8_t REG_CONTROL_MOTOR = 0x10;
      static constexpr uint8_t REG_CONTROL_OTA = 0x11;
      I2C() = default;
      I2C(const I2C &) = delete;
      I2C &operator=(const I2C &) = delete;
      i2c_slave_dev_handle_t i2c_slave_handle_ = nullptr;
      TaskHandle_t task_ = nullptr;
      QueueHandle_t event_queue_ = nullptr;
      void taskLoop();
  };
} // namespace i2c_slave