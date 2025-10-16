#pragma once

#include <esp_err.h>
#include <esp_wifi.h>

#define DPP_CONNECTED_BIT BIT0
#define DPP_CONNECT_FAIL_BIT BIT1
#define DPP_AUTH_FAIL_BIT BIT2
#define DPP_BOOTSTRAPPING_KEY ""
#define DPP_CHANNEL_LIST "6"
#define WIFI_MAX_RETRY_NUM 4

namespace wifi {
  class WiFi_DPP {
    public:
      static WiFi_DPP &instance() {
        static WiFi_DPP s;
        return s;
      }
      
      // DPP methods
      esp_err_t init();
      esp_err_t deinit();
      
      // Credentials method (alternative to DPP)
      esp_err_t set_credentials(const char *ssid, const char *password);

    private:
      static constexpr const char *TAG = "WiFi_DPP";
      void event_handler(esp_event_base_t event_base, int32_t event_id, void *event_data);
      bool provisioned_ = false;
      int retry_count_ = 0;
      wifi_config_t wifi_config_;
      bool initialized_ = false;
      esp_netif_t *netif_sta_;
      EventGroupHandle_t event_group_;
      // Constructor business
      WiFi_DPP() = default;
      WiFi_DPP(const WiFi_DPP &) = delete;
      WiFi_DPP &operator=(const WiFi_DPP &) = delete;
  };

}