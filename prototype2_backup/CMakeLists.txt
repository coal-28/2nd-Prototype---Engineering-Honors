idf_component_register(
    SRCS "before_hub_layout_v4.cpp" "before_hub_layout_v3.cpp" "before_hub_fixed.cpp" "before_hub_resize.cpp" "before_layout_v2.cpp" "before_home_correction.cpp" "before_batch4.cpp" "before_temperature_fix.cpp" "before_music_phone_batch.cpp" "before_radio_batch.cpp" "before_layout_batch.cpp" "main.cpp" "infotainment_ui.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        esp_lvgl_port
        bsp_display
        bsp_i2c
        bsp_illuminate
)