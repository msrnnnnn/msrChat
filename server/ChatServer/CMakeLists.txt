cmake_minimum_required(VERSION 3.15)
project(ChatServer VERSION 1.0.0 LANGUAGES CXX)

# =============================================================
# 1. 基础设置
# =============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================
# 2. 依赖查找
# =============================================================
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)

# =============================================================
# 3. Protobuf 编译（共享 proto/ 目录）
# =============================================================
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../proto/Message.proto")
protobuf_generate_cpp(PROTO_SOURCES PROTO_HEADERS ${PROTO_FILE})

# =============================================================
# 4. 源文件列表
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
    src/ThreadPool.cpp
    src/FileTransfer.cpp
    src/SQLiteMgr.cpp
    src/AuthRepository.cpp
    src/MessageRepository.cpp
    src/SchemaManager.cpp
    src/ImageStorage.cpp
    src/RateLimiter.cpp
    src/services/AuthService.cpp
    src/services/ChatService.cpp
    src/services/FileService.cpp
    src/services/ImageService.cpp
)

# =============================================================
# 5. 创建可执行文件
# =============================================================
add_executable(ChatServer ${SERVER_SOURCES})

# =============================================================
# 6. Target 作用域编译选项（Phase 6: 替代全局 add_compile_options）
# =============================================================
target_compile_definitions(ChatServer PRIVATE _WIN32_WINNT=0x0601 WINVER=0x0601)

if(WIN32 AND MSVC)
    target_compile_options(ChatServer PRIVATE /W3 /utf-8)
    target_compile_definitions(ChatServer PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(ChatServer PRIVATE /Od /Zi /MDd /fsanitize=address)
    else()
        target_compile_options(ChatServer PRIVATE /O2 /Oi /GL)
    endif()
elseif(WIN32)
    # MinGW
    target_compile_options(ChatServer PRIVATE -fno-omit-frame-pointer)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(ChatServer PRIVATE -g -O0)
    else()
        target_compile_options(ChatServer PRIVATE -O3)
    endif()
elseif(UNIX AND NOT APPLE)
    target_compile_options(ChatServer PRIVATE -fno-omit-frame-pointer -Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(ChatServer PRIVATE -g3 -O0 -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(ChatServer PRIVATE -fsanitize=address)
    else()
        target_compile_options(ChatServer PRIVATE -O3 -march=native)
    endif()
elseif(APPLE)
    target_compile_options(ChatServer PRIVATE -fno-omit-frame-pointer -Wall)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(ChatServer PRIVATE -g -O0)
    else()
        target_compile_options(ChatServer PRIVATE -O3)
    endif()
endif()

target_include_directories(ChatServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(ChatServer PRIVATE
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
# 7. 安装目标
# =============================================================
install(TARGETS ChatServer
    RUNTIME DESTINATION bin
)

# =============================================================
# 8. Google Test (Unit Tests) — 可选构建
# =============================================================
option(BUILD_TESTS "Build unit tests (requires Google Test)" OFF)

if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.15.2
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(ChatServerTests
        tests/test_ShardedMap.cpp
        tests/test_ThreadPool.cpp
        tests/test_ImageStorage.cpp
        tests/test_AuthFlow.cpp
        tests/test_MessageOps.cpp
        src/ThreadPool.cpp
        src/ImageStorage.cpp
        src/SQLiteMgr.cpp
        src/AuthRepository.cpp
        src/MessageRepository.cpp
        src/SchemaManager.cpp
        src/TokenManager.cpp
    )

    # 测试 target 编译选项（继承主 target 的设置）
    if(WIN32 AND MSVC)
        target_compile_options(ChatServerTests PRIVATE /W3 /utf-8)
        target_compile_definitions(ChatServerTests PRIVATE _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0601 WINVER=0x0601)
    elseif(UNIX AND NOT APPLE)
        target_compile_options(ChatServerTests PRIVATE -fno-omit-frame-pointer -Wall -Wextra -Wpedantic)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_compile_options(ChatServerTests PRIVATE -g3 -O0 -fsanitize=address)
            target_link_options(ChatServerTests PRIVATE -fsanitize=address)
        endif()
    endif()

    target_include_directories(ChatServerTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
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
endif()
