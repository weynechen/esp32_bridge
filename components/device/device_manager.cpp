#include "include/device_manager.h"
#include <algorithm>
#include <cstring>
#include "esp_log.h"

static const char* TAG = "DeviceManager";

namespace esp_framework {

void device_manager::register_device(std::shared_ptr<device> dev) {
    if (!dev) {
        ESP_LOGE(TAG, "尝试注册空设备指针");
        return;
    }
    
    // 检查是否已注册
    auto it = std::find_if(devices_.begin(), devices_.end(), 
                           [dev](const std::shared_ptr<device>& existing) {
                               return strcmp(existing->name(), dev->name()) == 0;
                           });
    
    if (it != devices_.end()) {
        ESP_LOGW(TAG, "设备 %s 已存在，跳过注册", dev->name());
        return;
    }
    
    devices_.push_back(dev);
    ESP_LOGI(TAG, "设备 %s 已注册", dev->name());
}

void device_manager::init_all() {
    for (auto& dev : devices_) {
        int ret = dev->init();
        if (ret != 0) {
            ESP_LOGE(TAG, "设备 %s 初始化失败: %d", dev->name(), ret);
        } else {
            ESP_LOGI(TAG, "设备 %s 初始化成功", dev->name());
        }
    }
}

void device_manager::deinit_all() {
    for (auto& dev : devices_) {
        int ret = dev->deinit();
        if (ret != 0) {
            ESP_LOGE(TAG, "设备 %s 反初始化失败: %d", dev->name(), ret);
        } else {
            ESP_LOGI(TAG, "设备 %s 反初始化成功", dev->name());
        }
    }
}

void device_manager::suspend_all() {
    for (auto& dev : devices_) {
        int ret = dev->suspend();
        if (ret != 0) {
            ESP_LOGE(TAG, "设备 %s 挂起失败: %d", dev->name(), ret);
        } else {
            ESP_LOGI(TAG, "设备 %s 已挂起", dev->name());
        }
    }
}

void device_manager::resume_all() {
    for (auto& dev : devices_) {
        int ret = dev->resume();
        if (ret != 0) {
            ESP_LOGE(TAG, "设备 %s 恢复失败: %d", dev->name(), ret);
        } else {
            ESP_LOGI(TAG, "设备 %s 已恢复", dev->name());
        }
    }
}

std::shared_ptr<device> device_manager::get_device_by_name(const std::string& name) {
    auto it = std::find_if(devices_.begin(), devices_.end(), 
                           [&name](const std::shared_ptr<device>& dev) {
                               return name == dev->name();
                           });
    
    if (it != devices_.end()) {
        return *it;
    }
    
    ESP_LOGW(TAG, "未找到设备: %s", name.c_str());
    return nullptr;
}

} // namespace esp_framework 