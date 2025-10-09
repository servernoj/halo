#pragma once

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace zigbee {

  class Coordinator {
    public:
      static Coordinator &instance() {
        static Coordinator s;
        return s;
      }
      esp_err_t init();
      esp_err_t deinit();
      bool getButtonValue();

    private:
      static constexpr const char *TAG = "ZigBeeC";
      static void zigbee_task(void *pvParameters);
      TaskHandle_t task_handle_ = nullptr;
      bool initialized_ = false;
      bool should_stop_ = false;
      // Constructor business
      Coordinator() = default;
      Coordinator(const Coordinator &) = delete;
      Coordinator &operator=(const Coordinator &) = delete;
  };

} // namespace zigbee