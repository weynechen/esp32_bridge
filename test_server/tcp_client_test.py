#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32 TCP客户端模拟器
用于模拟ESP32设备连接到TCP服务器并发送/接收数据
"""

import socket
import time
import argparse
import threading
import logging
import sys

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class TcpClient:
    def __init__(self, host='127.0.0.1', port=8080, client_id='ESP32-0001'):
        """初始化TCP客户端
        
        Args:
            host: 服务器地址
            port: 服务器端口
            client_id: 客户端标识符
        """
        self.host = host
        self.port = port
        self.client_id = client_id
        self.socket = None
        self.running = False
        self.receive_thread = None
        
    def connect(self):
        """连接到TCP服务器"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.host, self.port))
            self.running = True
            
            logger.info(f"已连接到服务器 {self.host}:{self.port}")
            
            # 启动接收线程
            self.receive_thread = threading.Thread(target=self._receive_data)
            self.receive_thread.daemon = True
            self.receive_thread.start()
            
            return True
            
        except Exception as e:
            logger.error(f"连接失败: {e}")
            return False
    
    def disconnect(self):
        """断开与服务器的连接"""
        self.running = False
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
        logger.info("已断开连接")
    
    def send_text(self, text):
        """发送文本数据
        
        Args:
            text: 要发送的文本
        
        Returns:
            bool: 是否成功发送
        """
        if not self.socket or not self.running:
            logger.error("客户端未连接")
            return False
        
        try:
            data = text.encode('utf-8')
            self.socket.send(data)
            logger.info(f"已发送: {text}")
            return True
        
        except Exception as e:
            logger.error(f"发送失败: {e}")
            self.disconnect()
            return False
    
    def send_binary(self, data):
        """发送二进制数据
        
        Args:
            data: 二进制数据字节数组
        
        Returns:
            bool: 是否成功发送
        """
        if not self.socket or not self.running:
            logger.error("客户端未连接")
            return False
        
        try:
            self.socket.send(data)
            hex_repr = ' '.join(f'{b:02x}' for b in data)
            logger.info(f"已发送二进制数据: {hex_repr}")
            return True
        
        except Exception as e:
            logger.error(f"发送失败: {e}")
            self.disconnect()
            return False
    
    def _receive_data(self):
        """接收服务器发送的数据"""
        while self.running and self.socket:
            try:
                data = self.socket.recv(1024)
                if not data:
                    logger.warning("服务器关闭了连接")
                    break
                
                # 尝试解码为文本
                try:
                    text = data.decode('utf-8')
                    logger.info(f"收到文本: {text}")
                except:
                    # 如果无法解码，则显示十六进制
                    hex_repr = ' '.join(f'{b:02x}' for b in data)
                    logger.info(f"收到二进制数据: {hex_repr}")
                
            except ConnectionResetError:
                logger.warning("连接被重置")
                break
            except Exception as e:
                logger.error(f"接收数据时出错: {e}")
                break
        
        # 如果循环退出，确保断开连接
        self.running = False
        logger.info("接收线程已退出")


def interactive_mode(client):
    """交互式模式
    
    Args:
        client: TcpClient实例
    """
    help_text = """
命令帮助:
  text <消息>     - 发送文本消息
  hex <十六进制>  - 发送十六进制数据 (例如: hex 48 65 6c 6c 6f)
  reconnect      - 重新连接到服务器
  disconnect     - 断开连接
  help           - 显示此帮助
  exit           - 退出客户端
"""
    print(help_text)
    
    while True:
        try:
            cmd = input("ESP32客户端> ").strip()
            if not cmd:
                continue
            
            if cmd == "exit":
                break
                
            elif cmd == "help":
                print(help_text)
                
            elif cmd == "reconnect":
                client.disconnect()
                time.sleep(1)
                if client.connect():
                    print("已重新连接到服务器")
                else:
                    print("重新连接失败")
                
            elif cmd == "disconnect":
                client.disconnect()
                print("已断开连接")
                
            elif cmd.startswith("text "):
                message = cmd[5:]  # 去掉"text "前缀
                if client.send_text(message):
                    pass  # 日志已经在方法内部打印
                else:
                    print("发送失败")
                
            elif cmd.startswith("hex "):
                try:
                    # 解析十六进制字符串为字节数组
                    hex_str = cmd[4:].strip()
                    hex_values = hex_str.split()
                    data = bytes([int(h, 16) for h in hex_values])
                    
                    if client.send_binary(data):
                        pass  # 日志已经在方法内部打印
                    else:
                        print("发送失败")
                        
                except ValueError as e:
                    print(f"无效的十六进制值: {e}")
                except Exception as e:
                    print(f"错误: {e}")
            
            else:
                print(f"未知命令: {cmd}，输入 'help' 查看可用命令")
                
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"错误: {e}")


def send_periodic_heartbeat(client, interval=5):
    """定期发送心跳包
    
    Args:
        client: TcpClient实例
        interval: 心跳间隔(秒)
    """
    counter = 0
    
    while client.running:
        time.sleep(interval)
        counter += 1
        if client.running:
            message = f"{client.client_id} 心跳 #{counter}"
            client.send_text(message)


def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='ESP32 TCP客户端模拟器')
    parser.add_argument('--host', default='127.0.0.1', help='服务器地址 (默认: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=8080, help='服务器端口 (默认: 8080)')
    parser.add_argument('--id', default='ESP32-0001', help='客户端ID (默认: ESP32-0001)')
    parser.add_argument('--heartbeat', action='store_true', help='启用心跳包 (默认关闭)')
    parser.add_argument('--interval', type=int, default=5, help='心跳间隔，单位秒 (默认: 5)')
    args = parser.parse_args()
    
    # 创建客户端
    client = TcpClient(args.host, args.port, args.id)
    
    # 连接到服务器
    if not client.connect():
        sys.exit(1)
    
    try:
        # 发送初始消息
        client.send_text(f"设备 {args.id} 已连接")
        
        # 如果启用了心跳，启动心跳线程
        if args.heartbeat:
            logger.info(f"启用心跳包，间隔 {args.interval} 秒")
            heartbeat_thread = threading.Thread(
                target=send_periodic_heartbeat,
                args=(client, args.interval)
            )
            heartbeat_thread.daemon = True
            heartbeat_thread.start()
        
        # 进入交互模式
        interactive_mode(client)
        
    except KeyboardInterrupt:
        pass
    finally:
        client.disconnect()


if __name__ == "__main__":
    main() 