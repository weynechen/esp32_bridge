#include "uart_device.h"
#include "esp_log.h"
#include "network_module.h"
#include <cstring>
#include "event_system.h"

// 定义一些常量
#define UART_BUF_SIZE (1024)
#define UART_TASK_STACK_SIZE (4096)
#define UART_TASK_PRIORITY (10)

static const char* TAG = "UART_DEVICE";

namespace esp_framework {

uart_device::uart_device(uart_port_t uart_num, int baud_rate, int tx_pin, int rx_pin)
    : uart_num_(uart_num), baud_rate_(baud_rate), tx_pin_(tx_pin), rx_pin_(rx_pin),
      uart_queue_(nullptr), uart_task_handle_(nullptr), is_initialized_(false) {
    ESP_LOGI(TAG, "创建UART设备: 端口=%d, 波特率=%d, TX=%d, RX=%d", 
             uart_num, baud_rate, tx_pin, rx_pin);
}

uart_device::~uart_device() {
    deinit();
}

int uart_device::init() {
    if (is_initialized_) {
        ESP_LOGW(TAG, "UART设备已经初始化");
        return 0;
    }
    
    // 配置UART参数
    uart_config_t uart_config = {
        .baud_rate = baud_rate_,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_APB,
    };
    
    // 安装UART驱动
    ESP_LOGI(TAG, "初始化UART驱动");
    int ret = uart_driver_install(uart_num_, UART_BUF_SIZE * 2, UART_BUF_SIZE * 2, 20, &uart_queue_, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART驱动安装失败: %d", ret);
        return -1;
    }
    
    // 配置UART参数
    ret = uart_param_config(uart_num_, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART参数配置失败: %d", ret);
        uart_driver_delete(uart_num_);
        return -1;
    }
    
    // 设置UART引脚
    ret = uart_set_pin(uart_num_, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART引脚设置失败: %d", ret);
        uart_driver_delete(uart_num_);
        return -1;
    }
    
    // 创建UART接收任务
    ret = xTaskCreate(uart_rx_task, "uart_rx_task", UART_TASK_STACK_SIZE, this, UART_TASK_PRIORITY, &uart_task_handle_);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "UART接收任务创建失败: %d", ret);
        uart_driver_delete(uart_num_);
        return -1;
    }
    
    is_initialized_ = true;
    ESP_LOGI(TAG, "UART设备初始化成功");
    return 0;
}

int uart_device::deinit() {
    if (!is_initialized_) {
        return 0;
    }
    
    // 删除UART接收任务
    if (uart_task_handle_ != nullptr) {
        vTaskDelete(uart_task_handle_);
        uart_task_handle_ = nullptr;
    }
    
    // 删除UART驱动
    int ret = uart_driver_delete(uart_num_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART驱动删除失败: %d", ret);
        return -1;
    }
    
    uart_queue_ = nullptr;
    is_initialized_ = false;
    ESP_LOGI(TAG, "UART设备已反初始化");
    return 0;
}

int uart_device::suspend() {
    if (!is_initialized_) {
        return 0;
    }
    
    ESP_LOGI(TAG, "挂起UART设备");
    
    // 暂停UART接收任务
    if (uart_task_handle_ != nullptr) {
        vTaskSuspend(uart_task_handle_);
    }
    
    return 0;
}

int uart_device::resume() {
    if (!is_initialized_) {
        return 0;
    }
    
    ESP_LOGI(TAG, "恢复UART设备");
    
    // 恢复UART接收任务
    if (uart_task_handle_ != nullptr) {
        vTaskResume(uart_task_handle_);
    }
    
    return 0;
}

int uart_device::send_data(const std::vector<uint8_t>& data) {
    if (!is_initialized_ || data.empty()) {
        return -1;
    }
    
    // 发送数据到UART
    int written = uart_write_bytes(uart_num_, data.data(), data.size());
    if (written < 0) {
        ESP_LOGE(TAG, "UART发送数据失败: %d", written);
    } else {
        ESP_LOGI(TAG, "UART发送数据成功: %d字节", written);
    }
    
    return written;
}

int uart_device::send_data(const std::string& data) {
    return send_data(std::vector<uint8_t>(data.begin(), data.end()));
}


void uart_device::uart_rx_task(void* arg) {
    uart_device* device = static_cast<uart_device*>(arg);
    uart_event_t event;
    uint8_t* data = new uint8_t[UART_BUF_SIZE];
    
    if (!data) {
        ESP_LOGE(TAG, "无法分配UART接收缓冲区");
        vTaskDelete(NULL);
        return;
    }
    
    auto& network = network_module::get_instance();
    
    ESP_LOGI(TAG, "UART接收任务已启动");
    
    while (1) {
        // 等待UART事件
        if (xQueueReceive(device->uart_queue_, &event, portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA: {
                    // 读取UART数据
                    int len = uart_read_bytes(device->uart_num_, data, event.size, portMAX_DELAY);
                    if (len > 0) {
                        ESP_LOGI(TAG, "接收到UART数据: %d字节", len);
                        
                        // 创建数据副本用于事件系统和网络发送
                        std::shared_ptr<uint8_t[]> data_copy(new uint8_t[len], std::default_delete<uint8_t[]>());
                        if (data_copy) {
                            // 复制数据
                            memcpy(data_copy.get(), data, len);
                            
                            // 使用data_copy创建临时vector用于网络发送
                            if (network.is_tcp_connected()) {
                                // 使用shared_ptr中的数据创建临时vector
                                std::vector<uint8_t> temp_vec(data_copy.get(), data_copy.get() + len);
                                if (network.send_data(temp_vec)) {
                                    ESP_LOGI(TAG, "数据已转发到TCP服务器");
                                } else {
                                    ESP_LOGE(TAG, "数据转发到TCP服务器失败");
                                }
                            } else {
                                ESP_LOGW(TAG, "TCP未连接，无法转发数据");
                            }
                            
                            // 发布事件，通知其他组件
                            event_data event_data(event_type::data_received, 
                                                  event_data_type::binary, 
                                                  data_copy, len);
                            event_bus::get_instance().publish(event_data);
                        } else {
                            ESP_LOGE(TAG, "内存不足，无法处理UART数据");
                        }
                    }
                    break;
                }
                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "UART FIFO溢出，清除FIFO");
                    uart_flush_input(device->uart_num_);
                    xQueueReset(device->uart_queue_);
                    break;
                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "UART缓冲区满，清除缓冲区");
                    uart_flush_input(device->uart_num_);
                    xQueueReset(device->uart_queue_);
                    break;
                case UART_BREAK:
                    ESP_LOGW(TAG, "UART接收到BREAK信号");
                    break;
                case UART_PARITY_ERR:
                    ESP_LOGW(TAG, "UART奇偶校验错误");
                    break;
                case UART_FRAME_ERR:
                    ESP_LOGW(TAG, "UART帧错误");
                    break;
                default:
                    ESP_LOGD(TAG, "UART其他事件: %d", event.type);
                    break;
            }
        }
    }
    
    delete[] data;
    vTaskDelete(NULL);
}

} // namespace esp_framework 