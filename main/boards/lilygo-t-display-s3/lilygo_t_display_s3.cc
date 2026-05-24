#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"

#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>

#define TAG "LilygoTDisplayS3"

// Early debug: toggle GPIO40 three times rapidly to signal constructor entry
// Uses ESP-IDF gpio driver API
static void early_debug_led(void) {
    gpio_set_direction(GPIO_NUM_40, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_40, 1);
    for (volatile int dly = 0; dly < 200000; dly++);
    gpio_set_level(GPIO_NUM_40, 0);
    for (volatile int dly = 0; dly < 200000; dly++);
    gpio_set_level(GPIO_NUM_40, 1);
    for (volatile int dly = 0; dly < 200000; dly++);
    gpio_set_level(GPIO_NUM_40, 0);
    for (volatile int dly = 0; dly < 200000; dly++);
    gpio_set_level(GPIO_NUM_40, 1);
    for (volatile int dly = 0; dly < 200000; dly++);
    gpio_set_level(GPIO_NUM_40, 0);
}

class LilygoTDisplayS3Board : public WifiBoard {
private:
    Button boot_button_;
    Button touch_button_;
    Button asr_button_;
    LcdDisplay* display_;

    void InitializeSpi() {
        gpio_set_level(BUILTIN_LED_GPIO, 0);  // debug
        ESP_LOGI(TAG, "SPI init: mosi=%d miso=%d clk=%d", DISPLAY_MOSI_PIN, DISPLAY_MISO_PIN, DISPLAY_CLK_PIN);
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = DISPLAY_MISO_PIN;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        esp_err_t err = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
        ESP_LOGI(TAG, "spi_bus_initialize result: %s", esp_err_to_name(err));
        if (err != ESP_OK) return;
        gpio_set_level(BUILTIN_LED_GPIO, 1);  // debug - SPI done
    }

    void InitializeLcdDisplay() {
        gpio_set_level(BUILTIN_LED_GPIO, 0);  // debug
        ESP_LOGI(TAG, "LCD init: cs=%d dc=%d rst=%d", DISPLAY_CS_PIN, DISPLAY_DC_PIN, DISPLAY_RST_PIN);
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_LOGI(TAG, "esp_lcd_new_panel_io_spi start");
        esp_err_t err = esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io);
        ESP_LOGI(TAG, "esp_lcd_new_panel_io_spi: %s", esp_err_to_name(err));
        if (err != ESP_OK) return;
        gpio_set_level(BUILTIN_LED_GPIO, 1);  // debug - panel io done
        
        ESP_LOGI(TAG, "esp_lcd_new_panel_st7789 start");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        err = esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel);
        ESP_LOGI(TAG, "esp_lcd_new_panel_st7789: %s", esp_err_to_name(err));
        if (err != ESP_OK) return;
        gpio_set_level(BUILTIN_LED_GPIO, 0);  // debug
        
        ESP_LOGI(TAG, "esp_lcd_panel_reset start");
        esp_lcd_panel_reset(panel);
        ESP_LOGI(TAG, "esp_lcd_panel_init start");
        esp_lcd_panel_init(panel);
        ESP_LOGI(TAG, "esp_lcd_panel_invert_color start");
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        ESP_LOGI(TAG, "esp_lcd_panel_swap_xy start");
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        ESP_LOGI(TAG, "esp_lcd_panel_mirror start");
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        gpio_set_level(BUILTIN_LED_GPIO, 1);  // debug - panel ops done
        
        ESP_LOGI(TAG, "Creating SpiLcdDisplay...");
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        ESP_LOGI(TAG, "SpiLcdDisplay created");
    }

    void InitializeButtons() {
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << BUILTIN_LED_GPIO,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf);

        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            gpio_set_level(BUILTIN_LED_GPIO, 1);
            app.ToggleChatState();
        });

        asr_button_.OnClick([this]() {
            std::string wake_word = "你好小智";
            Application::GetInstance().WakeWordInvoke(wake_word);
        });

        touch_button_.OnPressDown([this]() {
            gpio_set_level(BUILTIN_LED_GPIO, 1);
            Application::GetInstance().StartListening();
        });

        touch_button_.OnPressUp([this]() {
            gpio_set_level(BUILTIN_LED_GPIO, 0);
            Application::GetInstance().StopListening();
        });
    }

public:
    LilygoTDisplayS3Board() : WifiBoard(),
        boot_button_(BOOT_BUTTON_GPIO),
        touch_button_(TOUCH_BUTTON_GPIO),
        asr_button_(ASR_BUTTON_GPIO) {
        early_debug_led();  // 3 rapid LED blinks (GPIO40) before anything else
        ESP_LOGI(TAG, "=== LilygoTDisplayS3 CONSTRUCTOR START ===");
        ESP_LOGI(TAG, "DISPLAY_BL_PIN=%d GPIO_NUM_NC=%d", DISPLAY_BL_PIN, GPIO_NUM_NC);
        gpio_config_t dbg_io = {};
        dbg_io.pin_bit_mask = 1ULL << BUILTIN_LED_GPIO;
        dbg_io.mode = GPIO_MODE_OUTPUT;
        dbg_io.pull_up_en = GPIO_PULLUP_DISABLE;
        dbg_io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        dbg_io.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&dbg_io);
        gpio_set_level(BUILTIN_LED_GPIO, 1);  // LED on = constructor start
        ESP_LOGI(TAG, "GPIO %d set HIGH (constructor start)", BUILTIN_LED_GPIO);
        InitializeSpi();  // toggles: LOW at start, HIGH when done
        InitializeLcdDisplay();  // toggles: LOW at start, HIGH when done
        gpio_set_level(BUILTIN_LED_GPIO, 0);  // LED off = init funcs done
        ESP_LOGI(TAG, "Init funcs done, calling InitializeButtons()");
        InitializeButtons();
        gpio_set_level(BUILTIN_LED_GPIO, 1);  // LED on = buttons done
        ESP_LOGI(TAG, "Buttons done, initializing backlight...");
        if (DISPLAY_BL_PIN != GPIO_NUM_NC) {
            ESP_LOGI(TAG, "Initializing backlight on GPIO %d", DISPLAY_BL_PIN);
            GetBacklight()->RestoreBrightness();
            ESP_LOGI(TAG, "Backlight initialized, brightness: %d", GetBacklight()->brightness());
        } else {
            ESP_LOGW(TAG, "Backlight pin is NC, skipping backlight init");
        }
        gpio_set_level(BUILTIN_LED_GPIO, 0);  // LED off = constructor done
        ESP_LOGI(TAG, "=== LilygoTDisplayS3 CONSTRUCTOR END ===");
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BL_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BL_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }
};

DECLARE_BOARD(LilygoTDisplayS3Board);
