idf_component_register(
  SRCS "ota.cpp" "ota_status.cpp"
  INCLUDE_DIRS "."
  REQUIRES esp_http_client esp_https_ota nvs_flash wifi app_update
)