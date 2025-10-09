#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_power_config.h"

#define MAX_ATTR_PER_CLUSTER 10
#define COORDINATOR_ENDPOINT 1

namespace zigbee {
  static constexpr const char *TAG = "ZigBeeH";
  typedef struct device_context_s {
      esp_zb_ieee_addr_t long_addr;
      uint16_t short_addr;
  } device_context_t;
  typedef struct {
      uint16_t cluster_id;
      uint16_t attr_id;
      esp_zb_zcl_attr_type_t type;
      const char *friendly_name;
  } attr_map_entry_t;
  inline attr_map_entry_t attr_map[] = {
    /* Basic cluster */
    {
      ESP_ZB_ZCL_CLUSTER_ID_BASIC, //
      ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, //
      ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING, //
      "Manufacturer" //
    },
    {
      ESP_ZB_ZCL_CLUSTER_ID_BASIC, //
      ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, //
      ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING, //
      "Model" //
    },
    {
      ESP_ZB_ZCL_CLUSTER_ID_BASIC, //
      ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID, //
      ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING, //
      "Date" //
    },
    /* Power config */
    {
      ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG, //
      ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID, //
      ESP_ZB_ZCL_ATTR_TYPE_U8, //
      "Voltage", //
    },
    {
      ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG, //
      ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, //
      ESP_ZB_ZCL_ATTR_TYPE_U8, //
      "Percentage", //
    }
  };
  extern "C" {
    void simple_desc_response_cb(
      esp_zb_zdp_status_t zdo_status, //
      esp_zb_af_simple_desc_1_1_t *simple_desc, //
      void *user_ctx //
    );
    void active_ep_response_cb(
      esp_zb_zdp_status_t zdo_status, //
      uint8_t ep_count, //
      uint8_t *ep_list, //
      void *user_ctx //
    );
    void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct);
    esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message);
    esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message);
  };

}