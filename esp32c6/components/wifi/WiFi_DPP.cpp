#include <esp_dpp.h>
#include <esp_err.h>
#include <esp_log.h>
#include <qrcode.h>

#include "WiFi_DPP.hpp"

namespace wifi {
  void WiFi_DPP::event_handler(esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
      switch (event_id) {
        case WIFI_EVENT_STA_START: {
          if (!provisioned_) {
            ESP_ERROR_CHECK(esp_supp_dpp_start_listen());
            ESP_LOGI(TAG, "Started listening for DPP Authentication");
          } else {
            esp_wifi_connect();
          }
          break;
        }
        case WIFI_EVENT_STA_DISCONNECTED: {
          auto *disconn_info = static_cast<wifi_event_sta_disconnected_t *>(event_data);
          ESP_LOGW(
            TAG, "WiFi disconnect - Reason: %d (%s), RSSI: %d", disconn_info->reason,
            (disconn_info->reason == 2     ? "AUTH_EXPIRE"
             : disconn_info->reason == 3   ? "AUTH_LEAVE"
             : disconn_info->reason == 4   ? "ASSOC_EXPIRE"
             : disconn_info->reason == 5   ? "ASSOC_TOOMANY"
             : disconn_info->reason == 6   ? "NOT_AUTHED"
             : disconn_info->reason == 7   ? "NOT_ASSOCED"
             : disconn_info->reason == 8   ? "ASSOC_LEAVE"
             : disconn_info->reason == 15  ? "4WAY_HANDSHAKE_TIMEOUT"
             : disconn_info->reason == 201 ? "NO_AP_FOUND"
             : disconn_info->reason == 202 ? "AUTH_FAIL"
             : disconn_info->reason == 203 ? "ASSOC_FAIL"
             : disconn_info->reason == 204 ? "HANDSHAKE_TIMEOUT"
                                           : "UNKNOWN"),
            disconn_info->rssi
          );

          if (retry_count_ < WIFI_MAX_RETRY_NUM) {
            // Exponential backoff: wait longer between retries
            int delay_ms = (1 << retry_count_) * 1000; // 1s, 2s, 4s, 8s...
            if (delay_ms > 30000) {
              delay_ms = 30000; // Cap at 30s
            }

            ESP_LOGI(
              TAG, "Disconnect event, retry %d/%d in %dms", retry_count_ + 1, WIFI_MAX_RETRY_NUM, delay_ms
            );

            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            esp_wifi_connect();
            retry_count_++;
          } else {
            ESP_LOGE(TAG, "Max retries reached, connection failed");
            xEventGroupSetBits(event_group_, DPP_CONNECT_FAIL_BIT);
          }
          break;
        }
        case WIFI_EVENT_STA_CONNECTED: {
          ESP_LOGI(TAG, "Successfully connected to the AP ssid: %s ", wifi_config_.sta.ssid);
          break;
        }
        case WIFI_EVENT_DPP_URI_READY: {
          auto *uri_data = static_cast<wifi_event_dpp_uri_ready_t *>(event_data);
          if (uri_data != NULL) {
            esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
            ESP_LOGI(TAG, "Scan below QR Code to configure the enrollee:");
            esp_qrcode_generate(&cfg, (const char *)uri_data->uri);
          }
          break;
        }
        case WIFI_EVENT_DPP_CFG_RECVD: {
          auto *config = static_cast<wifi_event_dpp_config_received_t *>(event_data);
          memcpy(&wifi_config_, &config->wifi_cfg, sizeof(wifi_config_));
          retry_count_ = 0;
          esp_wifi_set_config(WIFI_IF_STA, &wifi_config_);
          esp_wifi_connect();
          break;
        }
        case WIFI_EVENT_DPP_FAILED: {
          auto *dpp_failure = static_cast<wifi_event_dpp_failed_t *>(event_data);
          if (retry_count_ < 5) {
            ESP_LOGI(
              TAG, "DPP Auth failed (Reason: %s), retry...", esp_err_to_name((int)dpp_failure->failure_reason)
            );
            ESP_ERROR_CHECK(esp_supp_dpp_start_listen());
            retry_count_++;
          } else {
            xEventGroupSetBits(event_group_, DPP_AUTH_FAIL_BIT);
          }
          break;
        }
        default:
          break;
      }
    }
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
      ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
      retry_count_ = 0;
      xEventGroupSetBits(event_group_, DPP_CONNECTED_BIT);
    }
  }
  esp_err_t WiFi_DPP::deinit() {
    if (!initialized_) {
      ESP_LOGW(TAG, "WiFi_DPP not initialized");
      return ESP_OK;
    }

    // Stop WiFi
    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to stop WiFi: %s", esp_err_to_name(ret));
    }

    // Set WiFi mode to NULL (reverse of init setting WIFI_MODE_STA)
    ret = esp_wifi_set_mode(WIFI_MODE_NULL);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to set WiFi mode to NULL: %s", esp_err_to_name(ret));
    }

    // Deinitialize WiFi
    ret = esp_wifi_deinit();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to deinitialize WiFi: %s", esp_err_to_name(ret));
    }

    // Destroy default WiFi STA netif
    esp_netif_destroy_default_wifi(netif_sta_);

    // Delete default event loop
    esp_event_loop_delete_default();

    // Deinitialize netif
    esp_netif_deinit();

    // Reset state
    initialized_ = false;
    provisioned_ = false;
    retry_count_ = 0;

    ESP_LOGI(TAG, "WiFi_DPP deinitialized");
    return ESP_OK;
  }

  esp_err_t WiFi_DPP::init() {
    if (initialized_) {
      ESP_LOGW(TAG, "Already initialized");
      return ESP_OK;
    }
    event_group_ = xEventGroupCreate();
    provisioned_ = false;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    netif_sta_ = esp_netif_create_default_wifi_sta();
    auto trampoline = [](void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
      auto self = static_cast<WiFi_DPP *>(arg);
      self->event_handler(event_base, event_id, event_data);
    };
    // -- Register event handlers
    ESP_ERROR_CHECK(
      esp_event_handler_register(//
        WIFI_EVENT, //
        ESP_EVENT_ANY_ID, //
        trampoline, //
        this //
      )
    );
    ESP_ERROR_CHECK(
      esp_event_handler_register(//
        IP_EVENT, //
        IP_EVENT_STA_GOT_IP, //
        trampoline, //
        this //
      )
    );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    // -- Check if WiFi credentials are stored in NVS
    ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &wifi_config_));
    if (strlen((const char *)wifi_config_.sta.ssid)) {
      provisioned_ = true;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    if (!provisioned_) {
      ESP_ERROR_CHECK(esp_supp_dpp_init());
      ESP_ERROR_CHECK(esp_supp_dpp_bootstrap_gen(DPP_CHANNEL_LIST, DPP_BOOTSTRAP_QR_CODE, NULL, ""));
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    // -- Wait for DPP completion
    EventBits_t bits = xEventGroupWaitBits(
      event_group_, //
      DPP_CONNECTED_BIT | DPP_CONNECT_FAIL_BIT | DPP_AUTH_FAIL_BIT, //
      pdFALSE, //
      pdFALSE, //
      portMAX_DELAY //
    );
    if (!provisioned_) {
      esp_supp_dpp_deinit();
    }
    esp_err_t ret = ESP_ERR_NOT_FINISHED;
    if (bits & DPP_CONNECTED_BIT) {
      wifi_ap_record_t ap_info;
      esp_wifi_sta_get_ap_info(&ap_info);
      ESP_LOGI(TAG, "Connected to channel %d of SSID %s", ap_info.primary, ap_info.ssid);
      initialized_ = true;
      provisioned_ = true;
      ret = ESP_OK;
    } else {
      if (bits & DPP_CONNECT_FAIL_BIT) {
        ESP_LOGW(TAG, "Failed to connect to SSID: %s", wifi_config_.sta.ssid);
      } else if (bits & DPP_AUTH_FAIL_BIT) {
        ESP_LOGW(
          TAG, //
          "DPP Authentication failed after %d retries", //
          retry_count_ //
        );
      } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
      }
    }
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, trampoline));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, trampoline));
    vEventGroupDelete(event_group_);
    return ret;
  }

  esp_err_t WiFi_DPP::set_credentials(const char *ssid, const char *password) {
    if (!ssid || strlen(ssid) == 0 || strlen(ssid) >= sizeof(wifi_config_.sta.ssid)) {
      ESP_LOGE(TAG, "Invalid SSID");
      return ESP_ERR_INVALID_ARG;
    }
    if (!password || strlen(password) >= sizeof(wifi_config_.sta.password)) {
      ESP_LOGE(TAG, "Invalid password");
      return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Setting WiFi credentials: SSID=%s", ssid);

    // Get current config
    wifi_config_t wifi_config = {};
    esp_wifi_get_config(WIFI_IF_STA, &wifi_config);

    // Set SSID and password
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';

    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    // Set config (automatically saves to NVS)
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(err));
      return err;
    }

    // Update internal config
    memcpy(&wifi_config_, &wifi_config, sizeof(wifi_config_t));
    provisioned_ = true;

    ESP_LOGI(TAG, "WiFi credentials set successfully");
    return ESP_OK;
  }
}