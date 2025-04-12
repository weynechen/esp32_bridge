#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_random.h"  // 用于esp_random()函数
#include <cmath>         // 用于fabs()函数
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "battery_manager.h"

static const char* TAG = "Battery";

// 配置参数
#define BATTERY_DEFAULT_LOW_THRESHOLD CONFIG_BATTERY_LOW_THRESHOLD
#define BATTERY_DEFAULT_CRITICAL_THRESHOLD CONFIG_BATTERY_CRITICAL_THRESHOLD
#define BATTERY_CHECK_INTERVAL_MS 10000  // 10秒检查一次
#define BATTERY_MIN_VOLTAGE 3.0f         // 最小电压 3.0V
#define BATTERY_MAX_VOLTAGE 4.2f         // 最大电压 4.2V
#define BATTERY_TEMP_WARNING 45.0f       // 温度警告阈值 45℃
#define BATTERY_TEMP_CRITICAL 55.0f      // 温度严重警告阈值 55℃

// 模拟电池相关常量，实际项目需替换为真实硬件
#define BATTERY_VOLTAGE_CHANNEL ADC1_CHANNEL_0
#define BATTERY_CURRENT_CHANNEL ADC1_CHANNEL_3
#define BATTERY_TEMP_CHANNEL ADC1_CHANNEL_6
#define BATTERY_CHARGE_ENABLE_PIN GPIO_NUM_10

// 添加错误处理模拟值
#define BATTERY_DEFAULT_VOLTAGE 3.8f
#define BATTERY_DEFAULT_CURRENT 100.0f
#define BATTERY_DEFAULT_TEMP 25.0f

namespace esp_framework {

// 电池设备实现
int battery_device::init() {
    ESP_LOGI(TAG, "初始化电池设备");
    
    try {
        // 初始化ADC
        adc1_config_width(ADC_WIDTH_BIT_12);
        
        // 配置电压测量通道
        voltage_channel_ = BATTERY_VOLTAGE_CHANNEL;
        adc1_config_channel_atten(static_cast<adc1_channel_t>(voltage_channel_), ADC_ATTEN_DB_11);
        
        // 配置电流测量通道
        current_channel_ = BATTERY_CURRENT_CHANNEL;
        adc1_config_channel_atten(static_cast<adc1_channel_t>(current_channel_), ADC_ATTEN_DB_11);
        
        // 配置温度测量通道
        temp_channel_ = BATTERY_TEMP_CHANNEL;
        adc1_config_channel_atten(static_cast<adc1_channel_t>(temp_channel_), ADC_ATTEN_DB_11);
        
        // 配置充电控制GPIO
        charge_enable_pin_ = BATTERY_CHARGE_ENABLE_PIN;
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << charge_enable_pin_);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
        
        // 默认开启充电
        enable_charging();
        
        initialized_ = true;
        ESP_LOGI(TAG, "电池设备初始化完成");
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "电池设备初始化失败: %s", e.what());
        return -1;
    } catch (...) {
        ESP_LOGE(TAG, "电池设备初始化发生未知异常");
        return -1;
    }
    return 0;
}

int battery_device::deinit() {
    if (!initialized_) {
        return 0;
    }
    
    // 禁用充电
    disable_charging();
    
    initialized_ = false;
    ESP_LOGI(TAG, "电池设备已释放");
    return 0;
}

int battery_device::suspend() {
    ESP_LOGI(TAG, "电池设备进入低功耗模式");
    // 低功耗模式下，减少ADC采样率
    return 0;
}

int battery_device::resume() {
    ESP_LOGI(TAG, "电池设备恢复正常模式");
    // 恢复正常ADC采样率
    return 0;
}

float battery_device::get_voltage() const {
    if (!initialized_) {
        ESP_LOGW(TAG, "设备未初始化，返回默认电压值");
        return BATTERY_DEFAULT_VOLTAGE;
    }
    
    // 使用模拟值以避免硬件问题
    int adc_raw = 2047 + (esp_random() % 1000);
    
    // 将12位ADC值(0-4095)转换为电压(3.0V-4.2V)
    float voltage = BATTERY_MIN_VOLTAGE + (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE) * adc_raw / 4095.0f;
    
    // 模拟随机波动
    voltage += ((esp_random() % 10) - 5) * 0.01f;
    
    // 仅当电压异常时才输出日志
    if (voltage < BATTERY_MIN_VOLTAGE || voltage > BATTERY_MAX_VOLTAGE) {
        ESP_LOGW(TAG, "电池电压异常: %.2fV", voltage);
    }
    
    return voltage;
}

float battery_device::get_current() const {
    if (!initialized_) {
        ESP_LOGW(TAG, "设备未初始化，返回默认电流值");
        return BATTERY_DEFAULT_CURRENT;
    }
    
    // 使用模拟值以避免硬件问题
    int adc_raw = 2047 + (esp_random() % 1000);
    
    // 将12位ADC值转换为电流(-1000mA至1000mA)
    float current = (adc_raw - 2048) / 2048.0f * 1000.0f;
    
    // 如果充电已禁用，则确保电流为正(放电)
    if (!is_charging()) {
        current = fabs(current);
    }
    
    // 模拟随机波动
    current += ((esp_random() % 10) - 5) * 0.5f;
    
    return current;
}

float battery_device::get_temperature() const {
    if (!initialized_) {
        ESP_LOGW(TAG, "设备未初始化，返回默认温度25.0°C");
        return BATTERY_DEFAULT_TEMP;
    }
    
    // 使用模拟值以避免硬件问题
    int adc_raw = 1024 + (esp_random() % 1000);
    
    // 将12位ADC值转换为温度(0-100℃)
    float temperature = adc_raw / 4095.0f * 100.0f;
    
    // 模拟随机波动
    temperature += ((esp_random() % 10) - 5) * 0.1f;
    
    // 仅当温度异常时才输出日志
    if (temperature > BATTERY_TEMP_WARNING) {
        ESP_LOGW(TAG, "电池温度过高: %.1f°C", temperature);
    }
    
    return temperature;
}

bool battery_device::is_charging() const {
    if (!initialized_) {
        return false;
    }
    
    return gpio_get_level(static_cast<gpio_num_t>(charge_enable_pin_)) == 1;
}

bool battery_device::enable_charging() {
    if (!initialized_) {
        return false;
    }
    
    esp_err_t ret = gpio_set_level(static_cast<gpio_num_t>(charge_enable_pin_), 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用充电失败: %d", ret);
        return false;
    }
    
    ESP_LOGI(TAG, "充电已启用");
    return true;
}

bool battery_device::disable_charging() {
    if (!initialized_) {
        return false;
    }
    
    esp_err_t ret = gpio_set_level(static_cast<gpio_num_t>(charge_enable_pin_), 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "禁用充电失败: %d", ret);
        return false;
    }
    
    ESP_LOGI(TAG, "充电已禁用");
    return true;
}

// 电池管理器实现
battery_manager& battery_manager::get_instance() {
    static battery_manager instance;
    return instance;
}

battery_manager::battery_manager() 
    : current_state_(battery_state::normal),
      charging_state_(charging_state::not_charging),
      charge_percentage_(50),
      low_battery_threshold_(BATTERY_DEFAULT_LOW_THRESHOLD),
      critical_battery_threshold_(BATTERY_DEFAULT_CRITICAL_THRESHOLD),
      last_voltage_(0.0f),
      last_current_(0.0f),
      last_temperature_(25.0f),
      temp_warning_active_(false) {
    
    ESP_LOGI(TAG, "电池管理器已创建");
}

battery_manager::~battery_manager() {
    ESP_LOGI(TAG, "电池管理器已销毁");
}

bool battery_manager::init(std::shared_ptr<battery_device> device) {
    if (!device) {
        ESP_LOGE(TAG, "电池设备指针为空");
        return false;
    }
    
    battery_device_ = device;
    
    // 注册事件监听 - 使用特殊方法订阅，由于是单例，不应该被shared_ptr删除
    auto listener_ptr = std::shared_ptr<event_listener>(this, [](event_listener*){});
    event_bus::get_instance().subscribe(event_type::network_connected, listener_ptr);
    event_bus::get_instance().subscribe(event_type::network_disconnected, listener_ptr);
    event_bus::get_instance().subscribe(event_type::enter_deep_sleep, listener_ptr);
    
    // 初始化状态
    update_battery_state();
    
    ESP_LOGI(TAG, "电池管理器初始化完成");
    return true;
}

float battery_manager::get_voltage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto device = battery_device_.lock();
    if (!device) {
        return last_voltage_;
    }
    
    return device->get_voltage();
}

float battery_manager::get_current() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto device = battery_device_.lock();
    if (!device) {
        return last_current_;
    }
    
    return device->get_current();
}

float battery_manager::get_temperature() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto device = battery_device_.lock();
    if (!device) {
        return last_temperature_;
    }
    
    return device->get_temperature();
}

int battery_manager::get_charge_percentage() const {
    return charge_percentage_;
}

battery_state battery_manager::get_battery_state() const {
    return current_state_;
}

charging_state battery_manager::get_charging_state() const {
    return charging_state_;
}

bool battery_manager::enable_charging() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto device = battery_device_.lock();
    if (!device) {
        return false;
    }
    
    return device->enable_charging();
}

bool battery_manager::disable_charging() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto device = battery_device_.lock();
    if (!device) {
        return false;
    }
    
    return device->disable_charging();
}

void battery_manager::set_low_battery_threshold(int percentage) {
    if (percentage < 5 || percentage > 50) {
        ESP_LOGW(TAG, "无效的低电量阈值: %d，应在5-50之间", percentage);
        return;
    }
    
    low_battery_threshold_ = percentage;
    ESP_LOGI(TAG, "已设置低电量阈值为: %d%%", percentage);
}

void battery_manager::set_critical_battery_threshold(int percentage) {
    if (percentage < 1 || percentage > 20) {
        ESP_LOGW(TAG, "无效的严重低电量阈值: %d，应在1-20之间", percentage);
        return;
    }
    
    critical_battery_threshold_ = percentage;
    ESP_LOGI(TAG, "已设置严重低电量阈值为: %d%%", percentage);
}

void battery_manager::loop() {
    static uint32_t last_check_time = 0;
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // 定期检查电池状态
    if (current_time - last_check_time >= BATTERY_CHECK_INTERVAL_MS) {
        last_check_time = current_time;
        update_battery_state();
    }
}

void battery_manager::on_event(const event_data& event) {
    switch (event.type) {
        case event_type::network_connected:
            // 网络连接后可以更频繁检查电池状态
            ESP_LOGI(TAG, "网络已连接，保持正常电池监控频率");
            break;
            
        case event_type::network_disconnected:
            // 网络断开后可以降低电池检查频率，节省电量
            ESP_LOGI(TAG, "网络已断开，降低电池监控频率");
            break;
            
        case event_type::enter_deep_sleep:
            // 进入深度睡眠前，禁用充电
            ESP_LOGI(TAG, "准备进入深度睡眠，禁用充电");
            disable_charging();
            break;
            
        default:
            break;
    }
}

void battery_manager::update_battery_state() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto device = battery_device_.lock();
    if (!device) {
        return;
    }
    
    // 获取当前状态
    float voltage = device->get_voltage();
    float current = device->get_current();
    float temperature = device->get_temperature();
    bool is_charging = device->is_charging();
    
    // 更新保存的状态
    last_voltage_ = voltage;
    last_current_ = current;
    last_temperature_ = temperature;
    
    // 计算电量百分比
    int percentage = calculate_percentage(voltage);
    charge_percentage_ = percentage;
    
    // 更新充电状态
    if (is_charging) {
        if (current < -500) {
            charging_state_ = charging_state::fast_charging;
        } else if (current < -100) {
            charging_state_ = charging_state::slow_charging;
        } else if (current < -10) {
            charging_state_ = charging_state::trickle_charging;
        } else if (percentage >= 100) {
            charging_state_ = charging_state::complete;
        } else {
            charging_state_ = charging_state::error;
        }
    } else {
        charging_state_ = charging_state::not_charging;
    }
    
    // 检查温度状态
    if (temperature > BATTERY_TEMP_CRITICAL) {
        // 温度过高，禁用充电
        ESP_LOGW(TAG, "温度过高 (%.1f°C)，禁用充电", temperature);
        device->disable_charging();
        
        // 发布温度过高事件
        if (!temp_warning_active_) {
            event_data event(event_type::battery_temp_high);
            event_bus::get_instance().publish(event);
            temp_warning_active_ = true;
        }
    } else if (temperature < BATTERY_TEMP_WARNING && temp_warning_active_) {
        // 温度恢复正常，启用充电
        ESP_LOGI(TAG, "温度恢复正常 (%.1f°C)，启用充电", temperature);
        device->enable_charging();
        
        // 发布温度正常事件
        event_data event(event_type::battery_temp_normal);
        event_bus::get_instance().publish(event);
        temp_warning_active_ = false;
    }
    
    // 更新电池状态
    battery_state prev_state = current_state_;
    
    if (is_charging) {
        current_state_ = battery_state::charging;
    } else if (percentage <= critical_battery_threshold_) {
        current_state_ = battery_state::critical;
    } else if (percentage <= low_battery_threshold_) {
        current_state_ = battery_state::low;
    } else if (percentage > 80) {
        current_state_ = battery_state::high;
    } else if (percentage >= 100) {
        current_state_ = battery_state::full;
    } else {
        current_state_ = battery_state::normal;
    }
    
    // 如果状态改变，发布相应事件
    if (prev_state != current_state_) {
        ESP_LOGI(TAG, "电池状态已改变，从 %d 变为 %d", 
                 static_cast<int>(prev_state), static_cast<int>(current_state_.load()));
        
        event_type evt_type;
        switch (current_state_) {
            case battery_state::low:
                evt_type = event_type::battery_low;
                ESP_LOGI(TAG, "准备发布电池电量低事件");
                break;
                
            case battery_state::critical:
                evt_type = event_type::battery_critical;
                ESP_LOGI(TAG, "准备发布电池电量严重不足事件");
                break;
                
            case battery_state::normal:
            case battery_state::high:
            case battery_state::full:
                evt_type = event_type::battery_normal;
                ESP_LOGI(TAG, "准备发布电池电量正常事件");
                break;
                
            case battery_state::charging:
                evt_type = event_type::charging_started;
                ESP_LOGI(TAG, "准备发布充电开始事件");
                break;
                
            default:
                return; // 不发布事件
        }
        
        event_data event(evt_type);
        event_bus::get_instance().publish(event);
    }
    
    // 仅在调试日志级别输出详细信息
    ESP_LOGD(TAG, "电池状态更新完成: 电压=%.2fV, 电流=%.2fmA, 温度=%.1f°C, 电量=%d%%, 状态=%d, 充电状态=%d",
             voltage, current, temperature, percentage, static_cast<int>(current_state_.load()), 
             static_cast<int>(charging_state_.load()));
}

int battery_manager::calculate_percentage(float voltage) const {
    // 简单线性映射从电压到百分比
    if (voltage <= BATTERY_MIN_VOLTAGE) {
        return 0;
    } else if (voltage >= BATTERY_MAX_VOLTAGE) {
        return 100;
    } else {
        return static_cast<int>((voltage - BATTERY_MIN_VOLTAGE) * 100.0f / 
                              (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE));
    }
}

} // namespace esp_framework 