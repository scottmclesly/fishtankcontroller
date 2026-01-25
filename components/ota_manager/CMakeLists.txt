idf_component_register(
    SRCS "ota_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES app_update esp_https_ota esp_http_client nvs_flash
)
