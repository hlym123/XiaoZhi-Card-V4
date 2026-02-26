#ifndef DISPLAY_H
#define DISPLAY_H

#include <lvgl.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_pm.h>

#include <string>
#include <chrono>
#include <functional>

struct DisplayFonts {
    const lv_font_t* text_font = nullptr;
    const lv_font_t* icon_font = nullptr;
    const lv_font_t* emoji_font = nullptr;
};

class Display {
public:
    Display();
    virtual ~Display();

    virtual void SetStatus(const char* status);
    virtual void ShowNotification(const char* notification, int duration_ms = 3000);
    virtual void ShowNotification(const std::string &notification, int duration_ms = 3000);
    virtual void SetEmotion(const char* emotion);
    virtual void SetChatMessage(const char* role, const char* content);
    virtual void SetIcon(const char* icon);
    virtual void SetPreviewImage(const lv_img_dsc_t* image);
    virtual void SetTheme(const std::string& theme_name);
    virtual std::string GetTheme() { return current_theme_name_; }
    virtual void UpdateStatusBar(bool update_all = false);
    virtual void SetPowerSaveMode(bool on);

    inline int width() const { return width_; }
    inline int height() const { return height_; }

    /* Add for XiaoZhi-Card Board */
    void UpdateVolume(int volume);               // 音量显示
    void FullRefresh();                          // 全刷
    void SetBtnChatMessage(const char* content); // 对话按钮内容
    void SetBtnNewChatVisible(bool visible);     // 新对话按钮可见性
    void SetContentVisible(bool visible);        // 主页面内容区域可见性
    virtual void NextScrTestPage();              // scr_test 下一页 (PaperMono)

    // 引导页面 
    lv_obj_t *scr_startup_ = nullptr;
    lv_obj_t *btn_startup_intro_ = nullptr;
    lv_obj_t *btn_startup_return_ = nullptr;
    lv_obj_t *scr_page1_ = nullptr;
    lv_obj_t *btn_page1_next_ = nullptr;
    lv_obj_t *scr_page2_ = nullptr;
    lv_obj_t *btn_page2_next_ = nullptr;
    lv_obj_t *scr_page3_ = nullptr;
    lv_obj_t *btn_page3_next_ = nullptr;
    lv_obj_t *scr_page4_ = nullptr;
    lv_obj_t *btn_page4_next_ = nullptr;
    lv_obj_t *scr_page5_ = nullptr;
    lv_obj_t *btn_page5_next_ = nullptr;
    // 提示页面 
    lv_obj_t *scr_tip_ = nullptr;                
    lv_obj_t *scr_tip_label_title_ = nullptr;
    lv_obj_t *scr_tip_label_ = nullptr;
    // 主页面
    lv_obj_t *scr_main_ = nullptr;
    // 测试页 (PaperMono develop 青锋渡)
    lv_obj_t *scr_test_ = nullptr;
    lv_obj_t *scr_test_label_title_ = nullptr;
    lv_obj_t *scr_test_label_author_ = nullptr;
    lv_obj_t *scr_test_label_content_ = nullptr;
    lv_obj_t *main_btn_chat_ = nullptr;            // 对话状态切换 
    lv_obj_t *main_btn_chat_label_ = nullptr;      //
    lv_obj_t *main_btn_new_chat_ = nullptr;        // 新对话
    lv_obj_t *main_btn_confirm_upgrade_ = nullptr; // 确认升级
    lv_obj_t *main_btn_skip_upgrade_ = nullptr;    // 暂不升级 
    // 设置页面  
    lv_obj_t *scr_setup_ = nullptr;
    lv_obj_t *label_volume_ = nullptr;
    lv_obj_t *setup_btn_plus_ = nullptr;
    lv_obj_t *setup_btn_minus_ = nullptr;
    lv_obj_t *setup_btn_sleep_ = nullptr;
    lv_obj_t *setup_btn_shutdown_ = nullptr;
    lv_obj_t *setup_btn_return_ = nullptr;
    lv_obj_t *setup_label_battery_ = nullptr;
    lv_obj_t *setup_btn_auto_sleep_ = nullptr;
    lv_obj_t *setup_label_auto_sleep_ = nullptr;
    lv_obj_t *setup_btn_clear_net_ = nullptr;
    lv_obj_t *setup_btn_cn_confirm_ = nullptr;
    lv_obj_t *setup_btn_cn_cancel_ = nullptr;
    lv_obj_t *setup_btn_sw_net_ = nullptr;
    lv_obj_t *setup_btn_confirm_ = nullptr;
    lv_obj_t *setup_btn_cancel_ = nullptr;
    lv_obj_t *setup_label_net_ = nullptr;
    // 关机页面
    lv_obj_t *scr_shutdown_ = nullptr;
    // 休眠页面 
    lv_obj_t *scr_sleep_ = nullptr;

    // 回调函数 
    std::function<void()> on_shutdown_ = nullptr;
    std::function<void()> on_click_dont_reming_ = nullptr;
    std::function<void()> on_switch_network_ = nullptr;
    std::function<void()> on_clear_network_ = nullptr;
    std::function<void()> on_manual_sleep_ = nullptr;
    std::function<void()> on_auto_sleep_changed_ = nullptr;

    //
    lv_obj_t *content_ = nullptr;            // 主页面内容区域 
    lv_obj_t *chat_message_label_ = nullptr; // 对话消息标签 
protected:
    int width_ = 0;
    int height_ = 0;
    
    esp_pm_lock_handle_t pm_lock_ = nullptr;
    lv_display_t *display_ = nullptr;
    lv_indev_t *indev_ = nullptr;

    lv_obj_t *emotion_label_ = nullptr;
    lv_obj_t *network_label_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *notification_label_ = nullptr;
    lv_obj_t *mute_label_ = nullptr;
    lv_obj_t *battery_label_ = nullptr;
    // lv_obj_t *chat_message_label_ = nullptr;
    lv_obj_t *low_battery_popup_ = nullptr;
    lv_obj_t *low_battery_label_ = nullptr;
    
    const char* battery_icon_ = nullptr;
    const char* network_icon_ = nullptr;
    bool muted_ = false;
    std::string current_theme_name_;

    std::chrono::system_clock::time_point last_status_update_time_;
    esp_timer_handle_t notification_timer_ = nullptr;

    friend class DisplayLockGuard;
    virtual bool Lock(int timeout_ms = 0) = 0;
    virtual void Unlock() = 0;
};


class DisplayLockGuard {
public:
    DisplayLockGuard(Display *display) : display_(display) {
        if (!display_->Lock(30000)) {
            ESP_LOGE("Display", "Failed to lock display");
        }
    }
    ~DisplayLockGuard() {
        display_->Unlock();
    }

private:
    Display *display_;
};

class NoDisplay : public Display {
private:
    virtual bool Lock(int timeout_ms = 0) override {
        return true;
    }
    virtual void Unlock() override {}
};

#endif
