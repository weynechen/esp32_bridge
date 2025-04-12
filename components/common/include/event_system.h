#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace esp_framework {

/**
 * @brief 事件类型枚举
 */
enum class event_type {
    network_connected,     // 网络已连接
    network_disconnected,  // 网络已断开
    data_received,         // 数据已接收
    battery_low,           // 电池电量低
    battery_critical,      // 电池电量严重不足
    battery_normal,        // 电池电量正常
    charging_started,      // 充电开始
    charging_complete,     // 充电完成
    battery_temp_high,     // 电池温度过高
    battery_temp_normal,   // 电池温度正常
    device_error,          // 设备错误
    enter_deep_sleep       // 进入深度睡眠
};

/**
 * @brief 事件数据类型枚举
 */
enum class event_data_type {
    none,           // 无数据
    integer,        // 整数
    floating,       // 浮点数
    boolean,        // 布尔值
    string,         // 字符串
    binary          // 二进制数据
};

/**
 * @brief 事件数据结构
 */
struct event_data {
    event_type type;         // 事件类型
    event_data_type data_type; // 数据类型
    std::shared_ptr<uint8_t[]> data; // 数据指针，使用shared_ptr管理内存
    size_t data_size;        // 数据大小（仅对string和binary有效）
    
    // 构造函数
    event_data(event_type t, event_data_type dt = event_data_type::none, 
               std::shared_ptr<uint8_t[]> d = nullptr, size_t ds = 0) 
        : type(t), data_type(dt), data(d), data_size(ds) {}
    
    // 析构函数 - 现在可以使用默认析构函数，shared_ptr会自动管理内存
    ~event_data() = default;
    
    // 复制构造和赋值操作禁用
    event_data(const event_data&) = delete;
    event_data& operator=(const event_data&) = delete;
    
    // 移动构造和赋值
    event_data(event_data&& other) noexcept
        : type(other.type), data_type(other.data_type), 
          data(std::move(other.data)), data_size(other.data_size) {
        other.data_size = 0;
    }
    
    event_data& operator=(event_data&& other) noexcept {
        if (this != &other) {
            type = other.type;
            data_type = other.data_type;
            data = std::move(other.data);
            data_size = other.data_size;
            other.data_size = 0;
        }
        return *this;
    }
};

/**
 * @brief 事件监听器接口
 */
class event_listener {
public:
    /**
     * @brief 事件处理函数
     * @param event 事件数据
     */
    virtual void on_event(const event_data& event) = 0;
    
    /**
     * @brief 虚析构函数
     */
    virtual ~event_listener() = default;
};

/**
 * @brief 事件管理器（单例模式）
 */
class event_bus {
public:
    /**
     * @brief 获取事件管理器实例
     * @return 事件管理器引用
     */
    static event_bus& get_instance();
    
    /**
     * @brief 注册事件监听器
     * @param type 事件类型
     * @param listener 监听器智能指针
     */
    void subscribe(event_type type, std::shared_ptr<event_listener> listener);
    
    /**
     * @brief 取消监听器注册
     * @param type 事件类型
     * @param listener 监听器智能指针
     */
    void unsubscribe(event_type type, std::shared_ptr<event_listener> listener);
    
    /**
     * @brief 发布事件
     * @param event 事件数据
     */
    void publish(const event_data& event);
    
private:
    /**
     * @brief 构造函数（私有）
     */
    event_bus() = default;
    
    /**
     * @brief 析构函数（私有）
     */
    ~event_bus() = default;
    
    // 禁止复制和移动
    event_bus(const event_bus&) = delete;
    event_bus& operator=(const event_bus&) = delete;
    event_bus(event_bus&&) = delete;
    event_bus& operator=(event_bus&&) = delete;
    
    // 事件监听器映射表
    std::map<event_type, std::vector<std::weak_ptr<event_listener>>> listeners_;
};

} // namespace esp_framework 