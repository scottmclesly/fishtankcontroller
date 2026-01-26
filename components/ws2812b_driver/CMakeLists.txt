idf_component_register(
    SRCS "ws2812b_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES led_strip
)
