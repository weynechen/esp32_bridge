#include "pmu.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "PMU";

// 配置参数
#define PMU_DEFAULT_IDLE_TIMEOUT CONFIG_POWER_SAVE_TIMEOUT

namespace esp_framework {

pmu::pmu(device_manager& dev_mgr, int idle_timeout_seconds)
    : dev_mgr_(dev_mgr),
      is_locked_(false),
      idle_timeout_(std::chrono::seconds(idle_timeout_seconds == 0 ? 
                                       PMU_DEFAULT_IDLE_TIMEOUT : 
                                       idle_timeout_seconds)),
      is_suspended_(false) {
    
    // 记录初始解锁时间
    last_unlock_time_ = std::chrono::steady_clock::now();
    
    ESP_LOGI(TAG, "电源管理器初始化完成，空闲超时时间: %d秒", 
            static_cast<int>(idle_timeout_.count()));
}

pmu::~pmu() {
    // 确保系统不会处于挂起状态
    if (is_suspended_) {
        dev_mgr_.resume_all();
    }
    
    ESP_LOGI(TAG, "电源管理器已销毁");
}

void pmu::lock() {
    if (!is_locked_) {
        is_locked_ = true;
        
        // 如果系统已挂起，则恢复
        if (is_suspended_) {
            dev_mgr_.resume_all();
            is_suspended_ = false;
            ESP_LOGI(TAG, "系统已恢复（从低功耗模式）");
        }
        
        ESP_LOGI(TAG, "电源管理器已锁定，系统将保持活跃");
    }
}

void pmu::unlock() {
    if (is_locked_) {
        is_locked_ = false;
        last_unlock_time_ = std::chrono::steady_clock::now();
        ESP_LOGI(TAG, "电源管理器已解锁，系统可能进入低功耗模式");
    }
}

bool pmu::is_locked() const {
    return is_locked_;
}

void pmu::enter_deep_sleep(uint32_t sleep_time_ms) {
    ESP_LOGI(TAG, "准备进入深度睡眠模式, 睡眠时间: %u毫秒", sleep_time_ms);
    
    // 发布进入深度睡眠事件，允许其他模块做准备
    event_data event(event_type::enter_deep_sleep);
    event_bus::get_instance().publish(event);
    
    // 给其他模块一些时间响应事件
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 配置唤醒源
    if (sleep_time_ms > 0) {
        ESP_LOGI(TAG, "配置定时器唤醒源，%u毫秒后唤醒", sleep_time_ms);
        esp_sleep_enable_timer_wakeup(sleep_time_ms * 1000); // 转换为微秒
    }
    
    // 进入深度睡眠
    ESP_LOGI(TAG, "正在进入深度睡眠模式...");
    esp_deep_sleep_start();
    
    // 此行不会执行，因为深度睡眠会重启系统
}

void pmu::loop() {
    if (is_locked_ || is_suspended_) {
        return;  // 被锁定或已挂起，无需处理
    }
    
    // 检查是否超时
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_unlock_time_);
    
    if (elapsed >= idle_timeout_) {
        // 超时，进入低功耗模式
        if (!is_suspended_) {
            ESP_LOGI(TAG, "空闲超时(%lld秒)，进入低功耗模式", 
                    static_cast<long long>(elapsed.count()));
            dev_mgr_.suspend_all();
            is_suspended_ = true;
        }
    }
}

void pmu::set_idle_timeout(int seconds) {
    if (seconds <= 0) {
        ESP_LOGW(TAG, "无效的空闲超时时间: %d秒，使用默认值", seconds);
        idle_timeout_ = std::chrono::seconds(PMU_DEFAULT_IDLE_TIMEOUT);
    } else {
        idle_timeout_ = std::chrono::seconds(seconds);
    }
    
    ESP_LOGI(TAG, "空闲超时时间设置为: %d秒", static_cast<int>(idle_timeout_.count()));
}

int pmu::get_idle_timeout() const {
    return static_cast<int>(idle_timeout_.count());
}

} // namespace esp_framework 