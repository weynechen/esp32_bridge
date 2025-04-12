# ESP32S3 基于 TCP 的多设备框架

基于ESP32S3平台的多设备管理框架，支持事件发布订阅模式、设备抽象管理和可扩展总线设计。

## 项目结构

```
.
├── CMakeLists.txt             // 项目主CMake文件
├── components                 // 组件目录
│   ├── common                 // 通用组件
│   │   ├── CMakeLists.txt
│   │   ├── component.mk
│   │   ├── event_system.cpp   // 事件系统实现
│   │   └── include
│   │       └── event_system.h // 事件系统头文件
│   └── device                 // 设备管理组件
│       ├── CMakeLists.txt
│       ├── component.mk
│       ├── device_manager.cpp // 设备管理器实现
│       └── include
│           ├── device.h       // 设备抽象接口
│           └── device_manager.h // 设备管理器头文件
└── main                       // 主程序
    ├── CMakeLists.txt
    ├── component.mk
    └── main.cpp               // 主程序入口
```

## 特性

- **设备抽象层**：通过通用接口统一管理不同类型设备
- **事件系统**：发布-订阅模式实现模块间通信
- **总线设计**：支持设备的批量生命周期管理
- **ESP-IDF日志系统**：直接使用ESP-IDF内置的日志功能
- **RAII设计**：通过智能指针和RAII原则管理资源
- **模块化**：良好的模块划分和职责分离

## 开发环境

- ESP-IDF 版本：4.4 或更高
- 编译器支持：支持C++14及以上特性
- 构建工具：CMake 3.5或更高

## 构建指南

1. 设置ESP-IDF环境变量：
```bash
. $HOME/esp/esp-idf/export.sh
```

2. 编译项目：
```bash
idf.py build
```

3. 烧录程序：
```bash
idf.py -p [PORT] flash
```

## 扩展开发指南

### 添加新设备

1. 创建设备类继承`device`接口：

```cpp
class my_device : public device {
public:
    const char* name() const override { return "my_device"; }
    int init() override { /* 初始化代码 */ return 0; }
    int deinit() override { /* 清理代码 */ return 0; }
    int suspend() override { /* 挂起代码 */ return 0; }
    int resume() override { /* 恢复代码 */ return 0; }
};
```

2. 在主程序中注册设备：

```cpp
auto my_dev = std::shared_ptr<my_device>(new my_device());
dev_mgr.register_device(my_dev);
```

### 使用事件系统

1. 创建监听器类：

```cpp
class my_listener : public event_listener {
public:
    void on_event(const event_data& event) override {
        // 处理事件
    }
};
```

2. 注册监听器：

```cpp
auto listener = std::shared_ptr<my_listener>(new my_listener());
event_bus::get_instance().subscribe(event_type::network_connected, listener);
```

3. 发布事件：

```cpp
event_data event(event_type::network_connected);
event_bus::get_instance().publish(event);
```

## 许可证

MIT 

## 日志使用方法

本项目使用ESP-IDF的日志系统，使用方法：

```cpp
#include "esp_log.h"

// 定义TAG
static const char* TAG = "MyModule";

// 不同级别的日志
ESP_LOGE(TAG, "错误信息: %d", error_code);
ESP_LOGW(TAG, "警告信息: %s", warning_msg);
ESP_LOGI(TAG, "一般信息");
ESP_LOGD(TAG, "调试信息");
ESP_LOGV(TAG, "详细信息");

// 设置日志级别
esp_log_level_set("*", ESP_LOG_INFO);      // 设置所有模块的日志级别
esp_log_level_set("MyModule", ESP_LOG_DEBUG); // 设置特定模块的日志级别
```

## 构建说明

请确保已安装ESP-IDF开发环境。

```bash
# 设置ESP-IDF环境
. $HOME/esp/esp-idf/export.sh

# 构建项目
idf.py build

# 烧录程序
idf.py -p /dev/ttyUSB0 flash

# 监视串口输出
idf.py -p /dev/ttyUSB0 monitor
``` 