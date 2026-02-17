cmake_minimum_required(VERSION 3.15)
project(GateServer VERSION 1.0.0 LANGUAGES CXX)

# =============================================================
# 1. 基础设置 (Basic Settings)
# =============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================
# 2. 依赖查找策略 (Dependency Strategy)
# =============================================================
list(PREPEND CMAKE_PREFIX_PATH "/usr/local")

# 强制使用 Protobuf 和 gRPC 的静态库
set(Protobuf_USE_STATIC_LIBS ON)
set(gRPC_USE_STATIC_LIBS ON)

# =============================================================
# 3. 查找依赖包 (Find Packages)
# =============================================================
# 3.1 查找 Boost
find_package(Boost REQUIRED COMPONENTS system thread filesystem)

# 3.2 查找 JsonCpp
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# 3.3 查找 Protobuf & gRPC
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# 3.4 查找 Threads
find_package(Threads REQUIRED)

# 3.5 查找 Redis (hiredis)
find_path(HIREDIS_INCLUDE_DIR hiredis.h PATH_SUFFIXES hiredis)
find_library(HIREDIS_LIBRARY NAMES hiredis)

if (NOT HIREDIS_INCLUDE_DIR OR NOT HIREDIS_LIBRARY)
    message(FATAL_ERROR "hiredis not found! Please run: sudo apt-get install libhiredis-dev")
else()
    message(STATUS "Found hiredis: ${HIREDIS_LIBRARY}")
endif()

# 3.6 [新增] 查找 MySQL Connector/C++ (Linux版)
# Linux下通常头文件在 /usr/include/jdbc 或 /usr/include/mysql-cppconn-8
find_path(MYSQL_INCLUDE_DIR mysql_connection.h PATH_SUFFIXES jdbc mysql-cppconn-8)
find_library(MYSQL_LIBRARY NAMES mysqlcppconn)

if (NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "MySQL Connector C++ not found! Please run: sudo apt-get install libmysqlcppconn-dev")
else()
    message(STATUS "Found MySQL Connector: ${MYSQL_LIBRARY}")
endif()

# =============================================================
# 4. 定义构建目标 (Build Target)
# =============================================================
set(SERVER_SOURCES
    src/GateServer.cpp
    src/CServer.cpp
    src/HttpConnection.cpp
    src/LogicSystem.cpp
    src/ConfigMgr.cpp
    src/VerifyGrpcClient.cpp
    src/AsioIOServicePool.cpp
    src/RedisMgr.cpp        
    src/MysqlMgr.cpp        # <--- 已取消注释
    src/MysqlDao.cpp        # <--- 已取消注释
)

# =============================================================
# 6. Protobuf 自动生成 (Proto Generation)
# =============================================================
# 获取 protoc 和 grpc_cpp_plugin 的路径
get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
get_target_property(protoc_location protobuf::protoc LOCATION)

# 定义 Proto 文件路径 (绝对路径)
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../shared/message.proto")
set(PROTO_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# 定义生成的文件名
set(PROTO_SRCS "${PROTO_SRC_DIR}/message.pb.cc" "${PROTO_SRC_DIR}/message.grpc.pb.cc")
set(PROTO_HDRS "${PROTO_SRC_DIR}/message.pb.h" "${PROTO_SRC_DIR}/message.grpc.pb.h")

# 添加自定义命令生成代码
add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
    COMMAND ${protoc_location}
    ARGS --grpc_out=${PROTO_SRC_DIR}
         --cpp_out=${PROTO_SRC_DIR}
         --plugin=protoc-gen-grpc=${grpc_cpp_plugin_location}
         -I "${CMAKE_CURRENT_SOURCE_DIR}/../../shared"
         ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Running C++ gRPC protocol buffer compiler on message.proto"
)

# 确保生成的代码也被包含在源文件列表中（或单独作为 target 依赖）
# 在 add_executable 前将生成的文件加入 SERVER_SOURCES
list(APPEND SERVER_SOURCES ${PROTO_SRCS})

add_executable(GateServer ${SERVER_SOURCES})

# =============================================================
# 5. 配置目标属性 (Target Properties)
# =============================================================
# 5.1 头文件路径
target_include_directories(GateServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROTO_SRC_DIR} # <--- 添加生成代码的目录
    ${Boost_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${Protobuf_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIR}
    ${MYSQL_INCLUDE_DIR}     # <--- 加入 MySQL 头文件路径
)

# 5.2 链接库
target_link_libraries(GateServer PRIVATE
    # 第三方库
    ${Boost_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    ${HIREDIS_LIBRARY}
    ${MYSQL_LIBRARY}         # <--- 链接 Linux 下的 mysqlcppconn 动态库

    # Google 全家桶 (静态库)
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc
    gRPC::grpc++_reflection
    
    # 系统级依赖
    Threads::Threads
    dl                      
    z                       
)

# =============================================================
# 7. 调试信息 (Debug Info)
# =============================================================
message(STATUS "Build Info:")
message(STATUS "   CXX Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "   Source Dir:   ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "   Hiredis Lib:  ${HIREDIS_LIBRARY}")
message(STATUS "   MySQL Lib:    ${MYSQL_LIBRARY}")
