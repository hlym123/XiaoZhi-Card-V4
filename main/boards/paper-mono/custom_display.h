#ifndef CUSTOM_DISPLAY_H
#define CUSTOM_DISPLAY_H

#include "display/display.h"
#include "lcd_display.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_touch.h>
#include <font_emoji.h>

#include <atomic>

class CustomDisplay : public Display {
protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    esp_lcd_touch_handle_t touch_ = nullptr;

    lv_draw_buf_t draw_buf_;
    lv_obj_t* status_bar_ = nullptr;
    // lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    lv_obj_t* preview_image_ = nullptr;

    const lv_font_t *font_18_ = nullptr;
    const lv_font_t *font_20_ = nullptr;
    const lv_font_t *font_22_ = nullptr;
    const lv_font_t *font_24_ = nullptr;
    const lv_font_t *font_26_ = nullptr;
    const lv_font_t *font_32_ = nullptr;
    const lv_font_t *font_34_ = nullptr;
    const lv_font_t *font_48_ = nullptr;

    DisplayFonts fonts_;
    ThemeColors current_theme_;

    void GuidePageUI();
    void SetupUI();
    virtual void NextScrTestPage() override;
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

protected:
    // 添加protected构造函数
    CustomDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, 
               esp_lcd_touch_handle_t touch, 
               DisplayFonts fonts, int width, int height);
    
public:
    ~CustomDisplay();
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetIcon(const char* icon) override;
    virtual void SetPreviewImage(const lv_img_dsc_t* img_dsc) override;
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    virtual void SetChatMessage(const char* role, const char* content) override; 
#endif  

    // Add theme switching function
    virtual void SetTheme(const std::string& theme_name) override;
};

// SPI EPD 显示器
class SpiCustomDisplay : public CustomDisplay {
public:
    SpiCustomDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  esp_lcd_touch_handle_t touch,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

#endif // CUSTOM_DISPLAY_H
