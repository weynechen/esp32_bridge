#include <stdio.h>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "event_system.h"
#include "device.h"
#include "device_manager.h"
#include "network_module.h"
#include "battery_manager.h"
#include "pmu.h"

// 使用命名空间
using namespace esp_framework;

static const char* TAG = "Main";

// 系统状态监听器
class system_listener : public event_listener {
public:
    void on_event(const event_data& event) override {
        switch (event.type) {
            case event_type::network_connected:
                ESP_LOGI(TAG, "网络已连接");
                break;
                
            case event_type::network_disconnected:
                ESP_LOGI(TAG, "网络已断开");
                break;
                
            case event_type::data_received:
                if (event.data_type == event_data_type::string && event.data) {
                    ESP_LOGI(TAG, "接收到数据: %s", (const char*)event.data.get());
                } else if (event.data_type == event_data_type::binary && event.data) {
                    ESP_LOGI(TAG, "接收到二进制数据: %zu字节", event.data_size);
                }
                break;
                
            case event_type::battery_low:
                ESP_LOGW(TAG, "电池电量低");
                break;
                
            case event_type::battery_critical:
                ESP_LOGE(TAG, "电池电量严重不足");
                break;
                
            case event_type::battery_normal:
                ESP_LOGI(TAG, "电池电量正常");
                break;
                
            case event_type::battery_temp_high:
                ESP_LOGW(TAG, "电池温度过高");
                break;
                
            case event_type::battery_temp_normal:
                ESP_LOGI(TAG, "电池温度恢复正常");
                break;
                
            case event_type::enter_deep_sleep:
                ESP_LOGI(TAG, "系统准备进入深度睡眠");
                break;
                
            default:
                break;
        }
    }
};

// 主任务
void main_task(void* pvParameter) {
    // 添加一个简单的保护
    if (pvParameter == NULL) {
        ESP_LOGE(TAG, "主任务参数为空，退出任务");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "主任务启动 - 开始执行");
    
    // 获取设备管理器
    device_manager* dev_mgr = static_cast<device_manager*>(pvParameter);
    if (dev_mgr == NULL) {
        ESP_LOGE(TAG, "设备管理器指针转换失败，退出任务");
        vTaskDelete(NULL);
        return;
    }
        
    // 声明PMU指针
    pmu* power_mgr = nullptr;

    
    // 获取当前可用堆内存
    ESP_LOGI(TAG, "事件监听器创建前可用堆内存: %u字节", esp_get_free_heap_size());
    
    // 在try之前先尝试创建事件监听器，确保没有内存问题
    system_listener* raw_listener = new system_listener();
    if (raw_listener == NULL) {
        ESP_LOGE(TAG, "事件监听器原始对象创建失败，可能是内存不足");
        vTaskDelete(NULL);
        return;
    }
    
    
    try {
        // 创建系统事件监听器
        auto sys_listener = std::shared_ptr<system_listener>(raw_listener);

        event_bus::get_instance().subscribe(event_type::network_connected, sys_listener);
        event_bus::get_instance().subscribe(event_type::network_disconnected, sys_listener);
        event_bus::get_instance().subscribe(event_type::data_received, sys_listener);
        event_bus::get_instance().subscribe(event_type::battery_low, sys_listener);
        event_bus::get_instance().subscribe(event_type::battery_critical, sys_listener);
        event_bus::get_instance().subscribe(event_type::battery_normal, sys_listener);
        event_bus::get_instance().subscribe(event_type::battery_temp_high, sys_listener);
        event_bus::get_instance().subscribe(event_type::battery_temp_normal, sys_listener);
        event_bus::get_instance().subscribe(event_type::enter_deep_sleep, sys_listener);
        
        // 获取网络模块和电池管理器实例
        auto& net_module = network_module::get_instance();
        auto& batt_mgr = battery_manager::get_instance();
        
        // 连接WiFi
        const char* ssid = CONFIG_WIFI_SSID;
        const char* password = CONFIG_WIFI_PASSWORD;
        
        // 检查SSID和密码
        if (ssid == NULL || password == NULL) {
            ESP_LOGE(TAG, "WiFi配置错误: SSID或密码为空");
        } else {
            ESP_LOGI(TAG, "WiFi配置正确，准备连接WiFi: SSID=%s, 密码长度=%d", ssid, strlen(password));
            
            bool wifi_connected = net_module.connect_wifi(ssid, password);
            if (!wifi_connected) {
                ESP_LOGE(TAG, "WiFi连接失败，无法继续网络操作");
            } else {
                ESP_LOGI(TAG, "WiFi连接成功，准备连接TCP服务器");
                
                // 连接TCP服务器
                const char* server_ip = CONFIG_TCP_SERVER_IP;
                uint16_t server_port = CONFIG_TCP_SERVER_PORT;
                
                if (server_ip == NULL) {
                    ESP_LOGE(TAG, "TCP服务器IP配置错误");
                } else {
                    ESP_LOGI(TAG, "正在连接TCP服务器: %s:%u", server_ip, server_port);
                    
                    // 尝试连接多次
                    bool tcp_connected = false;
                    for (int retry = 0; retry < 3; retry++) {
                        ESP_LOGI(TAG, "尝试TCP连接，第%d次", retry + 1);
                        tcp_connected = net_module.connect_tcp(server_ip, server_port);
                        if (tcp_connected) {
                            ESP_LOGI(TAG, "已连接到TCP服务器");
                            
                            // 发送测试数据
                            const char* test_msg = "Hello from ESP32S3!";
                            std::vector<uint8_t>* data = new std::vector<uint8_t>(test_msg, test_msg + strlen(test_msg));
                            if (net_module.send_data(*data)) {
                                ESP_LOGI(TAG, "测试数据发送成功");
                            } else {
                                ESP_LOGE(TAG, "测试数据发送失败");
                            }
                            delete data;
                            break;
                        } else {
                            ESP_LOGW(TAG, "TCP服务器连接失败，重试 %d/3", retry + 1);
                            vTaskDelay(1000 / portTICK_PERIOD_MS);
                        }
                    }
                    
                    if (!tcp_connected) {
                        ESP_LOGE(TAG, "多次连接TCP服务器失败");
                    }
                }
            }
        }
        
        // 创建PMU并获取锁
        power_mgr = new pmu(*dev_mgr, CONFIG_POWER_SAVE_TIMEOUT);
        power_mgr->lock(); // 初始时锁定，防止系统立即进入低功耗
        
        // 主循环
        while (1) {
            // 处理网络事件
            net_module.loop();
            
            // 处理电池管理
            batt_mgr.loop();
            
            // 处理电源管理
            power_mgr->loop();
            
            
            // 每秒执行一次
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        
        // 此处实际上不会执行到，但为了代码完整性添加释放资源代码
        delete power_mgr;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "主任务异常: %s", e.what());
        // 释放power_mgr资源
        if (power_mgr != nullptr) {
            delete power_mgr;
        }
    } catch (...) {
        ESP_LOGE(TAG, "主任务发生未知异常");
        // 释放power_mgr资源
        if (power_mgr != nullptr) {
            delete power_mgr;
        }
    }
    
    // 实际上不应该到达这里
    ESP_LOGE(TAG, "主任务异常退出");
    // 释放传入的设备管理器资源
    if (dev_mgr != nullptr) {
        delete dev_mgr;
    }
    vTaskDelete(NULL);
}

extern "C" void app_main(void)
{
    // 设置日志级别
    esp_log_level_set("*", ESP_LOG_INFO);

    
    ESP_LOGI(TAG, "系统启动");
    
    ESP_LOGI(TAG, "空闲堆内存: %u字节", esp_get_free_heap_size());
    
    // 创建并初始化设备管理器
    device_manager* dev_mgr = new device_manager();
    
    // 创建电池设备
    auto batt_dev = std::shared_ptr<battery_device>(new battery_device());
    if (!batt_dev) {
        ESP_LOGE(TAG, "电池设备创建失败");
        delete dev_mgr;
        return;
    }
    
    dev_mgr->register_device(batt_dev);
    
    // 初始化所有设备
    dev_mgr->init_all();
    
    // 初始化电池管理器
    battery_manager::get_instance().init(batt_dev);
    
    // 创建主任务前的堆内存
    ESP_LOGI(TAG, "创建主任务前空闲堆内存: %u字节", esp_get_free_heap_size());
    
    // 创建主任务
    BaseType_t ret = xTaskCreate(main_task, "main_task", 4096, dev_mgr, 5, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "主任务创建失败，错误码: %d", ret);
        delete dev_mgr;
        return;
    }
    
    // 创建主任务后的堆内存
    ESP_LOGI(TAG, "创建主任务后空闲堆内存: %u字节", esp_get_free_heap_size());
    
    // app_main函数可以返回，但主任务会继续运行
} 