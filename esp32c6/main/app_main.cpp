#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>

#include <Coordinator.hpp>
#include <I2C.hpp>
#include <Motor.hpp>
#include <ota.hpp>

extern "C" void app_main(void) {
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
  } else {
    ESP_ERROR_CHECK(err);
  }
  // Check if we're in OTA mode (reboot-triggered OTA)
  if (ota::OTA::instance().is_ota_mode_pending()) {
    ESP_LOGI("MAIN", "=== OTA MODE DETECTED ===");
    ESP_LOGI("MAIN", "Skipping normal app initialization, performing OTA...");
    // Perform OTA update (will reboot on success or failure)
    ota::OTA::instance().perform_ota_mode_update();
    // Should never reach here (perform_ota_mode_update reboots)
    ESP_LOGE("MAIN", "OTA mode update returned unexpectedly");
    return;
  }
  // Normal mode: Initialize OTA component for rollback handling
  ESP_LOGI("MAIN", "=== NORMAL MODE ===");
  ESP_ERROR_CHECK(ota::OTA::instance().init());
  // -- App components
  ESP_ERROR_CHECK(motor::Motor::instance().init());
  ESP_ERROR_CHECK(i2c_slave::I2C::instance().init());
  ESP_ERROR_CHECK(zigbee::Coordinator::instance().init());
}
