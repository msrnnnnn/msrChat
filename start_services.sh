#!/bin/bash

# Define service paths
GATE_SERVER_DIR="/home/msr/msrChat/server/GateServer/build"
CHAT_SERVER_DIR="/home/msr/msrChat/server/ChatServer/build"
STATUS_SERVER_DIR="/home/msr/msrChat/server/StatusServer/build"

# Function to start a service
start_service() {
    local dir=$1
    local name=$2
    local log_file="$3"

    if [ -d "$dir" ]; then
        echo "Starting $name..."
        cd "$dir"
        ./$name > /dev/null 2>&1 &
        echo "$name started with PID $!"
    else
        echo "Error: Directory $dir does not exist. Please build $name first."
    fi
}

# Stop existing services
echo "Stopping existing services..."
pkill -f GateServer
pkill -f ChatServer
pkill -f StatusServer
sleep 2

# Create logs directory if it doesn't exist
mkdir -p /home/msr/msrChat/server/GateServer/logs
mkdir -p /home/msr/msrChat/server/ChatServer/logs
mkdir -p /home/msr/msrChat/server/StatusServer/logs

# Start services
# Note: StatusServer should be started first as other services might depend on it
start_service "$STATUS_SERVER_DIR" "StatusServer"
sleep 2 # Wait for StatusServer to initialize

start_service "$CHAT_SERVER_DIR" "ChatServer"
start_service "$GATE_SERVER_DIR" "GateServer"

echo "All services started."
echo "You can check logs in:"
echo "  - server/StatusServer/logs/status_server.log"
echo "  - server/ChatServer/logs/chat_server.log"
echo "  - server/GateServer/logs/gate_server.log"
