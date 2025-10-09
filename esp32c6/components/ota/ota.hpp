#pragma once

#include <esp_err.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_ota_ops.h>

namespace ota {
  // OTA context structure for HTTP event handling
  struct ota_context_t {
    esp_ota_handle_t ota_handle;
    bool ota_success;
    int total_written;
    int content_length;
  };

  class OTA {
    public:
      static OTA &instance() {
        static OTA s;
        return s;
      }

      esp_err_t init();
      esp_err_t handle_rollback();
      esp_err_t set_ota_url(const char *url);
      esp_err_t get_ota_url(char *url_buffer, size_t *buffer_size);
      esp_err_t trigger_ota_update();
      
      // OTA mode management (reboot-based)
      bool is_ota_mode_pending();
      esp_err_t perform_ota_mode_update();
      
      // Deprecated / internal methods (kept for compatibility)
      esp_err_t check_and_update(const char *url);        // deprecated
      esp_err_t check_and_update_backup(const char *url); // backup method
      esp_err_t init_wifi();      // internal
      esp_err_t deinit_wifi();    // internal

    private:
      static constexpr const char *TAG = "OTA";
      static constexpr const char *NVS_NAMESPACE = "ota_config";
      static constexpr const char *NVS_URL_KEY = "firmware_url";
      static constexpr const char *NVS_OTA_MODE_KEY = "ota_mode";

      bool initialized_ = false;

      OTA() = default;
      OTA(const OTA &) = delete;
      OTA &operator=(const OTA &) = delete;

      static esp_err_t http_event_handler(esp_http_client_event_t *evt);
  };
}