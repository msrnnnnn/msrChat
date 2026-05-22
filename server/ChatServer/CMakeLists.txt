cmake_minimum_required(VERSION 3.15)
project(ChatServer VERSION 1.0.0 LANGUAGES CXX)

# =============================================================
# 1. 基础设置 (Basic Settings)
# =============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================
# 2. 平台检测与编译器设置 (Platform Detection)
# TODO: 将 add_compile_options/add_compile_definitions 改为 target_compile_options/definitions
# =============================================================
# =============================================================
if(WIN32)
    message(STATUS "Building ChatServer on Windows (MSVC/MinGW)")
    add_compile_definitions(_WIN32_WINNT=0x0601 WINVER=0x0601)
    
    if(MSVC)
        add_compile_options(/W3 /utf-8)
        add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            add_compile_options(/Od /Zi /MDd)
        else()
            add_compile_options(/O2 /Oi /GL)
        endif()
    else()
        # MinGW
        add_compile_options(-fno-omit-frame-pointer)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            add_compile_options(-g -O0)
        else()
            add_compile_options(-O3)
        endif()
    endif()
    
elseif(UNIX AND NOT APPLE)
    message(STATUS "Building ChatServer on Linux (GCC/Clang)")
    add_compile_options(
        -fno-omit-frame-pointer
        -Wall
        -Wextra
        -Wpedantic
    )
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(
            -g3
            -O0
            -fsanitize=address
            -fno-omit-frame-pointer
        )
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    else()
        add_compile_options(-O3 -march=native)
    endif()
    
    find_package(Threads REQUIRED)
    link_libraries(Threads::Threads)
    
elseif(APPLE)
    message(STATUS "Building ChatServer on macOS")
    add_compile_options(-fno-omit-frame-pointer -Wall)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O3)
    endif()
endif()

# =============================================================
# 2. 依赖查找 (Dependencies)
# =============================================================
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)

# =============================================================
# Protobuf 自动化生成
# =============================================================
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../client/QmsrChat/proto/Message.proto")
protobuf_generate_cpp(PROTO_SOURCES PROTO_HEADERS ${PROTO_FILE})

# =============================================================
# 3. 源文件列表 (Source Files)
# =============================================================
set(SERVER_SOURCES
    src/main.cpp
    ${PROTO_SOURCES}
    src/AsioIOServicePool.cpp
    src/CServer.cpp
    src/CSession.cpp
    src/MessageDispatcher.cpp
    src/LogicSystem.cpp
    src/SessionManager.cpp
    src/MessageRouter.cpp
    src/TokenManager.cpp
    src/Protocol/BaseProtocol.cpp
    src/Protocol/TLVProtocol.cpp
    src/Protocol/BinaryPacketProtocol.cpp
    src/ThreadPool.cpp
    src/FileTransfer.cpp
    src/SQLiteMgr.cpp
    src/UserData.cpp
    src/OfflineStorage.cpp
)

# =============================================================
# 5. 创建可执行文件 (Create Executable)
# =============================================================
add_executable(ChatServer ${SERVER_SOURCES})

target_include_directories(ChatServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Protocol
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Boost_INCLUDE_DIRS}
)

# =============================================================
# 6. 链接库 (Link Libraries)
# =============================================================
target_link_libraries(ChatServer
    ${Boost_LIBRARIES}
    Threads::Threads
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    SQLite::SQLite3
    protobuf::libprotobuf
    OpenSSL::SSL
    OpenSSL::Crypto
)

# =============================================================
# 7. 安装目标 (Install Target - Optional)
# =============================================================
install(TARGETS ChatServer
    RUNTIME DESTINATION bin
)

# =============================================================
# 8. Google Test (Unit Tests)
# =============================================================
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(ChatServerTests
    tests/test_ShardedMap.cpp
    tests/test_TLVProtocol.cpp
    tests/test_ThreadPool.cpp
    tests/test_UserData.cpp
    src/Protocol/TLVProtocol.cpp
    src/Protocol/BinaryPacketProtocol.cpp
    src/Protocol/BaseProtocol.cpp
    src/ThreadPool.cpp
    src/UserData.cpp
)

target_include_directories(ChatServerTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Protocol
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(ChatServerTests PRIVATE
    ${Boost_LIBRARIES}
    Threads::Threads
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    SQLite::SQLite3
    protobuf::libprotobuf
    OpenSSL::SSL
    OpenSSL::Crypto
    gtest
    gtest_main
)

include(GoogleTest)
gtest_discover_tests(ChatServerTests)
