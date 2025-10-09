#include "esp_zigbee_cluster.h"
#include "esp_zigbee_type.h"
#include "zcl/esp_zigbee_zcl_core.h"
#include <esp_check.h>
#include <esp_log.h>

#include "Coordinator.hpp"
#include "Handlers.hpp"
#include <Motor.hpp>

namespace zigbee {

  bool Coordinator::getButtonValue() {
    esp_zb_zcl_attr_t *result = esp_zb_zcl_get_attribute(
      COORDINATOR_ENDPOINT, //
      ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, //
      ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, //
      ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID //
    );
    return *(bool *)result->data_p;
  }

  esp_err_t Coordinator::init() {
    if (initialized_) {
      ESP_LOGW(TAG, "Already initialized");
      return ESP_OK;
    }

    should_stop_ = false;
    // Create Zigbee task
    BaseType_t result = xTaskCreate(zigbee_task, "zigbee_main", 4096, this, 5, &task_handle_);

    if (result != pdPASS) {
      ESP_LOGE(TAG, "Failed to create Zigbee task");
      return ESP_FAIL;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Zigbee coordinator initialized");
    return ESP_OK;
  }

  // NOTE: deinit() kept for future use, but not needed for OTA (we use reboot instead)
  esp_err_t Coordinator::deinit() {
    if (!initialized_) {
      ESP_LOGW(TAG, "Not initialized");
      return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing Zigbee coordinator...");

    // Signal the task to stop
    should_stop_ = true;

    // Wait for task to finish gracefully (with timeout)
    if (task_handle_ != nullptr) {
      int timeout_ms = 2000;
      int wait_ms = 0;
      while (eTaskGetState(task_handle_) != eDeleted && wait_ms < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(50));
        wait_ms += 50;
      }

      if (eTaskGetState(task_handle_) != eDeleted) {
        ESP_LOGW(TAG, "Timeout waiting for graceful shutdown, force deleting Zigbee task");
        vTaskDelete(task_handle_);
      } else {
        ESP_LOGI(TAG, "Zigbee task stopped gracefully");
      }
      task_handle_ = nullptr;
    }

    initialized_ = false;
    should_stop_ = false;
    ESP_LOGI(TAG, "Zigbee coordinator deinitialized");
    return ESP_OK;
  }

  void Coordinator::zigbee_task(void *pvParameters) {
    Coordinator *self = static_cast<Coordinator *>(pvParameters);

    // Initialize Zigbee stack
    esp_zb_cfg_t zb_cfg = {
      .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,
      .install_code_policy = false,
      .nwk_cfg = {
        .zczr_cfg = {
          .max_children = CONFIG_ZIGBEE_COORDINATOR_MAX_DEVICES,
        }
      }
    };

    esp_zb_init(&zb_cfg);

    // Create and configure coordinator endpoint
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(nullptr);
    esp_zb_attribute_list_t *identify_cluster = esp_zb_identify_cluster_create(nullptr);
    esp_zb_on_off_cluster_cfg_t on_off_cfg = {.on_off = false};
    esp_zb_attribute_list_t *on_off_cluster = esp_zb_on_off_cluster_create(&on_off_cfg);
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(cluster_list, on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_endpoint_config_t endpoint_cfg = {
      .endpoint = COORDINATOR_ENDPOINT,
      .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
      .app_device_id = ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID,
      .app_device_version = 0
    };
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_cfg);
    esp_zb_device_register(ep_list);

    // Register action handler
    esp_zb_core_action_handler_register(zb_action_handler);

    // Set channel
    esp_zb_set_primary_network_channel_set(1 << CONFIG_ZIGBEE_CHANNEL);

    // Start Zigbee stack
    ESP_ERROR_CHECK(esp_zb_start(false));

    ESP_LOGI(TAG, "Zigbee stack started on channel %d", CONFIG_ZIGBEE_CHANNEL);

    int8_t power;
    esp_zb_get_tx_power(&power);
    ESP_LOGI(TAG, "Zigbee radio power level set to %d dB", power);

    // Main loop - controllable iteration-based loop
    while (!self->should_stop_) {
      // Run one iteration of the Zigbee stack
      esp_zb_stack_main_loop_iteration();

      // Small delay to allow other tasks to run and check stop flag
      vTaskDelay(pdMS_TO_TICKS(1));
    }

    vTaskDelete(NULL);
  }

} // namespace zigbee
