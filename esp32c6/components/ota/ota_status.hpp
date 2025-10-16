#pragma once

#include <esp_err.h>
#include <cstdint>

namespace ota {
  enum class OTAState : uint8_t {
    UNKNOWN = 0,      // Uninitialized/invalid state
    IDLE = 1,
    DOWNLOADING = 2,
    VERIFYING = 3,
    FLASHING = 4,
    COMPLETE = 5,
    ERROR = 6
  };

  class OTAStatus {
    public:
      OTAState state;
      uint8_t progress; // 0-100
      esp_err_t error_code; // ESP-IDF error code (int32_t)
      uint32_t bytes_downloaded; // Total bytes downloaded
      char error_function[32]; // Function name where error occurred

      OTAStatus();

      // NVS persistence methods
      esp_err_t save_to_nvs();
      esp_err_t load_from_nvs();
      esp_err_t clear_nvs();
  };
} // namespace ota
