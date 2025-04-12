#pragma once

#include "device.h"
#include <vector>
#include <memory>
#include <string>

namespace esp_framework {

/**
 * @brief 设备管理器类
 * 
 * 管理所有挂载的设备，并可广播调用生命周期接口
 */
class device_manager {
public:
    /**
     * @brief 注册设备到管理器
     * @param dev 设备智能指针
     */
    void register_device(std::shared_ptr<device> dev);
    
    /**
     * @brief 初始化所有已注册设备
     */
    void init_all();
    
    /**
     * @brief 反初始化所有已注册设备
     */
    void deinit_all();
    
    /**
     * @brief 挂起所有已注册设备（低功耗模式）
     */
    void suspend_all();
    
    /**
     * @brief 恢复所有已注册设备（从低功耗模式）
     */
    void resume_all();
    
    /**
     * @brief 根据名称获取设备
     * @param name 设备名称
     * @return 设备智能指针，未找到返回nullptr
     */
    std::shared_ptr<device> get_device_by_name(const std::string& name);
    
private:
    /** 已注册设备列表 */
    std::vector<std::shared_ptr<device>> devices_;
};

} // namespace esp_framework 