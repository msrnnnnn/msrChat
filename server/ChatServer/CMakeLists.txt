cmake_minimum_required(VERSION 3.15)
project(ChatServer VERSION 1.0.0 LANGUAGES CXX)

# =============================================================
# 1. 基础设置 (Basic Settings)
# =============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================
# 2. 依赖查找 (Dependencies)
# =============================================================
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

# =============================================================
# 3. 源文件列表 (Source Files)
# =============================================================
set(SERVER_SOURCES
    src/main.cpp
    src/AsioIOServicePool.cpp
    src/CServer.cpp
    src/CSession.cpp
)

# =============================================================
# 4. 包含目录 (Include Directories)
# =============================================================
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
)

# =============================================================
# 5. 创建可执行文件 (Create Executable)
# =============================================================
add_executable(ChatServer ${SERVER_SOURCES})

# =============================================================
# 6. 链接库 (Link Libraries)
# =============================================================
target_link_libraries(ChatServer
    ${Boost_LIBRARIES}
    Threads::Threads
)

# =============================================================
# 7. 安装目标 (Install Target - Optional)
# =============================================================
install(TARGETS ChatServer
    RUNTIME DESTINATION bin
)