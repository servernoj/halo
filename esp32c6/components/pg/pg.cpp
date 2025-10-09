#include <esp_check.h>
#include <esp_log.h>

#include "pg.hpp"

namespace pg {

  TaskHandle_t task_;

  void IRAM_ATTR isr_handler(void *args) {
    BaseType_t hp = pdFALSE;
    vTaskNotifyGiveFromISR(task_, &hp);
    portYIELD_FROM_ISR(hp);
  }

  void taskLoop(void *arg) {
    ESP_LOGI(TAG, "Task loop started");
    for (;;) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      ESP_LOGW(TAG, "--------------- %d", gpio_get_level(STOPPER_PIN));
    }
  }

  esp_err_t init() {
    xTaskCreate(taskLoop, "pg_task", 4096, NULL, 7, &task_);
    gpio_config_t io = {
      .pin_bit_mask = 1ULL << STOPPER_PIN,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_POSEDGE
    };
    ESP_RETURN_ON_ERROR(
      gpio_config(&io), //
      TAG, //
      "gpio_config failed" //
    );
    ESP_RETURN_ON_ERROR(
      gpio_install_isr_service(ESP_INTR_FLAG_IRAM), //
      TAG, //
      "gpio_install_isr_service failed" //
    );
    ESP_RETURN_ON_ERROR(
      gpio_isr_handler_add(STOPPER_PIN, isr_handler, NULL), //
      TAG, //
      "gpio_isr_handler_add failed" //
    );
    ESP_RETURN_ON_ERROR(
      gpio_intr_enable(STOPPER_PIN), //
      TAG, //
      "gpio_intr_enable failed" //
    );

    return ESP_OK;
  }

}