cmake_minimum_required(VERSION 3.15)
project(GateServer)

# 1. 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

<<<<<<< HEAD
# 生成编译数据库，优化 VS Code 提示
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 2. 查找依赖库
find_package(Boost REQUIRED COMPONENTS system thread filesystem)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
find_package(Threads REQUIRED)

# 3. 生成可执行文件 (注意：这里直接列出文件，只写一次！)
add_executable(GateServer
=======
# 生成编译数据库 (compile_commands.json)，让 VSCode 智能提示更准确
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 2. 查找依赖库
# Boost
find_package(Boost REQUIRED COMPONENTS system thread filesystem)
# JsonCpp (通过 pkg-config 查找)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
# 线程库
find_package(Threads REQUIRED)
# [Architect Fix] 强制优先搜索 /usr/local (你手动安装 gRPC/Protobuf 的地方)
# 这样可以避免 CMake 错误地找到系统自带的老版本 Protobuf
set(CMAKE_PREFIX_PATH "/usr/local")
# gRPC 和 Protobuf (新增核心依赖)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# 3. 定义源文件列表 (清晰管理)
# 注意：message.pb.cc 和 message.grpc.pb.cc 是你在项目根目录生成的，所以不需要加 src/ 前缀
set(SERVER_SOURCES
>>>>>>> d4bdb55d943f6b6232f392fe15188f9e5cb730f4
    src/GateServer.cpp
    src/CServer.cpp
    src/HttpConnection.cpp
    src/LogicSystem.cpp
<<<<<<< HEAD
)

# 4. 配置头文件目录
target_include_directories(GateServer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(GateServer PRIVATE ${Boost_INCLUDE_DIRS})
target_include_directories(GateServer PRIVATE ${JSONCPP_INCLUDE_DIRS})

# 5. 链接库
=======
    src/ConfigManagement.cpp          # [新增] 配置管理器
    src/VerifyGrpcClient.cpp   # [新增] gRPC 客户端
    message.pb.cc              # [新增] Protobuf 序列化代码
    message.grpc.pb.cc         # [新增] gRPC 服务存根代码
)

# 4. 生成可执行文件
add_executable(GateServer ${SERVER_SOURCES})

# 5. 配置头文件搜索路径 (Include Directories)
target_include_directories(GateServer PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include # 你的 .h 文件
    ${CMAKE_CURRENT_SOURCE_DIR}         # 因为 message.pb.h 生成在根目录，必须加这个！
    ${Boost_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${Protobuf_INCLUDE_DIRS}            # 引入 Protobuf 头文件
    # gRPC 头文件通常会自动包含在系统路径中，不需要额外写
)

# 6. 链接库 (Link Libraries)
>>>>>>> d4bdb55d943f6b6232f392fe15188f9e5cb730f4
target_link_libraries(GateServer PRIVATE 
    ${Boost_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    Threads::Threads
<<<<<<< HEAD
)

# 输出调试信息
message(STATUS "✅ GateServer project configured successfully.")
=======
    protobuf::libprotobuf   # [新增] 链接 Protobuf 核心库
    gRPC::grpc++            # [新增] 链接 gRPC C++ 库
    gRPC::grpc              # [新增] 链接 gRPC 核心库
    gRPC::grpc++_reflection # [可选] 支持反射调试
)

# 输出调试信息
message(STATUS "-------------------------------------------------------")
message(STATUS "✅ Project: GateServer configured.")
message(STATUS "✅ Sources: ${SERVER_SOURCES}")
message(STATUS "-------------------------------------------------------")
>>>>>>> d4bdb55d943f6b6232f392fe15188f9e5cb730f4
