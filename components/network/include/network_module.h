#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "esp_log.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "event_system.h"

namespace esp_framework {

/**
 * @brief 网络模块类
 * 
 * 负责WiFi连接和TCP通信
 */
class network_module : public event_listener {
public:
    /**
     * @brief 获取网络模块单例实例
     * @return 网络模块引用
     */
    static network_module& get_instance();
    
    /**
     * @brief 连接WiFi网络
     * @param ssid WiFi名称
     * @param password WiFi密码
     * @return 连接成功返回true，失败返回false
     */
    bool connect_wifi(const std::string& ssid, const std::string& password);
    
    /**
     * @brief 断开WiFi连接
     */
    void disconnect_wifi();
    
    /**
     * @brief 连接TCP服务器
     * @param host 服务器地址
     * @param port 服务器端口
     * @return 连接成功返回true，失败返回false
     */
    bool connect_tcp(const std::string& host, uint16_t port);
    
    /**
     * @brief 断开TCP连接
     */
    void disconnect_tcp();
    
    /**
     * @brief 发送数据到TCP服务器
     * @param data 要发送的数据
     * @return 发送成功返回true，失败返回false
     */
    bool send_data(const std::vector<uint8_t>& data);
    
    /**
     * @brief 设置数据接收回调函数
     * @param callback 接收到数据时的回调函数
     */
    void set_data_callback(std::function<void(const std::vector<uint8_t>&)> callback);
    
    /**
     * @brief 处理网络事件，必须定期调用
     */
    void loop();
    
    /**
     * @brief 事件处理函数
     * @param event 事件数据
     */
    void on_event(const event_data& event) override;
    
    /**
     * @brief 检查是否连接到WiFi
     * @return 已连接返回true，未连接返回false
     */
    bool is_wifi_connected() const;
    
    /**
     * @brief 检查是否连接到TCP服务器
     * @return 已连接返回true，未连接返回false
     */
    bool is_tcp_connected() const;
    
private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    network_module();
    
    /**
     * @brief 私有析构函数（单例模式）
     */
    ~network_module();
    
    // 禁止拷贝和移动
    network_module(const network_module&) = delete;
    network_module& operator=(const network_module&) = delete;
    network_module(network_module&&) = delete;
    network_module& operator=(network_module&&) = delete;
    
    // WiFi事件处理
    static void wifi_event_handler(void* arg, esp_event_base_t event_base, 
                                  int32_t event_id, void* event_data);
    
    // TCP接收任务
    static void tcp_receive_task(void* pvParameters);
    
    // 私有成员变量
    std::string ssid_;                // WiFi名称
    std::string password_;            // WiFi密码
    std::string server_host_;         // 服务器地址
    uint16_t server_port_;            // 服务器端口
    int sock_;                        // Socket描述符
    bool wifi_connected_;             // WiFi连接状态
    bool tcp_connected_;              // TCP连接状态
    TaskHandle_t task_handle_;        // TCP接收任务句柄
    std::function<void(const std::vector<uint8_t>&)> data_callback_; // 数据接收回调
};

} // namespace esp_framework 