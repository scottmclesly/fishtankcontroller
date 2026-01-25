idf_component_register(
    SRCS "mqtt_client.c"
    INCLUDE_DIRS "include"
    REQUIRES mqtt esp-tls nvs_flash json esp_timer esp_system
)
