#include "esp_heap_caps.h"
#include "esp_system.h"
#include "infotainment_ui.h"
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_illuminate.h"
#include "esp_ldo_regulator.h"
#include "esp_check.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    static esp_ldo_channel_handle_t ldo3 = nullptr;
    static esp_ldo_channel_handle_t ldo4 = nullptr;

    esp_ldo_channel_config_t ldo3_config = {};
    ldo3_config.chan_id = 3;
    ldo3_config.voltage_mv = 2500;
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo3_config, &ldo3));

    esp_ldo_channel_config_t ldo4_config = {};
    ldo4_config.chan_id = 4;
    ldo4_config.voltage_mv = 3300;
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo4_config, &ldo4));

    ESP_ERROR_CHECK(i2c_init());
    ESP_ERROR_CHECK(touch_init());
    ESP_ERROR_CHECK(display_init());

    if (lvgl_port_lock(1000)) {
        ESP_LOGI("MEMORY", "Before UI: free heap=%lu, largest block=%lu",
         (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
         (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
        infotainment_ui_create();
        ESP_LOGI("UI_TEST", "Interface creation finished");
        lvgl_port_unlock();
        ESP_LOGI("UI_TEST", "LVGL unlocked");
    } else {
        ESP_ERROR_CHECK(ESP_ERR_TIMEOUT);
    }

    ESP_ERROR_CHECK(set_lcd_blight(100));

    while (true) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
