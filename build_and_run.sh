#!/bin/bash

# 定义颜色
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# 定义服务路径
ROOT_DIR="/home/msr/msrChat"
SERVER_DIR="$ROOT_DIR/server"
STATUS_SERVER_DIR="$SERVER_DIR/StatusServer"
CHAT_SERVER_DIR="$SERVER_DIR/ChatServer"
GATE_SERVER_DIR="$SERVER_DIR/GateServer"

# 错误处理函数
handle_error() {
    echo -e "${RED}Error: $1${NC}"
    exit 1
}

# 编译函数
build_service() {
    local service_name=$1
    local service_dir=$2
    
    echo -e "${GREEN}Building $service_name...${NC}"
    
    if [ ! -d "$service_dir" ]; then
        handle_error "Directory $service_dir does not exist."
    fi
    
    cd "$service_dir" || handle_error "Failed to enter $service_dir"
    
    # 创建 build 目录
    if [ ! -d "build" ]; then
        mkdir build
    fi
    
    cd build || handle_error "Failed to enter build directory"
    
    # 清理旧的 CMake 缓存 (可选，为了确保干净编译)
    # rm -rf CMakeCache.txt CMakeFiles
    
    # CMake 配置
    echo "Running CMake..."
    cmake .. || handle_error "CMake failed for $service_name"
    
    # Make 编译
    echo "Running Make..."
    make -j$(nproc) || handle_error "Make failed for $service_name"
    
    # 复制配置文件
    echo "Copying config.ini..."
    if [ -f "../config.ini" ]; then
        cp ../config.ini . || handle_error "Failed to copy config.ini"
    else
        echo -e "${RED}Warning: config.ini not found in $service_dir${NC}"
    fi
    
    echo -e "${GREEN}$service_name built successfully.${NC}"
    echo "----------------------------------------"
}

# 1. 停止现有服务
echo -e "${GREEN}Stopping existing services...${NC}"
pkill -f GateServer
pkill -f ChatServer
pkill -f StatusServer
sleep 2

# 2. 编译所有服务
echo -e "${GREEN}Starting build process...${NC}"

# 注意：顺序通常不重要，但如果存在依赖库可能需要注意
# 这里假设各个服务是独立的或者依赖已经安装在系统路径
build_service "StatusServer" "$STATUS_SERVER_DIR"
build_service "ChatServer" "$CHAT_SERVER_DIR"
build_service "GateServer" "$GATE_SERVER_DIR"

# 3. 启动所有服务
echo -e "${GREEN}Starting services...${NC}"

# 启动 StatusServer
echo "Starting StatusServer..."
cd "$STATUS_SERVER_DIR/build" || handle_error "Failed to enter StatusServer build dir"
./StatusServer > /dev/null 2>&1 &
PID_STATUS=$!
echo "StatusServer started with PID $PID_STATUS"
sleep 2 # 等待初始化

# 启动 ChatServer
echo "Starting ChatServer..."
cd "$CHAT_SERVER_DIR/build" || handle_error "Failed to enter ChatServer build dir"
./ChatServer > /dev/null 2>&1 &
PID_CHAT=$!
echo "ChatServer started with PID $PID_CHAT"
sleep 1

# 启动 GateServer
echo "Starting GateServer..."
cd "$GATE_SERVER_DIR/build" || handle_error "Failed to enter GateServer build dir"
./GateServer > /dev/null 2>&1 &
PID_GATE=$!
echo "GateServer started with PID $PID_GATE"

echo -e "${GREEN}All services started successfully!${NC}"
echo -e "StatusServer PID: $PID_STATUS"
echo -e "ChatServer PID:   $PID_CHAT"
echo -e "GateServer PID:   $PID_GATE"

# 检查进程是否存在
sleep 1
if ps -p $PID_STATUS > /dev/null && ps -p $PID_CHAT > /dev/null && ps -p $PID_GATE > /dev/null; then
    echo -e "${GREEN}System checks passed. Services are running.${NC}"
else
    echo -e "${RED}Warning: One or more services failed to start. Check logs.${NC}"
    ps aux | grep Server
fi
