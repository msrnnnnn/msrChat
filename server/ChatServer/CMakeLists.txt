cmake_minimum_required(VERSION 3.15)
project(ChatServer VERSION 1.0.0 LANGUAGES CXX)

# =============================================================
# 1. 基础设置 (Basic Settings)
# =============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")

# =============================================================
# 2. 依赖查找 (Dependencies)
# =============================================================
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# 引入 nlohmann_json
include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_Declare(spdlog URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz)
FetchContent_MakeAvailable(json)
FetchContent_MakeAvailable(spdlog)

# =============================================================
# 3. 源文件列表 (Source Files)
# =============================================================
set(SERVER_SOURCES
    src/main.cpp
    src/AsioIOServicePool.cpp
    src/CServer.cpp
    src/CSession.cpp
    ../GateServer/src/RedisMgr.cpp
)

# =============================================================
# 4. 包含目录 (Include Directories)
# =============================================================
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../GateServer/include
    ${Boost_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
)

# =============================================================
# 5. 创建可执行文件 (Create Executable)
# =============================================================
add_executable(ChatServer ${SERVER_SOURCES})
add_executable(AsioIOServicePoolTest ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/AsioIOServicePoolTest.cpp)
target_link_libraries(AsioIOServicePoolTest
    ${Boost_LIBRARIES}
    Threads::Threads
    GTest::GTest
    GTest::Main
)

# =============================================================
# 6. 链接库 (Link Libraries)
# =============================================================
target_link_libraries(ChatServer
    ${Boost_LIBRARIES}
    Threads::Threads
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

# =============================================================
# 7. 安装目标 (Install Target - Optional)
# =============================================================
install(TARGETS ChatServer
    RUNTIME DESTINATION bin
)
