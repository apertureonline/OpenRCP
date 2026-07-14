idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        controls
        control_mapper
        openrcp_app
        openrcp_display
        iris_fader
        bmd_shield
)