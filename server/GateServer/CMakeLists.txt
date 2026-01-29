cmake_minimum_required(VERSION 3.15)
project(GateServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ================= [Architect Fix: 隔离污染] =================
# 1. 强制 CMake 优先去 /usr/local 查找依赖 (你的新版本在这里)
set(CMAKE_PREFIX_PATH "/usr/local")

# 2. 强制使用静态库 (.a) 而不是动态库 (.so)
#    好处：把 Protobuf 代码直接打进你的 exe 里，运行时完全不依赖系统的 libprotobuf.so
#    解决：彻底根治 "version mismatch"
set(Protobuf_USE_STATIC_LIBS ON)
set(gRPC_USE_STATIC_LIBS ON) 
# =============================================================

# 查找依赖
find_package(Boost REQUIRED COMPONENTS system thread filesystem)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
find_package(Threads REQUIRED)

# 查找 Protobuf 和 gRPC (在设置了 PREFIX_PATH 后，它会找对地方)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# 源文件列表
set(SERVER_SOURCES
    src/GateServer.cpp
    src/CServer.cpp
    src/HttpConnection.cpp
    src/LogicSystem.cpp
    src/ConfigManagement.cpp
    src/VerifyGrpcClient.cpp
    message.pb.cc
    message.grpc.pb.cc
)

add_executable(GateServer ${SERVER_SOURCES})

# 包含路径
target_include_directories(GateServer PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${Protobuf_INCLUDE_DIRS} 
)

# 链接库 (静态链接需要手动处理一些底层依赖)
target_link_libraries(GateServer PRIVATE 
    ${Boost_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    Threads::Threads
    protobuf::libprotobuf   # 静态库
    gRPC::grpc++            # 静态库
    gRPC::grpc              # 静态库
    gRPC::grpc++_reflection
    dl                      # [Syscall] 静态链接时必须手动链 dlopen 相关接口
    z                       # [Syscall] gRPC 依赖压缩库
)

# [Debug Info] 打印出来看看它到底找的是谁，以此验尸
message(STATUS "-----------------------------------------")
message(STATUS "Protobuf Lib: ${Protobuf_LIBRARIES}")
message(STATUS "Protobuf Inc: ${Protobuf_INCLUDE_DIRS}")
message(STATUS "-----------------------------------------")