idf_component_register(
    SRCS
        "src/controls.c"
        "src/button_panel.c"
        "src/camera_button.c"
        "src/rotary_encoder.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2c freertos openrcp_model
)
