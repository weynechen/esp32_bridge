# ESP32 TCP客户端模拟器

这是一个用于模拟ESP32设备的TCP客户端程序，可以连接到TCP服务器并模拟ESP32设备发送/接收数据。这对于测试ESP32设备的TCP通信功能非常有用，无需实际的ESP32硬件。

## 功能特点

- 模拟ESP32设备连接到TCP服务器
- 支持发送文本和二进制数据
- 提供交互式命令行界面
- 可配置的自动心跳包功能
- 自动将接收到的数据显示为文本或十六进制格式

## 系统要求

- Python 3.6+
- 不需要额外的第三方库（仅使用Python标准库）

## 使用方法

### 基本用法

默认情况下，客户端会连接到本地（127.0.0.1）的8080端口：

```bash
python3 tcp_client_test.py
```

### 自定义连接参数

您可以指定服务器地址、端口和设备ID：

```bash
python3 tcp_client_test.py --host 192.168.1.100 --port 9000 --id ESP32-DEVICE-01
```

### 启用心跳包功能

客户端可以定期发送心跳包：

```bash
python3 tcp_client_test.py --heartbeat --interval 10
```

参数说明：
- `--heartbeat`：启用心跳包功能
- `--interval`：心跳包发送间隔，单位为秒（默认为5秒）

### 交互式命令

客户端启动后，进入交互式命令行界面，支持以下命令：

- `text <消息>`：发送文本消息
- `hex <十六进制>`：发送十六进制数据（例如：`hex 48 65 6c 6c 6f`）
- `reconnect`：重新连接到服务器
- `disconnect`：断开连接
- `help`：显示帮助信息
- `exit`：退出客户端

### 示例命令

```
ESP32客户端> text Hello Server
已发送: Hello Server

ESP32客户端> hex 48 65 6c 6c 6f
已发送二进制数据: 48 65 6c 6c 6f

ESP32客户端> disconnect
已断开连接

ESP32客户端> reconnect
已重新连接到服务器
```

## 应用场景

1. **测试TCP服务器功能**：无需实际ESP32硬件即可测试TCP服务器的接收和响应能力
2. **开发调试**：在ESP32硬件可用之前进行协议和通信测试
3. **模拟多个客户端**：可以同时运行多个客户端实例模拟多设备场景
4. **压力测试**：模拟多设备并发连接的情况

## 与TCP服务器配合使用

该客户端模拟器设计为与`tcp_server_test.py`配合使用，组成完整的TCP通信测试环境：

1. 首先启动TCP服务器：
   ```
   python3 tcp_server_test.py --port 8080
   ```

2. 然后启动一个或多个TCP客户端：
   ```
   python3 tcp_client_test.py --port 8080 --id ESP32-01
   python3 tcp_client_test.py --port 8080 --id ESP32-02
   ``` 