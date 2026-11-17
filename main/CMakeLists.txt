idf_component_register(
    SRCS "main.cpp" "infotainment_ui.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        esp_lvgl_port
        bsp_display
        bsp_i2c
        bsp_illuminate
)