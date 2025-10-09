#pragma once

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define STOPPER_PIN GPIO_NUM_3

namespace pg {
  static constexpr const char *TAG = "PG";
  esp_err_t init();
}