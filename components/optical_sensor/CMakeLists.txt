idf_component_register(
    SRCS "optical_sensor.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash driver tsl2591_driver ws2812b_driver
)
