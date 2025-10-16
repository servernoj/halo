#include "ota_status.hpp"
#include "ota_nvs_keys.hpp"
#include <cstring>
#include <nvs.h>

namespace ota {
  OTAStatus::OTAStatus()
    : state(OTAState::IDLE),
      progress(0),
      error_code(ESP_OK),
      bytes_downloaded(0),
      error_function{0} {}

  esp_err_t OTAStatus::save_to_nvs() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
      return err;
    }

    nvs_set_u8(nvs_handle, NVS_KEY_STATE, static_cast<uint8_t>(state));
    nvs_set_u8(nvs_handle, NVS_KEY_PROGRESS, progress);
    nvs_set_i32(nvs_handle, NVS_KEY_ERROR, error_code);
    nvs_set_u32(nvs_handle, NVS_KEY_BYTES, bytes_downloaded);
    nvs_set_str(nvs_handle, NVS_KEY_ERROR_FN, error_function);

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
  }

  esp_err_t OTAStatus::load_from_nvs() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
      return err;
    }

    uint8_t state_val = 0;
    if (nvs_get_u8(nvs_handle, NVS_KEY_STATE, &state_val) == ESP_OK) {
      state = static_cast<OTAState>(state_val);
    }

    nvs_get_u8(nvs_handle, NVS_KEY_PROGRESS, &progress);

    int32_t err_code = 0;
    if (nvs_get_i32(nvs_handle, NVS_KEY_ERROR, &err_code) == ESP_OK) {
      error_code = static_cast<esp_err_t>(err_code);
    }

    nvs_get_u32(nvs_handle, NVS_KEY_BYTES, &bytes_downloaded);

    size_t len = sizeof(error_function);
    nvs_get_str(nvs_handle, NVS_KEY_ERROR_FN, error_function, &len);

    nvs_close(nvs_handle);
    return ESP_OK;
  }

  esp_err_t OTAStatus::clear_nvs() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
      return err;
    }

    nvs_erase_key(nvs_handle, NVS_KEY_STATE);
    nvs_erase_key(nvs_handle, NVS_KEY_PROGRESS);
    nvs_erase_key(nvs_handle, NVS_KEY_ERROR);
    nvs_erase_key(nvs_handle, NVS_KEY_BYTES);
    nvs_erase_key(nvs_handle, NVS_KEY_ERROR_FN);

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
  }
} // namespace ota
