#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>

#include "Handlers.hpp"
#include <Motor.hpp>

namespace zigbee {
  extern "C" {
    void bind_response_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
      device_context_t *ctx = (device_context_t *)user_ctx;
      if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Bound to 0x%04X", ctx->short_addr);
      } else {
        ESP_LOGE(TAG, "Bound to 0x%04X failed", ctx->short_addr);
      }
    }
    void simple_desc_response_cb(
      esp_zb_zdp_status_t zdo_status, //
      esp_zb_af_simple_desc_1_1_t *simple_desc, //
      void *user_ctx //
    ) {
      if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        device_context_t *ctx = (device_context_t *)user_ctx;
        ESP_LOGI(
          TAG, //
          "Clusters of endpoint %d on device: 0x%04X", //
          simple_desc->endpoint, //
          ctx->short_addr
        );
        int totalClusters = simple_desc->app_input_cluster_count + simple_desc->app_output_cluster_count;
        for (int i = 0; i < totalClusters; i++) {
          bool isInput = i < simple_desc->app_input_cluster_count;
          ESP_LOGI(
            TAG, //
            "  [0x%04X]: %s", simple_desc->app_cluster_list[i], isInput ? "input" : "output"
          );
          if (isInput) {
            uint16_t attrIds[MAX_ATTR_PER_CLUSTER];
            int clusterSize = 0;
            size_t total = sizeof(attr_map) / sizeof(attr_map[0]);
            for (int j = 0; j < total; j++) {
              if (attr_map[j].cluster_id == simple_desc->app_cluster_list[i]) {
                attrIds[clusterSize++] = attr_map[j].attr_id;
              }
            }
            if (clusterSize > 0) {
              // -- Request to read/discover attribute types
              esp_zb_zcl_disc_attr_cmd_t cmd_disc_req = {
                .zcl_basic_cmd = {
                  .dst_addr_u = {.addr_short = ctx->short_addr},
                  .dst_endpoint = simple_desc->endpoint,
                  .src_endpoint = COORDINATOR_ENDPOINT,
                },
                .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
                .cluster_id = simple_desc->app_cluster_list[i],
                .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
                .start_attr_id = 0,
                .max_attr_number = MAX_ATTR_PER_CLUSTER
              };
              esp_zb_zcl_disc_attr_cmd_req(&cmd_disc_req);
              // -- Request to read attribute values
              esp_zb_zcl_read_attr_cmd_t cmd_read_req = { //
                .zcl_basic_cmd = {
                  .dst_addr_u = {.addr_short = ctx->short_addr},
                  .dst_endpoint = simple_desc->endpoint,
                  .src_endpoint = COORDINATOR_ENDPOINT,
                },
                .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
                .clusterID = simple_desc->app_cluster_list[i],
                .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
                .attr_number = 3,
                .attr_field = attrIds,
              };
              esp_zb_zcl_read_attr_cmd_req(&cmd_read_req);
            }
          } else if (simple_desc->app_cluster_list[i] == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
            esp_zb_zdo_bind_req_param_t bind_req;
            memset(&bind_req, 0, sizeof(bind_req));
            esp_zb_ieee_addr_t coordinator_ieee_addr;
            esp_zb_get_long_address(coordinator_ieee_addr);
            memcpy(bind_req.src_address, ctx->long_addr, sizeof(bind_req.src_address));
            memcpy(bind_req.dst_address_u.addr_long, coordinator_ieee_addr, sizeof(esp_zb_ieee_addr_t));
            bind_req.src_endp = simple_desc->endpoint;
            bind_req.cluster_id = simple_desc->app_cluster_list[i];
            bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
            bind_req.dst_endp = COORDINATOR_ENDPOINT;
            bind_req.req_dst_addr = ctx->short_addr;
            esp_zb_zdo_device_bind_req(&bind_req, bind_response_cb, ctx);
            ESP_LOGI(TAG, "Binding device 0x%X", ctx->short_addr);
          }
        }
      } else {
        ESP_LOGE(TAG, "EP Response status(0x%02X)", zdo_status);
      }
    }
    // --
    void active_ep_response_cb(
      esp_zb_zdp_status_t zdo_status, //
      uint8_t ep_count, //
      uint8_t *ep_list, //
      void *user_ctx //
    ) {
      if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        device_context_t *ctx = (device_context_t *)user_ctx;
        ESP_LOGI(
          TAG, //
          "Found %d endpoints on device 0x%02X", //
          ep_count, //
          ctx->short_addr
        );
        for (int i = 0; i < ep_count; i++) {
          esp_zb_zdo_simple_desc_req_param_t sdr = {
            .addr_of_interest = ctx->short_addr, //
            .endpoint = ep_list[i] //
          };
          esp_zb_zdo_simple_desc_req(&sdr, simple_desc_response_cb, user_ctx);
        }
      } else {
        ESP_LOGE(TAG, "EP Response status(0x%02X)", zdo_status);
      }
    }
    // --
    void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
      uint32_t *p_sg_p = signal_struct->p_app_signal;
      esp_err_t err_status = signal_struct->esp_err_status;
      esp_zb_app_signal_type_t sig_type = static_cast<esp_zb_app_signal_type_t>(*p_sg_p);

      switch (sig_type) {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP: {
          ESP_LOGI(TAG, "Initialize Zigbee stack");
          esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
          break;
        }
        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT: {
          if (err_status == ESP_OK) {
            ESP_LOGI(
              TAG, "Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non"
            );
            if (esp_zb_bdb_is_factory_new()) {
              ESP_LOGI(TAG, "Start network formation");
              esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
            } else {
              esp_zb_bdb_open_network(20);
              ESP_LOGI(TAG, "Device rebooted");
            }
          } else {
            ESP_LOGE(TAG, "Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
          }
          break;
        }
        case ESP_ZB_BDB_SIGNAL_FORMATION: {
          if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Network formed successfully");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
          }
          break;
        }
        case ESP_ZB_BDB_SIGNAL_STEERING: {
          if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Network steering complete");
          }
          break;
        }
        case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE: {
          esp_zb_zdo_signal_device_annce_params_t *params;
          params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
          ESP_LOGI(TAG, "Device joined: 0x%04x", params->device_short_addr);
          break;
        }
        case ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED: {
          esp_zb_zdo_signal_device_authorized_params_t *params;
          params = (esp_zb_zdo_signal_device_authorized_params_t *)esp_zb_app_signal_get_params(p_sg_p);
          device_context_t *ctx = (device_context_t *)malloc(sizeof(device_context_t));
          memset(ctx, 0, sizeof(*ctx));
          memcpy(ctx->long_addr, params->long_addr, sizeof(esp_zb_ieee_addr_t));
          ctx->short_addr = params->short_addr;
          ESP_LOGI(TAG, "Device authorized: 0x%04x", params->short_addr);
          esp_zb_zdo_active_ep_req_param_t req;
          req.addr_of_interest = params->short_addr;
          esp_zb_zdo_active_ep_req(&req, active_ep_response_cb, ctx);
          break;
        }
        case ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION: {
          esp_zb_zdo_signal_leave_indication_params_t *sig_params;
          sig_params = (esp_zb_zdo_signal_leave_indication_params_t *)esp_zb_app_signal_get_params(p_sg_p);
          ESP_LOGI(TAG, "Device left: 0x%04x, ", sig_params->short_addr);
          break;
        }
        case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS: {
          if (err_status == ESP_OK) {
            if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
              ESP_LOGW(
                TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(),
                *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)
              );
            } else {
              ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
            }
          }
          break;
        }
        default:
          ESP_LOGI(
            TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
            esp_err_to_name(err_status)
          );
          break;
      }
    }
    // --
    esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message) {
      esp_err_t ret = ESP_OK;
      bool light_state = 0;

      ESP_RETURN_ON_FALSE(
        message, //
        ESP_FAIL, //
        TAG, //
        "Empty message" //
      );
      ESP_RETURN_ON_FALSE(
        message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, //
        ESP_ERR_INVALID_ARG, //
        TAG, //
        "Received message: error status(%d)", message->info.status //
      );
      ESP_LOGI(
        TAG,
        "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), "
        "data size(%d)",
        message->info.dst_endpoint, message->info.cluster, message->attribute.id, message->attribute.data.size
      );
      if (message->info.dst_endpoint == COORDINATOR_ENDPOINT) {
        if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
          if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID
              && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
            light_state
              = message->attribute.data.value ? *(bool *)message->attribute.data.value : light_state;
            motor::Motor::instance().submit(
              motor::Move {
                .degrees = +1,
                .end_action = motor::EndAction::HOLD,
                .move_type = motor::MoveType::FREE
              }
            );
          }
        } else {
          ESP_LOGI(
            TAG, "Message data: cluster(0x%x), attribute(0x%x)  ", message->info.cluster,
            message->attribute.id
          );
        }
      }
      return ret;
    }
    // --
    esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
      esp_err_t ret = ESP_OK;

      switch (callback_id) {
        case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID: {
          ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
          break;
        }
        case ESP_ZB_CORE_CMD_DISC_ATTR_RESP_CB_ID: {
          auto m = (esp_zb_zcl_cmd_discover_attributes_resp_message_t *)message;
          if (m->info.status == ESP_ZB_ZCL_STATUS_SUCCESS) {
            esp_zb_zcl_disc_attr_variable_s *next = m->variables;
            ESP_LOGI(TAG, "Attribute types for cluster 0x%04X:", m->info.cluster);
            while (next) {
              ESP_LOGI(TAG, "  [0x%04X]: 0x%02X", next->attr_id, next->data_type);
              next = next->next;
            }
          }
          break;
        }
        case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID: {
          auto m = (esp_zb_zcl_cmd_read_attr_resp_message_t *)message;
          if (m->info.status == ESP_ZB_ZCL_STATUS_SUCCESS) {
            esp_zb_zcl_read_attr_resp_variable_s *next = m->variables;
            ESP_LOGI(TAG, "Attribute values for cluster 0x%04X:", m->info.cluster);
            while (next) {
              if (next->status == ESP_ZB_ZCL_STATUS_SUCCESS) {
                int idx = -1;
                size_t total = sizeof(attr_map) / sizeof(attr_map[0]);
                for (int i = 0; i < total; i++) {
                  if ( //
                    attr_map[i].cluster_id == m->info.cluster && //
                    attr_map[i].attr_id == next->attribute.id //
                  ) {
                    idx = i;
                    break;
                  }
                }
                if (idx >= 0) {
                  attr_map_entry_t *entry = attr_map + idx;
                  switch (entry->type) {
                    case ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING: {
                      uint8_t len = *(uint8_t *)next->attribute.data.value;
                      ESP_LOGI(
                        TAG, "  %-15s: %.*s", //
                        entry->friendly_name, len, (char *)next->attribute.data.value + 1
                      );
                      break;
                    }
                    case ESP_ZB_ZCL_ATTR_TYPE_U8: {
                      uint8_t rawValue = *(uint8_t *)next->attribute.data.value;
                      ESP_LOGI(
                        TAG, "  %-15s: %u", //
                        entry->friendly_name, rawValue
                      );
                      break;
                    }
                    default:
                      break;
                  }
                }
              } else {
                ESP_LOGW(TAG, "Attr[0x%04X] status: 0x%02X", next->attribute.id, next->status);
              }
              next = next->next;
            }
          }
          break;
        }
        default:
          ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
          break;
      }
      return ret;
    }
  }
}