idf_component_register(
    SRCS "src/bmd_shield.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c esp_driver_gpio openrcp_model
)