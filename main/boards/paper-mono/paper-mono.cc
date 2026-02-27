#include "wifi_board.h"
#include "audio/codecs/no_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include <esp_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_app_desc.h"
#include <driver/i2c_master.h>
#include "driver/spi_common.h"
#include "mcp_server.h"
#include "assets/lang_config.h"
#include "custom_display.h"
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "font_emoji.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_epd_gdem0397t81p.h"
#include <esp_lvgl_port.h>
#include "esp_lcd_touch_ft5x06.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"
#include "M5PM1.h"
#include "M5IOE1.h"


static const char *TAG = "Paper-Mono Board";


LV_FONT_DECLARE(font_puhui_30_4);
LV_FONT_DECLARE(font_awesome_30_4);


class PaperMonoBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    M5PM1 pmic_;
    M5IOE1 ioe1_;
    bool pmic_ioe1_ready_ = false;
    Button user_button_;
    Button bl_key_;
    uint8_t bl_level_ = 3;  // 0-5: 20,40,60,80,100,0，初始 80%
    SpiCustomDisplay *display_ = nullptr;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    esp_lcd_touch_handle_t touch_ = nullptr;

    NoAudioCodecPdmInputOnly *pdm_codec_ = nullptr;

    void InitializeI2c();
    void InitializePmicIoe();
    void InitializeSpi();
    void InitializeDisplay();
    void InitializeButtons();
    void InitializeBacklight();

    void ClearDisplay(uint8_t color);

    bool IsGuidePageRequired();

    virtual Display *GetDisplay() override;

public:
    PaperMonoBoard();
    ~PaperMonoBoard();
    virtual AudioCodec *GetAudioCodec() override;
    virtual bool GetBatteryLevel(int &level, bool &charging, bool &discharging) override;
};

void PaperMonoBoard::InitializeI2c()
{
    ESP_LOGI(TAG, "Initialize I2C peripheral");
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = SYS_I2C_NUM,
        .sda_io_num = SYS_I2C_PIN_SDA,
        .scl_io_num = SYS_I2C_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = { .enable_internal_pullup = 0 },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
}

void PaperMonoBoard::InitializePmicIoe()
{
    if (pmic_ioe1_ready_) return;

    m5pm1_err_t pmic_ret = pmic_.begin(i2c_bus_, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K);
    if (pmic_ret != M5PM1_OK) {
        ESP_LOGE(TAG, "M5PM1 init failed: %d", pmic_ret);
        return;
    }
    ESP_LOGI(TAG, "M5PM1 initialized");

    m5ioe1_err_t ioe1_ret = ioe1_.begin(i2c_bus_, M5IOE1_DEFAULT_ADDR, M5IOE1_I2C_FREQ_DEFAULT, M5IOE1_INT_MODE_POLLING);
    if (ioe1_ret != M5IOE1_OK) {
        ESP_LOGE(TAG, "M5IOE1 init failed: %d", ioe1_ret);
        return;
    }
    ESP_LOGI(TAG, "M5IOE1 initialized");
    pmic_ioe1_ready_ = true;

    pmic_.setLedEnLevel(false); // 关闭电源指示灯
}

void PaperMonoBoard::InitializeSpi()
{
    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t bus_cfg = {};
    bus_cfg.sclk_io_num = EPD_PIN_SCK;
    bus_cfg.mosi_io_num = EPD_PIN_MOSI;
    bus_cfg.miso_io_num = EPD_PIN_MISO;
    bus_cfg.quadwp_io_num = GPIO_NUM_NC;
    bus_cfg.quadhd_io_num = GPIO_NUM_NC;
    bus_cfg.max_transfer_sz = EPD_RES_HEIGHT * EPD_RES_WIDTH;
    ESP_ERROR_CHECK(spi_bus_initialize(EPD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));
}

void PaperMonoBoard::InitializeDisplay()
{
    InitializePmicIoe();
    if (!pmic_ioe1_ready_) {
        ESP_LOGE(TAG, "M5PM1/M5IOE1 not ready, display init aborted");
        return;
    }

    /* M5IOE1: EPD 电源与复位 */
    ioe1_.pinMode(EPD_PWR_EN_IOE_PIN, OUTPUT);
    ioe1_.setDriveMode(EPD_PWR_EN_IOE_PIN, M5IOE1_DRIVE_PUSHPULL);
    ioe1_.digitalWrite(EPD_PWR_EN_IOE_PIN, HIGH);  /* 开启 EPD 电源 */
    vTaskDelay(pdMS_TO_TICKS(10));

    ioe1_.pinMode(EPD_RST_IOE_PIN, OUTPUT);
    ioe1_.setDriveMode(EPD_RST_IOE_PIN, M5IOE1_DRIVE_PUSHPULL);
    ioe1_.digitalWrite(EPD_RST_IOE_PIN, LOW);   /* 复位拉低 */
    vTaskDelay(pdMS_TO_TICKS(10));
    ioe1_.digitalWrite(EPD_RST_IOE_PIN, HIGH);  /* 复位释放 */
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.dc_gpio_num = EPD_PIN_DC;
    io_cfg.cs_gpio_num = EPD_PIN_CS;
    io_cfg.pclk_hz = 40 * 1000 * 1000;
    io_cfg.lcd_cmd_bits = 8;
    io_cfg.lcd_param_bits = 8;
    io_cfg.spi_mode = 0;
    io_cfg.trans_queue_depth = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EPD_SPI_HOST, &io_cfg, &panel_io_));

    gdem0397t81p_vendor_config_t vendor_cfg = {
        .busy_gpio_num = EPD_BUSY_PIN,
    };
    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.reset_gpio_num = GPIO_NUM_NC;  /* M5IOE1 controls RST */
    panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_cfg.bits_per_pixel = 8;
    panel_cfg.vendor_config = &vendor_cfg;
    ESP_LOGI(TAG, "Install GDEM0397T81P panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_gdem0397t81p(panel_io_, &panel_cfg, &panel_));
    esp_lcd_panel_reset(panel_);
    esp_lcd_panel_init(panel_);
    // // TODO: 这里等待耗时，需要优化  
    // ClearDisplay(0x00);
    // ClearDisplay(0xFF);

    /* M5IOE1: Touch 电源与复位 */
    ioe1_.pinMode(TP_PWR_EN_IOE_PIN, OUTPUT);
    ioe1_.setDriveMode(TP_PWR_EN_IOE_PIN, M5IOE1_DRIVE_PUSHPULL);
    ioe1_.digitalWrite(TP_PWR_EN_IOE_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(20));

    ioe1_.pinMode(TP_RST_IOE_PIN, OUTPUT);
    ioe1_.setDriveMode(TP_RST_IOE_PIN, M5IOE1_DRIVE_PUSHPULL);
    ioe1_.digitalWrite(TP_RST_IOE_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "Initialize touch IO (I2C)");
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    tp_io_config.scl_speed_hz = 100000;
    esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);

    ESP_LOGI(TAG, "Initialize touch controller FT5X06");
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = DISPLAY_WIDTH,
        .y_max = DISPLAY_HEIGHT,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = TOUCH_INT_GPIO,
        .levels = { .reset = 0, .interrupt = 0 },
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
        .user_data = this
    };
    esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &touch_);

    display_ = new SpiCustomDisplay(panel_io_, panel_, touch_, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                 DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                 DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                 {
                                     .text_font  = &font_puhui_30_4,
                                     .icon_font  = &font_awesome_30_4,
                                     .emoji_font = font_emoji_64_init(),
                                 });

    display_->on_click_dont_reming_ = [this]() {
        nvs_handle_t handle;
        if (nvs_open("app_config", NVS_READWRITE, &handle) == ESP_OK) {
            nvs_set_u8(handle, "dont_remind", 1);
            nvs_commit(handle);
            nvs_close(handle);
        }
    };
    display_->on_shutdown_ = [this]() {
        
    };
    display_->on_clear_network_ = [this]() {
    
    };
    // WiFi-only: no on_switch_network_, hide switch network button
    if (display_->setup_btn_sw_net_ != nullptr) {
        lv_obj_add_flag(display_->setup_btn_sw_net_, LV_OBJ_FLAG_HIDDEN);
    }
    display_->on_auto_sleep_changed_ = [this]() { /* PaperMono: no power_save_timer */ };
    display_->on_manual_sleep_ = [this]() { 
        
    };

    InitializeBacklight();
}

void PaperMonoBoard::InitializeBacklight()
{
    if (!pmic_ioe1_ready_) return;

    pmic_.pinMode(BL_PWM_GPIO, OUTPUT);

    /* Configure PWM block first, then switch pin to PWM output (some PMICs need this order) */
    m5pm1_err_t err = pmic_.setPwmFrequency(1000);
    ESP_LOGI(TAG, "backlight PWM setPwmFrequency(%d) success", 1000);

    err = pmic_.setPwmDuty(BL_PWM_CH, 60, false, true);
    ESP_LOGI(TAG, "backlight PWM setPwmDuty success: %d", 60);
 
    pmic_.pinMode(BL_PWM_GPIO, ANALOG);  /* GPIO3 -> PWM0 output */
    vTaskDelay(pdMS_TO_TICKS(10));       /* let mux settle */
    ESP_LOGI(TAG, "backlight PWM: %d Hz, %d%%", 1000, 60);
}

void PaperMonoBoard::InitializeButtons()
{
    user_button_.OnClick([this]() {
        Application::GetInstance().Schedule([] {
            auto& board = Board::GetInstance();
            auto display = board.GetDisplay();
            if (lvgl_port_lock(3000)) {
                lv_obj_t *active = lv_screen_active();
                if (display->scr_test_ != nullptr && active == display->scr_test_) {
                    lvgl_port_unlock();
                    display->NextScrTestPage();
                } else if (active == display->scr_main_) {
                    lvgl_port_unlock();
                    Application::GetInstance().ToggleChatState();
                } else {
                    lvgl_port_unlock();
                }
            }
        });
    });
    user_button_.OnDoubleClick([this]() {
        Application::GetInstance().Schedule([] {
            auto& board = Board::GetInstance();
            auto display = board.GetDisplay();
            if (display->scr_test_ == nullptr) return;
            if (lvgl_port_lock(3000)) {
                lv_obj_t *active = lv_screen_active();
                if (active == display->scr_main_) {
                    lv_screen_load(display->scr_test_);
                } else if (active == display->scr_test_) {
                    lv_screen_load(display->scr_main_);
                }
                lvgl_port_unlock();
                display->FullRefresh();
            }
        });
    });

    /* GPIO_KEY2 单击切换背光: 20 -> 40 -> 60 -> 80 -> 100 -> 0 -> ... */
    static const uint8_t BL_LEVELS[] = {20, 40, 60, 80, 100, 0};
    bl_key_.OnClick([this]() {
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateStarting) {
            ResetWifiConfiguration();
        }
        // app.ToggleChatState();
        if (!pmic_ioe1_ready_) return;
        bl_level_ = (bl_level_ + 1) % 6;
        uint8_t duty = BL_LEVELS[bl_level_];
        m5pm1_err_t err = pmic_.setPwmDuty(BL_PWM_CH, duty, false, true);
        if (err == M5PM1_OK) {
            ESP_LOGI(TAG, "backlight %d%%", duty);
        }
    });
}

bool PaperMonoBoard::IsGuidePageRequired()
{
    return false;
}

PaperMonoBoard::PaperMonoBoard() : user_button_(USER_BUTTON_GPIO, false, 2000, 400),
                                   bl_key_(GPIO_KEY2, false, 2000, 400)
{
    InitializeI2c();
    InitializePmicIoe();
    InitializeSpi();
    InitializeDisplay();
    InitializeButtons();
 
    if (IsGuidePageRequired() && display_) {
        lv_screen_load(display_->scr_startup_);
    } else if (display_) {
        lv_screen_load(display_->scr_main_);
    }
}

PaperMonoBoard::~PaperMonoBoard()
{
    if (display_) {
        delete display_;
    }
    if (pdm_codec_) {
        delete pdm_codec_;
    }
}

Display *PaperMonoBoard::GetDisplay()
{
    return display_;
}

AudioCodec *PaperMonoBoard::GetAudioCodec()
{
    if (!pdm_codec_) {
        /* PDM 电源使能 (M5IOE1_G12) */
        if (pmic_ioe1_ready_) {
            ioe1_.pinMode(PDM_PWR_EN_IOE_PIN, OUTPUT);
            ioe1_.setDriveMode(PDM_PWR_EN_IOE_PIN, M5IOE1_DRIVE_PUSHPULL);
            ioe1_.digitalWrite(PDM_PWR_EN_IOE_PIN, HIGH);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        pdm_codec_ = new NoAudioCodecPdmInputOnly(24000, 24000, PDM_CLK_PIN, PDM_DATA_PIN);
    }
    return pdm_codec_;
}

bool PaperMonoBoard::GetBatteryLevel(int &level, bool &charging, bool &discharging)
{
    if (!pmic_ioe1_ready_) {
        return false;
    }
    uint16_t vbat_mv = 0;
    m5pm1_err_t err = pmic_.readVbat(&vbat_mv);
    if (err != M5PM1_OK) {
        return false;
    }
    // m5pm1_pwr_src_t pwr_src = M5PM1_PWR_SRC_UNKNOWN;
    // err = pmic_.getPowerSource(&pwr_src);
    // if (err != M5PM1_OK) {
    //     return false;
    // }
    // charging = (pwr_src == M5PM1_PWR_SRC_5VIN || pwr_src == M5PM1_PWR_SRC_5VINOUT);
    // discharging = (pwr_src == M5PM1_PWR_SRC_BAT);
    charging = false;
    discharging = true;
    /* Li-ion: 3.2V empty, 4.2V full */
    const int v_min = 3200;
    const int v_max = 4200;
    if (vbat_mv <= v_min) {
        level = 0;
    } else if (vbat_mv >= v_max) {
        level = 100;
    } else {
        level = (int)((float)(vbat_mv - v_min) / (v_max - v_min) * 100.0f);
    }
    return true;
}

void PaperMonoBoard::ClearDisplay(uint8_t color)
{
    static uint8_t *buf = nullptr;
    static size_t buf_size = 0;
    if (!panel_) return;
    buf_size = EPD_RES_WIDTH * EPD_RES_HEIGHT;
    if (!buf) {
        buf = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
        if (!buf) {
            ESP_LOGE(TAG, "Failed to allocate buffer");
            return;
        }
    }
    memset(buf, color, buf_size);
    panel_gdem0397t81p_draw_bitmap_full(panel_, 0, 0, EPD_RES_WIDTH, EPD_RES_HEIGHT, buf);
}

DECLARE_BOARD(PaperMonoBoard);
