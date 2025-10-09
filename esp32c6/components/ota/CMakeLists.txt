idf_component_register(
  SRCS "ota.cpp"
  INCLUDE_DIRS "."
  REQUIRES esp_http_client esp_https_ota nvs_flash wifi_dpp app_update
)