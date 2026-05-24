#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"

#define TAG "main"

extern "C" void app_main(void)
{
    // Early debug: toggle GPIO40 to signal app_main entered
    gpio_set_direction(GPIO_NUM_40, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_40, 1);  // LED on = app_main started
    ESP_LOGI(TAG, "=== APP_MAIN START ===");

    // Initialize NVS flash for WiFi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS flash initialized");

    // Force LCD power (GPIO15) and backlight (GPIO38) on for testing
    gpio_config_t io = {};
    io.pin_bit_mask = (1ULL << 15) | (1ULL << 38);
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io);
    gpio_set_level(GPIO_NUM_15, 1);  // LCD power on
    gpio_set_level(GPIO_NUM_38, 1);  // Backlight on
    gpio_set_level(GPIO_NUM_40, 0);  // LED off after init
    ESP_LOGI(TAG, "FORCE LCD/GPIO15=1 BL/GPIO38=1");

    // Initialize and run the application
    ESP_LOGI(TAG, "Creating Application::GetInstance()...");
    gpio_set_level(GPIO_NUM_40, 1);  // LED on = before GetInstance
    auto& app = Application::GetInstance();
    gpio_set_level(GPIO_NUM_40, 0);  // LED off = after GetInstance
    ESP_LOGI(TAG, "Application instance created, calling Initialize()...");
    
    gpio_set_level(GPIO_NUM_40, 1);  // LED on = before Initialize
    app.Initialize();
    gpio_set_level(GPIO_NUM_40, 0);  // LED off = after Initialize
    ESP_LOGI(TAG, "App initialized, calling Run()...");
    
    gpio_set_level(GPIO_NUM_40, 1);  // LED on = before Run
    app.Run();  // This function runs the main event loop and never returns
}
