idf_component_register(
    SRCS "src/openrcp_app.c"
    INCLUDE_DIRS "include"
    REQUIRES openrcp_model control_mapper
)