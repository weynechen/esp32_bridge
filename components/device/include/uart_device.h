#pragma once

#include <string>
#include <memory>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "device.h"

namespace esp_framework {

/**
 * @brief UART设备类
 * 
 * 处理串口通信并转发数据到TCP服务器
 */
class uart_device : public device {
public:
    /**
     * @brief 构造函数
     * @param uart_num UART端口号
     * @param baud_rate 波特率
     * @param tx_pin 发送引脚
     * @param rx_pin 接收引脚
     */
    uart_device(uart_port_t uart_num = UART_NUM_1, 
               int baud_rate = 115200,
               int tx_pin = 17,  // 默认TX引脚
               int rx_pin = 18); // 默认RX引脚
    
    /**
     * @brief 析构函数
     */
    ~uart_device() override;
    
    /**
     * @brief 获取设备名称
     * @return 设备名称字符串
     */
    const char* name() const override { return "uart_device"; }
    
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
     * @brief 发送数据到UART
     * @param data 要发送的数据
     * @return 成功发送的字节数，失败返回负值
     */
    int send_data(const std::vector<uint8_t>& data);


    int send_data(const std::string& data);

    
private:
    // UART相关配置
    uart_port_t uart_num_;
    int baud_rate_;
    int tx_pin_;
    int rx_pin_;
    QueueHandle_t uart_queue_;
    TaskHandle_t uart_task_handle_;
    bool is_initialized_;
    
    // UART接收任务
    static void uart_rx_task(void* arg);
};

} // namespace esp_framework 