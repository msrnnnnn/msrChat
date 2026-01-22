cmake_minimum_required(VERSION 3.15)
project(GateServer)

# 1. 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 生成编译数据库，优化 VS Code 提示
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 2. 查找依赖库
find_package(Boost REQUIRED COMPONENTS system thread filesystem)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
find_package(Threads REQUIRED)

# 3. 生成可执行文件 (注意：这里直接列出文件，只写一次！)
add_executable(GateServer
    src/GateServer.cpp
    src/CServer.cpp
    src/HttpConnection.cpp
    src/LogicSystem.cpp
)

# 4. 配置头文件目录
target_include_directories(GateServer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(GateServer PRIVATE ${Boost_INCLUDE_DIRS})
target_include_directories(GateServer PRIVATE ${JSONCPP_INCLUDE_DIRS})

# 5. 链接库
target_link_libraries(GateServer PRIVATE 
    ${Boost_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    Threads::Threads
)

# 输出调试信息
message(STATUS "✅ GateServer project configured successfully.")