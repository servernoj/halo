#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>
#include <string.h>

#include "ota.hpp"
#include <WiFi_DPP.hpp>

namespace ota {
  esp_err_t OTA::init() {
    if (initialized_) {
      ESP_LOGW(TAG, "OTA already initialized");
      return ESP_OK;
    }

    // Check if we're in PENDING_VERIFY state
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
      if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGW(TAG, "OTA image in PENDING_VERIFY state - calling rollback handler");
        return handle_rollback();
      }
    }

    initialized_ = true;
    ESP_LOGI(TAG, "OTA initialized successfully");
    return ESP_OK;
  }

  esp_err_t OTA::handle_rollback() {
    ESP_LOGW(TAG, "Handling OTA rollback decision");

    // For minimal implementation, mark the app as valid (no rollback)
    // You can customize this logic based on your requirements
    esp_err_t ret = esp_ota_mark_app_valid_cancel_rollback();
    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "OTA image marked as valid, rollback cancelled");
    } else {
      ESP_LOGE(TAG, "Failed to mark OTA image as valid: %s", esp_err_to_name(ret));
      // If marking as valid fails, rollback and reboot
      ESP_LOGW(TAG, "Rolling back to previous version and rebooting");
      esp_ota_mark_app_invalid_rollback_and_reboot();
    }

    return ret;
  }

  esp_err_t OTA::set_ota_url(const char *url) {
    if (!url || strlen(url) == 0) {
      return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err;

    nvs_handle_t nvs_handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
      return err;
    }

    err = nvs_set_str(nvs_handle, NVS_URL_KEY, url);
    if (err == ESP_OK) {
      err = nvs_commit(nvs_handle);
      ESP_LOGI(TAG, "OTA URL updated: %s", url);
    }

    nvs_close(nvs_handle);
    return err;
  }

  esp_err_t OTA::get_ota_url(char *url_buffer, size_t *buffer_size) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
      return err;
    }

    size_t required_size = 0;
    err = nvs_get_str(nvs_handle, NVS_URL_KEY, NULL, &required_size);
    if (err == ESP_OK && required_size <= *buffer_size) {
      err = nvs_get_str(nvs_handle, NVS_URL_KEY, url_buffer, &required_size);
      if (err == ESP_OK) {
        *buffer_size = required_size;
      }
    } else if (err == ESP_OK) {
      err = ESP_ERR_NO_MEM; // Buffer too small
    }

    nvs_close(nvs_handle);
    return err;
  }

  esp_err_t OTA::trigger_ota_update() {
    // New approach: Set flag and reboot into OTA mode
    ESP_LOGI(TAG, "Triggering OTA update via reboot...");

    // Set OTA mode flag
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to open NVS for OTA mode flag: %s", esp_err_to_name(err));
      return err;
    }

    err = nvs_set_u8(nvs_handle, NVS_OTA_MODE_KEY, 1);
    if (err == ESP_OK) {
      err = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to set OTA mode flag: %s", esp_err_to_name(err));
      return err;
    }

    ESP_LOGI(TAG, "OTA mode flag set, rebooting in 1 second...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    // Never reaches here
    return ESP_OK;
  }

  bool OTA::is_ota_mode_pending() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
      return false;
    }

    uint8_t ota_mode = 0;
    err = nvs_get_u8(nvs_handle, NVS_OTA_MODE_KEY, &ota_mode);
    nvs_close(nvs_handle);

    if (err == ESP_OK && ota_mode == 1) {
      ESP_LOGI(TAG, "OTA mode pending flag detected");
      return true;
    }
    return false;
  }

  esp_err_t OTA::perform_ota_mode_update() {
    ESP_LOGI(TAG, "=== Entering OTA Mode ===");

    // Helper lambda to clear OTA mode flag
    auto clear_ota_mode = []() {
      nvs_handle_t nvs_handle;
      esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
      if (err == ESP_OK) {
        nvs_erase_key(nvs_handle, NVS_OTA_MODE_KEY);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "OTA mode flag cleared");
      }
    };

    // Get URL from NVS
    char url[256];
    size_t buf_len = sizeof(url);
    esp_err_t err = get_ota_url(url, &buf_len);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "No OTA URL configured in NVS");
      clear_ota_mode();
      return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "OTA URL: %s", url);

    // Initialize WiFi (no Zigbee interference!)
    err = init_wifi();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(err));
      clear_ota_mode();
      deinit_wifi();
      return err;
    }

    ESP_LOGI(TAG, "WiFi connected, starting OTA download...");

    // Perform OTA update (simplified version without Zigbee deinit)
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
      ESP_LOGE(TAG, "No OTA partition found");
      clear_ota_mode();
      deinit_wifi();
      return ESP_ERR_NOT_FOUND;
    }

    esp_ota_handle_t ota_handle;
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
      clear_ota_mode();
      deinit_wifi();
      return err;
    }

    ota_context_t ota_context = {ota_handle, false, 0, 0};

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = 30000;
    config.buffer_size = 65536;
    config.event_handler = http_event_handler;
    config.user_data = &ota_context;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    err = esp_http_client_perform(client);

    if (err == ESP_OK && ota_context.ota_success) {
      err = esp_ota_end(ota_handle);
      if (err == ESP_OK) {
        err = esp_ota_set_boot_partition(update_partition);
        if (err == ESP_OK) {
          ESP_LOGI(TAG, "OTA update successful (%d bytes)", ota_context.total_written);
          clear_ota_mode();
          esp_http_client_cleanup(client);
          deinit_wifi();
          ESP_LOGI(TAG, "Rebooting into new firmware...");
          vTaskDelay(pdMS_TO_TICKS(1000));
          esp_restart();
        } else {
          ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(err));
        }
      } else {
        ESP_LOGE(TAG, "Failed to end OTA: %s", esp_err_to_name(err));
      }
    } else {
      ESP_LOGE(TAG, "OTA download failed: %s", esp_err_to_name(err));
      esp_ota_abort(ota_handle);
    }

    esp_http_client_cleanup(client);
    clear_ota_mode();
    deinit_wifi();

    // If we reach here, OTA failed - reboot to normal mode
    ESP_LOGW(TAG, "OTA failed, rebooting to normal mode...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return err;
  }

  esp_err_t OTA::http_event_handler(esp_http_client_event_t *evt) {
    // Get OTA context from user_data
    ota_context_t *ota_context = (ota_context_t *)evt->user_data;

    switch (evt->event_id) {
      case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
      case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
      case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
      case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        // Capture content length for progress reporting
        if (ota_context && strcasecmp(evt->header_key, "Content-Length") == 0) {
          ota_context->content_length = atoi(evt->header_value);
          ESP_LOGI(TAG, "Content-Length: %d bytes", ota_context->content_length);
        }
        break;
      case HTTP_EVENT_ON_DATA:
        if (evt->data_len > 0 && ota_context) {
          esp_err_t err = esp_ota_write(ota_context->ota_handle, evt->data, evt->data_len);
          if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            ota_context->ota_success = false;
            return ESP_FAIL;
          }
          ota_context->total_written += evt->data_len;
          ota_context->ota_success = true;

          // Print progress every 16KB for more frequent updates
          if (ota_context->content_length > 0) {
            int progress_percent = (ota_context->total_written * 100) / ota_context->content_length;
            ESP_LOGI(
              TAG, "Download progress: %d/%d bytes (%d%%)", ota_context->total_written,
              ota_context->content_length, progress_percent
            );
          } else {
            ESP_LOGI(TAG, "Downloaded: %d bytes", ota_context->total_written);
          }
        }
        break;
      case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        if (ota_context) {
          ESP_LOGI(TAG, "Download completed: %d bytes total", ota_context->total_written);
        }
        break;
      case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
      default:
        break;
    }
    return ESP_OK;
  }

  esp_err_t OTA::init_wifi() {
    ESP_LOGI(TAG, "Initializing WiFi for OTA");
    esp_err_t ret = wifi::WiFi_DPP::instance().init();
    return ret;
  }

  esp_err_t OTA::deinit_wifi() {
    ESP_LOGI(TAG, "Deinitializing WiFi after OTA");
    return wifi::WiFi_DPP::instance().deinit();
  }

}