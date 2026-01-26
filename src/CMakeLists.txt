idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
        lwip
        driver
        app_update
        esp_https_ota
        # Custom components
        poet_sensor
        wifi_manager
        http_server
        mqtt_client
        calibration
        tank_settings
        warning_manager
        derived_metrics
        display_driver
        data_history
        ota_manager
        optical_sensor
)
