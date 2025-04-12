#pragma once

#include <memory>
#include <atomic>
#include <mutex>
#include "esp_log.h"
#include "device.h"
#include "event_system.h"

namespace esp_framework {

/**
 * @brief 电池状态枚举
 */
enum class battery_state {
    critical,   // 电量严重不足（<10%）
    low,        // 电量不足（<20%）
    normal,     // 正常电量（20%-80%）
    high,       // 高电量（>80%）
    full,       // 满电（100%）
    charging,   // 充电中
    error       // 电池错误
};

/**
 * @brief 充电状态枚举
 */
enum class charging_state {
    not_charging,    // 未充电
    fast_charging,   // 快速充电
    slow_charging,   // 慢速充电
    trickle_charging,// 涓流充电
    complete,        // 充电完成
    error            // 充电错误
};

// 前向声明
class pmu;

/**
 * @brief 电池传感器设备类
 */
class battery_device : public device {
public:
    /**
     * @brief 获取设备名称
     * @return 设备名称字符串
     */
    const char* name() const override { return "battery_device"; }
    
    /**
     * @brief 初始化设备
     * @return 成功返回0，失败返回负值
     */
    int init() override;
    
    /**
     * @brief 反初始化设备
     * @return 成功返回0，失败返回负值
     */
    int deinit() override;
    
    /**
     * @brief 挂起设备（低功耗模式）
     * @return 成功返回0，失败返回负值
     */
    int suspend() override;
    
    /**
     * @brief 恢复设备（从低功耗模式）
     * @return 成功返回0，失败返回负值
     */
    int resume() override;
    
    /**
     * @brief 获取电池电压
     * @return 电池电压(V)
     */
    float get_voltage() const;
    
    /**
     * @brief 获取电池电流
     * @return 电池电流(mA)，正值表示放电，负值表示充电
     */
    float get_current() const;
    
    /**
     * @brief 获取电池温度
     * @return 电池温度(℃)
     */
    float get_temperature() const;
    
    /**
     * @brief 获取电池充电状态
     * @return 是否正在充电
     */
    bool is_charging() const;
    
    /**
     * @brief 启用充电
     * @return 成功返回true，失败返回false
     */
    bool enable_charging();
    
    /**
     * @brief 禁用充电
     * @return 成功返回true，失败返回false
     */
    bool disable_charging();
    
private:
    // ADC通道配置
    int voltage_channel_ = -1;
    int current_channel_ = -1;
    int temp_channel_ = -1;
    
    // 充电控制GPIO
    int charge_enable_pin_ = -1;
    
    // 是否已初始化
    bool initialized_ = false;
};

/**
 * @brief 电池管理器类
 */
class battery_manager : public event_listener {
public:
    /**
     * @brief 获取电池管理器单例实例
     * @return 电池管理器引用
     */
    static battery_manager& get_instance();
    
    /**
     * @brief 初始化电池管理器
     * @param device 电池设备指针
     * @return 初始化成功返回true，失败返回false
     */
    bool init(std::shared_ptr<battery_device> device);
    
    /**
     * @brief 获取电池电压
     * @return 电池电压(V)
     */
    float get_voltage() const;
    
    /**
     * @brief 获取电池电流
     * @return 电池电流(mA)，正值表示放电，负值表示充电
     */
    float get_current() const;
    
    /**
     * @brief 获取电池温度
     * @return 电池温度(℃)
     */
    float get_temperature() const;
    
    /**
     * @brief 获取电池充电百分比
     * @return 电池充电百分比(0-100)
     */
    int get_charge_percentage() const;
    
    /**
     * @brief 获取电池状态
     * @return 电池状态
     */
    battery_state get_battery_state() const;
    
    /**
     * @brief 获取充电状态
     * @return 充电状态
     */
    charging_state get_charging_state() const;
    
    /**
     * @brief 启用充电
     * @return 成功返回true，失败返回false
     */
    bool enable_charging();
    
    /**
     * @brief 禁用充电
     * @return 成功返回true，失败返回false
     */
    bool disable_charging();
    
    /**
     * @brief 设置低电量阈值
     * @param percentage 电量百分比
     */
    void set_low_battery_threshold(int percentage);
    
    /**
     * @brief 设置严重低电量阈值
     * @param percentage 电量百分比
     */
    void set_critical_battery_threshold(int percentage);
    
    /**
     * @brief 处理电池监控，必须定期调用
     */
    void loop();
    
    /**
     * @brief 事件处理函数
     * @param event 事件数据
     */
    void on_event(const event_data& event) override;
    
private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    battery_manager();
    
    /**
     * @brief 私有析构函数（单例模式）
     */
    ~battery_manager();
    
    // 禁止拷贝和移动
    battery_manager(const battery_manager&) = delete;
    battery_manager& operator=(const battery_manager&) = delete;
    battery_manager(battery_manager&&) = delete;
    battery_manager& operator=(battery_manager&&) = delete;
    
    // 状态计算辅助函数
    void update_battery_state();
    
    // 计算电池电量百分比
    int calculate_percentage(float voltage) const;
    
    // 成员变量
    std::weak_ptr<battery_device> battery_device_; // 电池设备弱引用
    std::atomic<battery_state> current_state_;     // 当前电池状态
    std::atomic<charging_state> charging_state_;   // 当前充电状态
    std::atomic<int> charge_percentage_;           // 电量百分比
    
    int low_battery_threshold_;         // 低电量阈值
    int critical_battery_threshold_;    // 严重低电量阈值
    
    float last_voltage_;                // 上次测量的电压
    float last_current_;                // 上次测量的电流
    float last_temperature_;            // 上次测量的温度
    
    bool temp_warning_active_;          // 温度警告状态
    
    mutable std::mutex mutex_;          // 保护共享数据的互斥锁
};

} // namespace esp_framework 