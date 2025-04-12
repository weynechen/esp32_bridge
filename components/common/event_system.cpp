#include "include/event_system.h"
#include "esp_log.h"
#include <algorithm>

static const char* TAG = "EventBus";

namespace esp_framework {

// 获取事件管理器实例
event_bus& event_bus::get_instance() {
    static event_bus instance;
    return instance;
}

// 注册事件监听器
void event_bus::subscribe(event_type type, std::shared_ptr<event_listener> listener) {
    if (!listener) {
        ESP_LOGE(TAG, "尝试注册空监听器");
        return;
    }
    
    // 检查是否已注册
    auto& listeners = listeners_[type];
    
    // 清理过期的weak_ptr
    listeners.erase(
        std::remove_if(listeners.begin(), listeners.end(),
                      [](const std::weak_ptr<event_listener>& wp) {
                          return wp.expired();
                      }),
        listeners.end()
    );
    
    // 检查是否已存在
    auto it = std::find_if(listeners.begin(), listeners.end(),
                          [&listener](const std::weak_ptr<event_listener>& wp) {
                              auto sp = wp.lock();
                              return sp && sp == listener;
                          });
    
    if (it != listeners.end()) {
        ESP_LOGW(TAG, "监听器已存在，跳过注册");
        return;
    }
    
    // 添加监听器
    listeners.push_back(listener);
    ESP_LOGI(TAG, "已注册事件监听器: %d", static_cast<int>(type));
}

// 取消监听器注册
void event_bus::unsubscribe(event_type type, std::shared_ptr<event_listener> listener) {
    if (!listener) {
        ESP_LOGE(TAG, "尝试取消注册空监听器");
        return;
    }
    
    auto it = listeners_.find(type);
    if (it == listeners_.end()) {
        ESP_LOGW(TAG, "事件类型 %d 无监听器", static_cast<int>(type));
        return;
    }
    
    auto& listeners = it->second;
    
    // 移除指定监听器
    listeners.erase(
        std::remove_if(listeners.begin(), listeners.end(),
                      [&listener](const std::weak_ptr<event_listener>& wp) {
                          auto sp = wp.lock();
                          return !sp || sp == listener;
                      }),
        listeners.end()
    );
    
    ESP_LOGI(TAG, "已取消注册事件监听器: %d", static_cast<int>(type));
}

// 发布事件
void event_bus::publish(const event_data& event) {
    auto it = listeners_.find(event.type);
    if (it == listeners_.end()) {
        ESP_LOGD(TAG, "事件类型 %d 无监听器，跳过发布", static_cast<int>(event.type));
        return;
    }
    
    auto& listeners = it->second;
    
    // 清理过期的weak_ptr
    listeners.erase(
        std::remove_if(listeners.begin(), listeners.end(),
                      [](const std::weak_ptr<event_listener>& wp) {
                          return wp.expired();
                      }),
        listeners.end()
    );
    
    ESP_LOGI(TAG, "发布事件: %d, 监听器数: %d", 
             static_cast<int>(event.type), listeners.size());
    
    // 通知所有监听器
    for (auto& wp : listeners) {
        auto sp = wp.lock();
        if (sp) {
            sp->on_event(event);
        }
    }
}

} // namespace esp_framework 