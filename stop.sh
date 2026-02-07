#!/bin/bash

# 定义颜色
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}Stopping services...${NC}"

# 发送 SIGTERM 信号 (默认) 给所有服务
# 这会触发代码中的 boost::asio::signal_set 处理逻辑，执行优雅退出
pkill -f GateServer
pkill -f ChatServer
pkill -f StatusServer

# 等待进程完全退出
echo "Waiting for processes to exit..."
sleep 2

# 检查是否还有残留进程
if pgrep -f GateServer > /dev/null || pgrep -f ChatServer > /dev/null || pgrep -f StatusServer > /dev/null; then
    echo -e "${RED}Warning: Some services did not stop gracefully. Forcing kill...${NC}"
    pkill -9 -f GateServer
    pkill -9 -f ChatServer
    pkill -9 -f StatusServer
else
    echo -e "${GREEN}All services stopped successfully.${NC}"
fi
