#pragma once

namespace ota {
  // Shared NVS namespace and keys for all OTA-related storage
  static constexpr const char *NVS_NAMESPACE = "ota_config";
  static constexpr const char *NVS_KEY_URL = "ota_url";
  static constexpr const char *NVS_KEY_MODE = "ota_mode";
  static constexpr const char *NVS_KEY_STATE = "ota_state";
  static constexpr const char *NVS_KEY_PROGRESS = "ota_progress";
  static constexpr const char *NVS_KEY_ERROR = "ota_error";
  static constexpr const char *NVS_KEY_BYTES = "ota_bytes";
  static constexpr const char *NVS_KEY_ERROR_FN = "ota_err_fn";
} // namespace ota
