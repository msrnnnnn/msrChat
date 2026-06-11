# ==============================================================================
# ChatServer 多阶段 Dockerfile
# Stage 1: 编译（含所有依赖）
# Stage 2: 运行时（仅含运行时依赖）
# ==============================================================================

# ─── Stage 1: Builder ───
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    cmake g++ make \
    libboost-system-dev libboost-thread-dev \
    libssl-dev \
    libsqlite3-dev \
    nlohmann-json3-dev \
    libspdlog-dev \
    protobuf-compiler libprotobuf-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# 复制 proto 文件（位于项目根）
COPY proto/ /build/proto/

# 复制服务端源码
COPY server/ChatServer/ /build/server/ChatServer/

# 编译
RUN cd server/ChatServer && \
    cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --target ChatServer -j$(nproc)

# ─── Stage 2: Runtime ───
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 libboost-thread1.74.0 \
    libssl3 \
    libsqlite3-0 \
    libspdlog1 \
    libprotobuf23 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 从 builder 复制二进制
COPY --from=builder /build/server/ChatServer/build/ChatServer /app/ChatServer

# 复制默认配置（如有）
COPY server/ChatServer/config.ini.example /app/config.ini

EXPOSE 8080

ENTRYPOINT ["/app/ChatServer"]
