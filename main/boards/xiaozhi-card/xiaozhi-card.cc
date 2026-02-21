#include "dual_network_board.h"
#include "audio/codecs/es8311_audio_codec.h"
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
#include "driver/sdspi_host.h" 
#include "mcp_server.h"
#include "assets/lang_config.h"
#include "display/epd_display.h"
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "power_save_timer.h"
#include "font_emoji.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "led_strip.h"
#include "bq27220.h"
#include "aw32001.h"
#include "esp_epd_gdey027t91.h"
#include <esp_lvgl_port.h>
#include "esp_lcd_touch_ft5x06.h"
// #include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"


static const char *TAG = "XiaoZhi-Card Board";

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);


class MovingAverageFilter {
public:
    MovingAverageFilter(int size)
        : size_(size), buffer_(new float[size]), index_(0), count_(0), sum_(0), first_init_(true) 
    {
        memset(buffer_, 0, size * sizeof(float));
    }

    ~MovingAverageFilter() {
        delete[] buffer_;
    }

    float update(float value) {
        if (first_init_) {
            // 首次初始化直接填满 buffer
            for (int i = 0; i < size_; i++) buffer_[i] = value;
            sum_ = value * size_;
            count_ = size_;
            index_ = 0;
            first_init_ = false;
            return value;
        }

        // 移动平均更新
        sum_ -= buffer_[index_];
        buffer_[index_] = value;
        sum_ += value;

        index_ = (index_ + 1) % size_;
        if (count_ < size_) count_++;

        return sum_ / count_;
    }

private:
    int size_;
    float* buffer_;
    int index_;
    int count_;
    float sum_;
    bool first_init_;
};

class XiaozhiCardBoard : public DualNetworkBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;        // I2C 
    Button user_button_;                     // 用户按键
    SpiEpdDisplay *display_ = nullptr;       // 显示屏
    Aw32001 *charger_ = nullptr;             // 充电管理
    Bq27220 *guage_ = nullptr;               // 电量计
    led_strip_handle_t led_strip_ = nullptr; // 底座指示灯 

    // Display and touch handles
    esp_lcd_panel_io_handle_t panel_io_ = nullptr; 
    esp_lcd_panel_handle_t panel_ = nullptr;
    esp_lcd_touch_handle_t touch_ = nullptr;

    // Private methods
    void InitializeI2c();            // I2C (AW32001，BQ27220，触摸屏)
    void InitializeSpi();            // SPI (显示屏，SD 卡)
    void InitializeCharger();        // 充电 AW32001
    void InitializeGuage();          // 电量 BQ27220 
    void InitializeDisplay();        // 显示屏
    void InitializeButtons();        // 按键
    void InitializeIndicator();      // 底座指示灯   
    void InitializeTools();          // 

    virtual Display *GetDisplay() override;

public:
    XiaozhiCardBoard();
    ~XiaozhiCardBoard();
    void SetIndicator(uint8_t r, uint8_t g, uint8_t b); // 设置（底座）指示灯 
    virtual AudioCodec *GetAudioCodec() override;
    virtual bool GetBatteryLevel(int &level, bool &charging, bool &discharging) override;
};

void XiaozhiCardBoard::InitializeI2c()
{
    ESP_LOGI(TAG, "Initialize I2C peripheral");
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = SYS_I2C_PIN_SDA,
        .scl_io_num = SYS_I2C_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 0,
        },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
}

void XiaozhiCardBoard::InitializeSpi()
{
    ESP_LOGI(TAG, "Initialize SPI bus");

    spi_bus_config_t bus_cfg = {};
    bus_cfg.sclk_io_num = EPD_PIN_SCK;
    bus_cfg.mosi_io_num = EPD_PIN_MOSI;
    bus_cfg.miso_io_num = EPD_PIN_MISO;
    bus_cfg.quadwp_io_num = GPIO_NUM_NC;
    bus_cfg.quadhd_io_num = GPIO_NUM_NC;
    bus_cfg.max_transfer_sz = EPD_RES_HEIGHT * EPD_RES_WIDTH; // / 8 + 1;   
    ESP_ERROR_CHECK(spi_bus_initialize(EPD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));
}

void XiaozhiCardBoard::InitializeCharger()
{
    ESP_LOGI(TAG, "Init Charger AW32001");

    charger_ = new Aw32001(i2c_bus_, I2C_ADDR_AW32001);
    charger_->SetShippingMode(false);               // 关闭运输模式 
    charger_->SetNtcFunction(false);                // 未使用 NTC
    charger_->SetDischargeCurrent(2800);            // 最大放电电流 2800mA
    charger_->SetChargeCurrent(260);                // 最大充电电流 260mA
    charger_->SetChargeVoltage(4200);               // 满电电压 4.2V
    charger_->SetPreChargeCurrent(31);              // 预充电电流 31mA
    charger_->SetPrechargeToFastchargeThreshold(0); // 
    charger_->SetCharge(true);                      // 开启充电 
}

void XiaozhiCardBoard::InitializeGuage()
{
    ESP_LOGI(TAG, "Init Gauge BQ27220");

    guage_ = new Bq27220(i2c_bus_, I2C_ADDR_BQ27220);
}

void XiaozhiCardBoard::InitializeDisplay()
{
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.dc_gpio_num = EPD_PIN_DC;
    io_cfg.cs_gpio_num = EPD_PIN_CS;
    io_cfg.pclk_hz = 40 * 1000 * 1000; // 40MHz
    io_cfg.lcd_cmd_bits = 8;    
    io_cfg.lcd_param_bits = 8;  
    io_cfg.spi_mode = 0;
    io_cfg.trans_queue_depth = 8;  
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EPD_SPI_HOST, &io_cfg, &panel_io_));

    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.reset_gpio_num = EPD_PIN_RST;
    panel_cfg.rgb_endian = LCD_RGB_ENDIAN_BGR;
    panel_cfg.bits_per_pixel = 1;
    ESP_LOGI(TAG, "Install gdey027t91 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_gdey027t91(panel_io_, &panel_cfg, &panel_));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
    // // TODO: 这里等待耗时，需要优化  
    // ClearDisplay(0x00);
    // ClearDisplay(0xFF);

    // Initialize touch panel
    ESP_LOGI(TAG, "Initialize touch IO (I2C)");
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    tp_io_config.scl_speed_hz = 100000;
    esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);

    ESP_LOGI(TAG, "Initialize touch controller FT5X06");
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = DISPLAY_WIDTH,
        .y_max = DISPLAY_HEIGHT,
        .rst_gpio_num = GPIO_NUM_NC, // Shared with LCD reset
        .int_gpio_num = TOUCH_INT_GPIO,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
        .user_data = this 
    };
    esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &touch_);
   
    display_ = new SpiEpdDisplay(panel_io_, panel_, touch_, DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                              DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, 
                              DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY, 
                              {
                                  .text_font  = &font_puhui_16_4,
                                  .icon_font  = &font_awesome_16_4,
                                  .emoji_font = font_emoji_64_init(),
                              });
}

void XiaozhiCardBoard::InitializeButtons()
{
    user_button_.OnClick([this]() {
        auto& app = Application::GetInstance();
        app.ToggleChatState();
    });
}

void XiaozhiCardBoard::InitializeIndicator()
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {};
    rmt_config.clk_src = RMT_CLK_SRC_DEFAULT, 
    rmt_config.resolution_hz = 10 * 1000 * 1000;
    rmt_config.flags.with_dma = false;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_));
    led_strip_clear(led_strip_);

    SetIndicator(0, 0, 50);
}

void XiaozhiCardBoard::SetIndicator(uint8_t r, uint8_t g, uint8_t b)
{
    if (led_strip_ != nullptr) {
        led_strip_set_pixel(led_strip_, 0, r, g, b);
        led_strip_refresh(led_strip_);
    }
}

// 物联网初始化，添加对 AI 可见设备
void XiaozhiCardBoard::InitializeTools() 
{
    // TODO:
    // 切换网络
    // 重置 Wi-Fi
    // 底座灯设置
    // 底座 Grove 口
}

XiaozhiCardBoard::XiaozhiCardBoard() : DualNetworkBoard(ML307R_PIN_TX, ML307R_PIN_RX, ML307R_PIN_DTR),
         user_button_(USER_BUTTON_GPIO, false, 2000, 400) // 双击间隔为 400ms 内 
{
    // Initialize hardware components
    InitializeI2c();
    InitializeCharger();
    InitializeGuage();
    InitializeSpi();
    InitializeDisplay(); 
    InitializeButtons();
    InitializeIndicator();
    InitializeTools();
}

XiaozhiCardBoard::~XiaozhiCardBoard()
{
    if (charger_) {
        delete charger_;
    }
    if (guage_) {
        delete guage_;
    }
    if (display_) {
        delete display_;
    }
}

Display *XiaozhiCardBoard::GetDisplay()
{
    return display_;
}

bool XiaozhiCardBoard::GetBatteryLevel(int &level, bool &charging, bool &discharging)
{
    static uint8_t countdown = 10;
    static char text_tip[64];
    static float bat_vol;
    static bool last_charging = false;
    static MovingAverageFilter bat_filter(60); // 60 点滑动平均
    float raw_level;
    static int last_level = 0;

    /* 读取电池电压 */
    bat_vol = guage_->getVolt(VOLT_MODE::VOLT) / 1000.0f;
    if (bat_vol >= BAT_VOL_FULL) {
        raw_level = 100;
    } else {
        raw_level = (bat_vol - BAT_VOL_EMPTY) / (BAT_VOL_FULL - BAT_VOL_EMPTY) * 100;
        if (raw_level < 0) {
            raw_level = 0;
        } 
    }

    float filtered_level = bat_filter.update(raw_level);
    level = static_cast<int>(filtered_level + 0.5f); // 平均值取整
    if (last_level != level) { // 状态变化时才更新显示 
        last_level = level;
    }

    charging = (charger_->GetChargeState() != 0);
    discharging = !charging;

    return true;
}

// void XiaozhiCardBoard::ClearDisplay(uint8_t color)
// {
    // static uint8_t *buf = nullptr;
    // static size_t buf_size = 0;
    // if (panel_ == nullptr) {
    //     printf("ClearDisplay: panel_ is null!\n");
    //     return;
    // }
    // buf_size = EPD_RES_WIDTH * EPD_RES_HEIGHT;
    // if (buf == nullptr) {
    //     buf = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    //     if (!buf) {
    //         printf("ClearDisplay: failed to allocate %u bytes in SPIRAM!\n", (unsigned)buf_size);
    //         return;
    //     }
    // }
    // memset(buf, color, buf_size);
    // panel_gdey027t91_draw_bitmap_full(panel_, 0, 0, EPD_RES_WIDTH, EPD_RES_HEIGHT, buf);
// }

AudioCodec *XiaozhiCardBoard::GetAudioCodec()
{
    static Es8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                        AUDIO_I2S_PIN_MCLK, AUDIO_I2S_PIN_BCLK, AUDIO_I2S_PIN_WS, AUDIO_I2S_PIN_DOUT,
                                        AUDIO_I2S_PIN_DIN, AUDIO_PIN_PA, AUDIO_CODEC_ES8311_ADDR);
    return &audio_codec;
}

DECLARE_BOARD(XiaozhiCardBoard);
 