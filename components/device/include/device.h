#pragma once

#include <string>
#include <memory>

namespace esp_framework {

/**
 * @brief 设备抽象基类
 * 
 * 定义所有外设统一操作接口（初始化、挂起、恢复等）
 */
class device {
public:
    /**
     * @brief 获取设备名称
     * @return 设备名称字符串
     */
    virtual const char* name() const = 0;
    
    /**
     * @brief 初始化设备
     * @return 成功返回0，失败返回负值
     */
    virtual int init() = 0;
    
    /**
     * @brief 反初始化设备
     * @return 成功返回0，失败返回负值
     */
    virtual int deinit() = 0;
    
    /**
     * @brief 挂起设备（低功耗模式）
     * @return 成功返回0，失败返回负值
     */
    virtual int suspend() = 0;
    
    /**
     * @brief 恢复设备（从低功耗模式）
     * @return 成功返回0，失败返回负值
     */
    virtual int resume() = 0;
    
    /**
     * @brief 虚析构函数
     */
    virtual ~device() = default;
};

} // namespace esp_framework 