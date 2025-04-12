#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "sdkconfig.h"
#include "network_module.h"
#include "esp_netif.h"

static const char* TAG = "Network";

// FreeRTOS事件组位
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// TCP缓冲区大小
#define TCP_BUFFER_SIZE 1024

// TCP任务栈大小和优先级
#define TCP_TASK_STACK_SIZE 4096
#define TCP_TASK_PRIORITY 5

namespace esp_framework {

// 创建事件组
static EventGroupHandle_t s_wifi_event_group = NULL;

// 静态实例
network_module& network_module::get_instance() {
    static network_module instance;
    return instance;
}

// 构造函数
network_module::network_module() 
    : ssid_(),
      password_(),
      server_host_(),
      server_port_(0),
      sock_(-1), 
      wifi_connected_(false), 
      tcp_connected_(false),
      task_handle_(nullptr) {
    
    // 初始化NVS闪存（WiFi库需要）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // 创建FreeRTOS事件组
    s_wifi_event_group = xEventGroupCreate();
    
    // 注册网络事件监听 - 使用特殊方法订阅，由于是单例，不应该被shared_ptr删除
    auto event_listener_ptr = std::shared_ptr<event_listener>(this, [](event_listener*){});
    event_bus::get_instance().subscribe(event_type::enter_deep_sleep, event_listener_ptr);
    
    ESP_LOGI(TAG, "网络模块初始化完成");
}

// 析构函数
network_module::~network_module() {
    // 确保断开所有连接
    disconnect_tcp();
    disconnect_wifi();
    
    // 删除事件组
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }
    
    // 取消事件订阅 - 使用特殊方法，由于是单例，不应该被shared_ptr删除
    auto event_listener_ptr = std::shared_ptr<event_listener>(this, [](event_listener*){});
    event_bus::get_instance().unsubscribe(event_type::enter_deep_sleep, event_listener_ptr);
    
    ESP_LOGI(TAG, "网络模块已销毁");
}

// WiFi事件处理函数
void network_module::wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    // 获取实例
    network_module& net = network_module::get_instance();
    
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "WiFi STA启动，尝试连接到AP");
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
            ESP_LOGW(TAG, "WiFi连接断开，原因: %d，尝试重新连接...", event->reason);
            
            // 打印断开原因
            switch (event->reason) {
                case WIFI_REASON_UNSPECIFIED:
                    ESP_LOGW(TAG, "WiFi断开原因: 未指定");
                    break;
                case WIFI_REASON_AUTH_EXPIRE:
                    ESP_LOGW(TAG, "WiFi断开原因: 认证过期");
                    break;
                case WIFI_REASON_AUTH_LEAVE:
                    ESP_LOGW(TAG, "WiFi断开原因: AP主动断开");
                    break;
                case WIFI_REASON_ASSOC_EXPIRE:
                    ESP_LOGW(TAG, "WiFi断开原因: 关联过期");
                    break;
                case WIFI_REASON_ASSOC_TOOMANY:
                    ESP_LOGW(TAG, "WiFi断开原因: AP连接设备过多");
                    break;
                case WIFI_REASON_NOT_AUTHED:
                    ESP_LOGW(TAG, "WiFi断开原因: 未认证");
                    break;
                case WIFI_REASON_NOT_ASSOCED:
                    ESP_LOGW(TAG, "WiFi断开原因: 未关联");
                    break;
                case WIFI_REASON_ASSOC_LEAVE:
                    ESP_LOGW(TAG, "WiFi断开原因: 主动断开关联");
                    break;
                case WIFI_REASON_ASSOC_NOT_AUTHED:
                    ESP_LOGW(TAG, "WiFi断开原因: 关联但未认证");
                    break;
                case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
                    ESP_LOGW(TAG, "WiFi断开原因: 4次握手超时 - 可能是密码错误");
                    break;
                default:
                    ESP_LOGW(TAG, "WiFi断开原因: 其他(%d)", event->reason);
            }
            
            net.wifi_connected_ = false;
            esp_wifi_connect();
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            
            // 发布网络断开事件
            esp_framework::event_data disconnect_event(event_type::network_disconnected);
            event_bus::get_instance().publish(disconnect_event);
        } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
            wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*) event_data;
            ESP_LOGI(TAG, "WiFi已连接到AP SSID:%s, channel:%d", 
                     event->ssid, event->channel);
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* ip_event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "获取IP地址: " IPSTR ", 网关: " IPSTR, 
                     IP2STR(&ip_event->ip_info.ip), 
                     IP2STR(&ip_event->ip_info.gw));
            net.wifi_connected_ = true;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            
            // 发布网络连接事件
            esp_framework::event_data connect_event(event_type::network_connected);
            event_bus::get_instance().publish(connect_event);
        } else if (event_id == IP_EVENT_STA_LOST_IP) {
            ESP_LOGW(TAG, "IP地址丢失");
        }
    }
}

// 连接WiFi
bool network_module::connect_wifi(const std::string& ssid, const std::string& password) {
    if (wifi_connected_) {
        ESP_LOGW(TAG, "WiFi已连接，请先断开");
        return true; // 已连接视为成功
    }
    
    ssid_ = ssid;
    password_ = password;
    
    ESP_LOGI(TAG, "开始连接WiFi: %s", ssid.c_str());
    
    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();
    
    // 初始化WiFi子系统
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // 注册事件处理程序
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    // 配置WiFi
    wifi_config_t wifi_config = {};
    strlcpy((char*)wifi_config.sta.ssid, ssid.c_str(), sizeof(wifi_config.sta.ssid));
    strlcpy((char*)wifi_config.sta.password, password.c_str(), sizeof(wifi_config.sta.password));
    
    // 配置WiFi模式和启动
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // 等待连接或超时
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          pdMS_TO_TICKS(30000));
    
    // 检查连接结果
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "成功连接到WiFi SSID: %s", ssid.c_str());
        return true;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "连接到WiFi SSID: %s 失败", ssid.c_str());
        return false;
    } else {
        ESP_LOGE(TAG, "连接超时");
        return false;
    }
}

// 断开WiFi
void network_module::disconnect_wifi() {
    if (!wifi_connected_) {
        return;
    }
    
    // 确保先断开TCP
    disconnect_tcp();
    
    // 停止WiFi
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    
    // 取消事件处理程序注册
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler));
    
    wifi_connected_ = false;
    ESP_LOGI(TAG, "已断开WiFi连接");
}

// TCP接收任务
void network_module::tcp_receive_task(void* pvParameters) {
    network_module* net = static_cast<network_module*>(pvParameters);
    int sock = net->sock_;
    uint8_t* rx_buffer = new uint8_t[TCP_BUFFER_SIZE];
    
    while (net->tcp_connected_) {
        // 接收数据
        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        
        if (len < 0) {
            // 连接错误
            ESP_LOGE(TAG, "TCP接收错误: errno %d", errno);
            net->disconnect_tcp();
            break;
        } else if (len == 0) {
            // 连接关闭
            ESP_LOGI(TAG, "TCP连接被关闭");
            net->disconnect_tcp();
            break;
        } else {
            // 收到数据
            ESP_LOGI(TAG, "收到 %d 字节数据", len);
            
            
            // 发布数据接收事件
            auto event_data_ptr = std::make_shared<uint8_t[]>(len);
            if (event_data_ptr) {
                memcpy(event_data_ptr.get(), rx_buffer, len);
                esp_framework::event_data data_event(event_type::data_received, 
                         event_data_type::binary, 
                         event_data_ptr, 
                         len);
                event_bus::get_instance().publish(data_event);
                // 使用shared_ptr自动管理内存，不需要手动释放
            }
        }
    }
    
    delete[] rx_buffer;
    // 任务完成
    net->task_handle_ = nullptr;
    vTaskDelete(NULL);
}

// 连接TCP服务器
bool network_module::connect_tcp(const std::string& host, uint16_t port) {
    if (tcp_connected_) {
        ESP_LOGW(TAG, "TCP已连接，请先断开");
        return true; // 已连接视为成功
    }
    
    if (!wifi_connected_) {
        ESP_LOGE(TAG, "WiFi未连接，无法建立TCP连接");
        return false;
    }
    
    server_host_ = host;
    server_port_ = port;
    
    ESP_LOGI(TAG, "开始连接TCP服务器: %s:%d", host.c_str(), port);
    
    // 创建套接字
    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock_ < 0) {
        ESP_LOGE(TAG, "创建套接字失败: errno %d", errno);
        return false;
    }
    
    // 配置服务器地址
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(host.c_str());
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    
    // 设置套接字为非阻塞模式
    int flags = fcntl(sock_, F_GETFL, 0);
    fcntl(sock_, F_SETFL, flags | O_NONBLOCK);
    
    // 设置超时时间
    struct timeval tv;
    tv.tv_sec = 60*10; //设置超时时间(s)
    tv.tv_usec = 0;
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    // 连接服务器
    int err = connect(sock_, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0 && errno != EINPROGRESS) {
        ESP_LOGE(TAG, "连接TCP服务器失败: errno %d", errno);
        close(sock_);
        sock_ = -1;
        return false;
    }
    
    // 如果连接正在进行中（非阻塞模式）
    if (err != 0 && errno == EINPROGRESS) {
        // 等待连接完成
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(sock_, &write_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 5;  // 5秒超时
        timeout.tv_usec = 0;
        
        int ret = select(sock_ + 1, NULL, &write_fds, NULL, &timeout);
        if (ret <= 0) {
            ESP_LOGE(TAG, "TCP连接超时或失败: %d", ret);
            close(sock_);
            sock_ = -1;
            return false;
        }
        
        // 检查连接是否成功
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(sock_, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            ESP_LOGE(TAG, "TCP连接建立失败: %d", error);
            close(sock_);
            sock_ = -1;
            return false;
        }
    }
    
    // 恢复阻塞模式
    flags = fcntl(sock_, F_GETFL, 0);
    fcntl(sock_, F_SETFL, flags & ~O_NONBLOCK);
    
    tcp_connected_ = true;
    ESP_LOGI(TAG, "成功连接到TCP服务器: %s:%d", host.c_str(), port);
    
    // 创建TCP接收任务
    if (task_handle_ == nullptr) {
        int ret = xTaskCreate(tcp_receive_task, "tcp_receive", TCP_TASK_STACK_SIZE, this, TCP_TASK_PRIORITY, &task_handle_);
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "TCP接收任务创建失败: %d", ret);
            disconnect_tcp();
            return false;
        }
        ESP_LOGI(TAG, "TCP接收任务创建成功");
    }
    
    return true;
}

// 断开TCP连接
void network_module::disconnect_tcp() {
    if (!tcp_connected_) {
        return;
    }
    
    // 设置状态为断开，使接收任务退出
    tcp_connected_ = false;
    
    // 关闭socket
    if (sock_ >= 0) {
        close(sock_);
        sock_ = -1;
        ESP_LOGI(TAG, "TCP连接已关闭");
    }
    
    // 等待接收任务结束
    if (task_handle_ != nullptr) {
        vTaskDelay(pdMS_TO_TICKS(100)); // 给任务一些时间退出
    }
    
    // 确保回调不会被调用
    data_callback_ = nullptr;
}

// 发送数据
bool network_module::send_data(const std::vector<uint8_t>& data) {
    if (!tcp_connected_ || sock_ < 0) {
        ESP_LOGE(TAG, "TCP未连接，无法发送数据");
        return false;
    }
    
    ESP_LOGI(TAG, "发送 %zu 字节数据", data.size());
    
    int err = send(sock_, data.data(), data.size(), 0);
    if (err < 0) {
        ESP_LOGE(TAG, "发送数据失败: errno %d", errno);
        return false;
    }
    
    return true;
}

// 设置数据接收回调
void network_module::set_data_callback(std::function<void(const std::vector<uint8_t>&)> callback) {
    data_callback_ = callback;
}

// 检查是否连接到WiFi
bool network_module::is_wifi_connected() const {
    return wifi_connected_;
}

// 检查是否连接到TCP服务器
bool network_module::is_tcp_connected() const {
    return tcp_connected_;
}

// 处理事件循环
void network_module::loop() {
    // 目前不需要特殊处理，只需定期调用以响应事件
}

// 事件处理
void network_module::on_event(const event_data& event) {
    switch (event.type) {
        case event_type::enter_deep_sleep:
            // 进入深度睡眠前，断开所有连接
            ESP_LOGI(TAG, "准备进入深度睡眠，断开所有网络连接");
            disconnect_tcp();
            disconnect_wifi();
            break;
            
        default:
            break;
    }
}

} // namespace esp_framework 