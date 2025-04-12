#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32 TCP服务器测试工具
用于接收和响应ESP32设备的TCP连接请求
"""

import socket
import time
import argparse
import threading
import logging
import sys
import signal

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# 全局变量
running = True

class TcpServer:
    def __init__(self, host='0.0.0.0', port=8080):
        """初始化TCP服务器
        
        Args:
            host: 服务器监听地址，默认所有地址
            port: 服务器监听端口
        """
        self.host = host
        self.port = port
        self.server_socket = None
        self.clients = []
        self.running = False
        
    def start(self):
        """启动TCP服务器"""
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind((self.host, self.port))
            self.server_socket.listen(5)
            
            self.running = True
            logger.info(f"服务器启动，监听 {self.host}:{self.port}")
            
            # 启动接收线程
            accept_thread = threading.Thread(target=self._accept_connections)
            accept_thread.daemon = True
            accept_thread.start()
            
            return True
            
        except Exception as e:
            logger.error(f"服务器启动失败: {e}")
            return False
    
    def stop(self):
        """停止TCP服务器"""
        self.running = False
        
        # 关闭所有客户端连接
        for client in self.clients:
            try:
                client.close()
            except:
                pass
        
        self.clients = []
        
        # 关闭服务器
        if self.server_socket:
            try:
                self.server_socket.close()
            except:
                pass
            self.server_socket = None
            
        logger.info("服务器已停止")
    
    def _accept_connections(self):
        """接受客户端连接的线程"""
        while self.running:
            try:
                self.server_socket.settimeout(1.0)
                client_socket, addr = self.server_socket.accept()
                
                logger.info(f"接受来自 {addr[0]}:{addr[1]} 的连接")
                
                # 将客户端添加到列表
                self.clients.append(client_socket)
                
                # 为每个客户端创建一个处理线程
                client_thread = threading.Thread(
                    target=self._handle_client,
                    args=(client_socket, addr)
                )
                client_thread.daemon = True
                client_thread.start()
                
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    logger.error(f"接受连接时出错: {e}")
                break
    
    def _handle_client(self, client_socket, addr):
        """处理客户端数据
        
        Args:
            client_socket: 客户端socket
            addr: 客户端地址
        """
        try:
            while self.running:
                # 接收数据
                data = client_socket.recv(1024)
                
                if not data:
                    logger.info(f"客户端 {addr[0]}:{addr[1]} 断开连接")
                    break
                
                # 打印接收到的数据
                try:
                    decoded = data.decode('utf-8')
                    logger.info(f"从 {addr[0]}:{addr[1]} 接收: {decoded}")
                except UnicodeDecodeError:
                    logger.info(f"从 {addr[0]}:{addr[1]} 接收二进制数据: {data.hex()}")
                
                # 回复客户端
                reply = f"服务器已接收 {len(data)} 字节"
                client_socket.send(reply.encode('utf-8'))
                
        except Exception as e:
            logger.error(f"处理客户端 {addr[0]}:{addr[1]} 时出错: {e}")
        
        finally:
            # 关闭连接并从列表中删除
            try:
                client_socket.close()
                if client_socket in self.clients:
                    self.clients.remove(client_socket)
            except:
                pass

def signal_handler(sig, frame):
    """处理中断信号"""
    global running
    logger.info("接收到中断信号，正在停止...")
    running = False

def main():
    """主函数"""
    global running
    
    # 解析命令行参数
    parser = argparse.ArgumentParser(description='TCP服务器测试工具')
    parser.add_argument('--host', default='0.0.0.0', help='服务器监听地址')
    parser.add_argument('--port', type=int, default=8080, help='服务器监听端口')
    args = parser.parse_args()
    
    # 处理中断信号
    signal.signal(signal.SIGINT, signal_handler)
    
    # 创建并启动服务器
    server = TcpServer(args.host, args.port)
    if not server.start():
        sys.exit(1)
    
    logger.info("服务器运行中，按Ctrl+C停止...")
    
    # 主循环
    try:
        while running:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    
    # 停止服务器
    server.stop()
    logger.info("服务器已退出")

if __name__ == "__main__":
    main() 