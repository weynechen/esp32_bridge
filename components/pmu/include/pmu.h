#pragma once

#include <atomic>
#include <chrono>
#include "esp_log.h"
#include "esp_sleep.h"
#include "device_manager.h"
#include "event_system.h"

namespace esp_framework {

/**
 * @brief 电源管理器类
 * 
 * 负责设备低功耗控制和深度睡眠管理
 */
class pmu {
public:
    /**
     * @brief 构造函数
     * @param dev_mgr 设备管理器引用
     * @param idle_timeout_seconds 空闲超时时间(秒)
     */
    pmu(device_manager& dev_mgr, int idle_timeout_seconds = 10);
    
    /**
     * @brief 析构函数
     */
    ~pmu();
    
    /**
     * @brief 获取锁，保持系统活跃
     */
    void lock();
    
    /**
     * @brief 释放锁，允许系统进入低功耗模式
     */
    void unlock();
    
    /**
     * @brief 检查是否被锁定
     * @return 被锁定返回true，否则返回false
     */
    bool is_locked() const;
    
    /**
     * @brief 进入深度睡眠
     * @param sleep_time_ms 睡眠时间(毫秒)，0表示无限期睡眠
     */
    void enter_deep_sleep(uint32_t sleep_time_ms = 0);
    
    /**
     * @brief 必须定期调用以检查空闲状态
     */
    void loop();
    
    /**
     * @brief 设置空闲超时时间
     * @param seconds 超时时间(秒)
     */
    void set_idle_timeout(int seconds);
    
    /**
     * @brief 获取空闲超时时间
     * @return 超时时间(秒)
     */
    int get_idle_timeout() const;
    
private:
    device_manager& dev_mgr_;                             // 设备管理器引用
    std::atomic<bool> is_locked_;                         // 是否已锁定
    std::chrono::steady_clock::time_point last_unlock_time_; // 上次解锁时间
    std::chrono::seconds idle_timeout_;                   // 空闲超时时间
    bool is_suspended_;                                  // 是否已挂起
};

} // namespace esp_framework 