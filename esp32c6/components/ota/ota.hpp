#pragma once

#include <esp_err.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_ota_ops.h>

#include "ota_status.hpp"

namespace ota {
  // OTA context structure for HTTP event handling
  struct ota_context_t {
      esp_ota_handle_t ota_handle;
      bool ota_success;
      int total_written;
      int content_length;
      OTAStatus *status; // Pointer to status for updates during OTA
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

      // OTA status management
      const OTAStatus &get_status() const { return status_; }
      void reset_status();

      esp_err_t init_wifi(); // internal
      esp_err_t deinit_wifi(); // internal

    private:
      static constexpr const char *TAG = "OTA";

      bool initialized_ = false;
      OTAStatus status_;

      OTA() = default;
      OTA(const OTA &) = delete;
      OTA &operator=(const OTA &) = delete;

      static esp_err_t http_event_handler(esp_http_client_event_t *evt);

      // Helper methods for status updates
      void set_state(OTAState state);
      void set_error(esp_err_t err, const char *function_name);
      void set_progress(uint8_t progress);
  };
}