#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32设备TCP测试服务器
该服务器接收ESP32设备发送的TCP数据，并可以向设备发送命令
"""

import socket
import threading
import time
import argparse
import binascii
import logging

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class TcpServer:
    def __init__(self, host='0.0.0.0', port=8080):
        """初始化TCP服务器
        
        Args:
            host: 服务器监听的地址，默认为所有地址
            port: 服务器监听的端口，默认为8080
        """
        self.host = host
        self.port = port
        self.server_socket = None
        self.clients = {}  # 客户端连接字典 {addr: socket}
        self.clients_lock = threading.Lock()
        self.running = False
    
    def start(self):
        """启动TCP服务器"""
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind((self.host, self.port))
            self.server_socket.listen(5)
            self.running = True
            
            logger.info(f"TCP服务器已启动，监听地址: {self.host}:{self.port}")
            
            # 启动接受连接的线程
            accept_thread = threading.Thread(target=self._accept_connections)
            accept_thread.daemon = True
            accept_thread.start()
            
            # 启动命令行交互线程
            cmd_thread = threading.Thread(target=self._command_interface)
            cmd_thread.daemon = True
            cmd_thread.start()
            
            # 主线程保持运行
            while self.running:
                time.sleep(1)
                
        except KeyboardInterrupt:
            logger.info("收到中断信号，服务器正在关闭...")
        except Exception as e:
            logger.error(f"服务器出错: {e}")
        finally:
            self.stop()
    
    def stop(self):
        """停止TCP服务器"""
        self.running = False
        
        # 关闭所有客户端连接
        with self.clients_lock:
            for client_socket in self.clients.values():
                try:
                    client_socket.close()
                except:
                    pass
            self.clients.clear()
        
        # 关闭服务器套接字
        if self.server_socket:
            self.server_socket.close()
            self.server_socket = None
        
        logger.info("服务器已关闭")
    
    def _accept_connections(self):
        """接受新的客户端连接"""
        while self.running:
            try:
                client_socket, client_addr = self.server_socket.accept()
                client_socket.settimeout(60)  # 设置超时
                
                # 将新客户端添加到字典
                with self.clients_lock:
                    self.clients[client_addr] = client_socket
                
                logger.info(f"新客户端连接: {client_addr[0]}:{client_addr[1]}")
                
                # 为每个客户端创建一个处理线程
                client_thread = threading.Thread(
                    target=self._handle_client,
                    args=(client_socket, client_addr)
                )
                client_thread.daemon = True
                client_thread.start()
                
            except Exception as e:
                if self.running:
                    logger.error(f"接受连接出错: {e}")
    
    def _handle_client(self, client_socket, client_addr):
        """处理客户端连接
        
        Args:
            client_socket: 客户端套接字
            client_addr: 客户端地址 (ip, port)
        """
        try:
            while self.running:
                # 接收数据
                data = client_socket.recv(1024)
                if not data:
                    logger.info(f"客户端 {client_addr[0]}:{client_addr[1]} 断开连接")
                    break
                
                # 处理接收到的数据
                self._process_data(client_socket, client_addr, data)
                
        except socket.timeout:
            logger.warning(f"客户端 {client_addr[0]}:{client_addr[1]} 连接超时")
        except ConnectionResetError:
            logger.warning(f"客户端 {client_addr[0]}:{client_addr[1]} 连接重置")
        except Exception as e:
            logger.error(f"处理客户端 {client_addr[0]}:{client_addr[1]} 时出错: {e}")
        finally:
            # 关闭连接并从字典中移除
            client_socket.close()
            with self.clients_lock:
                if client_addr in self.clients:
                    del self.clients[client_addr]
            logger.info(f"客户端 {client_addr[0]}:{client_addr[1]} 已断开连接")
    
    def _process_data(self, client_socket, client_addr, data):
        """处理收到的数据
        
        Args:
            client_socket: 客户端套接字
            client_addr: 客户端地址
            data: 接收到的二进制数据
        """
        try:
            # 尝试将数据解码为字符串
            text_data = data.decode('utf-8', errors='replace')
            logger.info(f"收到来自 {client_addr[0]}:{client_addr[1]} 的文本数据: {text_data}")
        except:
            # 如果解码失败，则显示为十六进制
            hex_data = binascii.hexlify(data).decode('ascii')
            logger.info(f"收到来自 {client_addr[0]}:{client_addr[1]} 的二进制数据: {hex_data}")
        
        # 向客户端发送确认消息
        try:
            response = f"已收到数据: {len(data)}字节".encode('utf-8')
            client_socket.send(response)
        except Exception as e:
            logger.error(f"向客户端 {client_addr[0]}:{client_addr[1]} 发送响应时出错: {e}")
    
    def _command_interface(self):
        """命令行交互界面"""
        help_text = """
命令帮助:
  list           - 列出所有连接的客户端
  send <索引> <消息> - 向指定客户端发送消息
  broadcast <消息>  - 向所有客户端广播消息
  help           - 显示此帮助
  exit           - 退出服务器
"""
        print(help_text)
        
        while self.running:
            try:
                cmd = input("TCP服务器> ").strip()
                if not cmd:
                    continue
                
                if cmd == "exit":
                    logger.info("用户请求退出")
                    self.running = False
                    break
                    
                elif cmd == "help":
                    print(help_text)
                    
                elif cmd == "list":
                    with self.clients_lock:
                        if not self.clients:
                            print("没有连接的客户端")
                        else:
                            print("连接的客户端:")
                            for i, addr in enumerate(self.clients.keys()):
                                print(f"  [{i}] {addr[0]}:{addr[1]}")
                
                elif cmd.startswith("send "):
                    parts = cmd.split(" ", 2)
                    if len(parts) < 3:
                        print("格式错误, 使用: send <索引> <消息>")
                        continue
                    
                    try:
                        idx = int(parts[1])
                        message = parts[2]
                        
                        with self.clients_lock:
                            if not self.clients or idx < 0 or idx >= len(self.clients):
                                print(f"客户端索引 {idx} 不存在")
                                continue
                            
                            client_addr = list(self.clients.keys())[idx]
                            client_socket = self.clients[client_addr]
                            
                            try:
                                client_socket.send(message.encode('utf-8'))
                                print(f"消息已发送到 {client_addr[0]}:{client_addr[1]}")
                            except Exception as e:
                                print(f"发送消息失败: {e}")
                    
                    except ValueError:
                        print("索引必须是一个数字")
                
                elif cmd.startswith("broadcast "):
                    message = cmd[10:]  # 去掉"broadcast "前缀
                    
                    with self.clients_lock:
                        if not self.clients:
                            print("没有连接的客户端")
                        else:
                            failed = 0
                            for addr, sock in self.clients.items():
                                try:
                                    sock.send(message.encode('utf-8'))
                                except:
                                    failed += 1
                            
                            print(f"广播消息已发送到 {len(self.clients) - failed}/{len(self.clients)} 个客户端")
                
                else:
                    print(f"未知命令: {cmd}，输入 'help' 查看可用命令")
                    
            except KeyboardInterrupt:
                self.running = False
                break
            except Exception as e:
                logger.error(f"命令处理出错: {e}")


def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='ESP32 TCP测试服务器')
    parser.add_argument('--host', default='0.0.0.0', help='服务器监听地址 (默认: 0.0.0.0)')
    parser.add_argument('--port', type=int, default=8080, help='服务器监听端口 (默认: 8080)')
    args = parser.parse_args()
    
    server = TcpServer(args.host, args.port)
    try:
        server.start()
    except KeyboardInterrupt:
        pass
    finally:
        server.stop()


if __name__ == "__main__":
    main() 