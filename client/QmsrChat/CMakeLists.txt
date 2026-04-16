cmake_minimum_required(VERSION 3.16)

project(QmsrChat VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================
# Platform Detection and Compiler-Specific Settings
# =============================================================
if(WIN32)
    message(STATUS "Building on Windows (MSVC/MinGW)")
    # Windows: Use -fno-omit-frame-pointer for better debugging
    # and add Windows-specific definitions
    add_compile_definitions(_WIN32_WINNT=0x0601 WINVER=0x0601)
    
    if(MSVC)
        # MSVC-specific flags
        add_compile_options(/W3 /utf-8)
        # Disable security warnings for third-party libraries
        add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            add_compile_options(/Od /Zi /MDd)
        else()
            add_compile_options(/O2 /Oi /GL)
        endif()
    else()
        # MinGW-specific flags
        add_compile_options(-fno-omit-frame-pointer)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            add_compile_options(-g -O0)
        else()
            add_compile_options(-O3)
        endif()
    endif()
    
elseif(UNIX AND NOT APPLE)
    message(STATUS "Building on Linux (GCC/Clang)")
    # Linux: Essential flags for sanitizer and debugging
    add_compile_options(
        -fno-omit-frame-pointer
        -Wall
        -Wextra
        -Wpedantic
    )
    
    # Add sanitizers for debug builds
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
    
    # Linux-specific pthread linking
    find_package(Threads REQUIRED)
    link_libraries(Threads::Threads)
    
    # Use std::filesystem for Linux (C++17)
    add_compile_definitions(STD_FILESYSTEM_EXPERIMENTAL=0)
    
elseif(APPLE)
    message(STATUS "Building on macOS")
    # macOS-specific settings
    add_compile_options(
        -fno-omit-frame-pointer
        -Wall
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O3)
    endif()
endif()

# Ensure consistent linker flags across platforms
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Quick)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Quick)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Qml)

# =============================================================
# Spdlog 配置
# =============================================================
find_package(spdlog REQUIRED)

# =============================================================
# Protobuf 配置
# =============================================================
find_package(Protobuf REQUIRED)

# 设置 proto 文件路径
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/proto/Message.proto")

# 使用 protobuf_generate_cpp 生成 C++ 源文件
protobuf_generate_cpp(PROTO_SOURCES PROTO_HEADERS ${PROTO_FILE})

# 添加 include 目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# 统一管理所有源文件
# 无论是在 Qt5 还是 Qt6 下编译，都使用这份清单
set(PROJECT_SOURCES
        src/main.cpp
        ${PROTO_SOURCES}
        ui/mainwindow.ui
        ui/logindialog.ui
        ui/registerdialog.ui
        ui/resetdialog.ui
        ui/ChatDialog.ui
        resources.qrc
        config.ini
        # UI 相关
        include/MainWindow.h
        include/LoginDialog.h
        include/RegisterDialog.h
        include/ResetDialog.h
        include/ChatDialog.h
        # 核心模块
        include/Global.h
        include/UserMgr.h
        include/TcpMgr.h
        include/TcpWorker.h
        include/FileRecvMgr.h
        include/singleton.h
        include/Logger.h
        include/logmacros.h
        # 工具类
        include/DPIHelper.h
        include/PathUtils.h
        include/Utils.h
        # UI 组件
        include/TimerBtn.h
        include/ClickedLabel.h
        # 数据模型
        include/ChatListModel.h
        include/ChatItemDelegate.h
        include/ChatModel.h
        include/DbMgr.h
        include/DbWorker.h
        include/RingBuffer.h
        include/ChatController.h
        # QML 视图
        ChatView.qml
        # 实现文件
        src/MainWindow.cpp
        src/LoginDialog.cpp
        src/RegisterDialog.cpp
        src/ResetDialog.cpp
        src/ChatDialog.cpp
        src/Global.cpp
        src/Logger.cpp
        src/UserMgr.cpp
        src/TcpMgr.cpp
        src/TcpWorker.cpp
        src/FileRecvMgr.cpp
        src/TimerBtn.cpp
        src/ClickedLabel.cpp
        src/ChatListModel.cpp
        src/ChatItemDelegate.cpp
        src/ChatModel.cpp
        src/DbMgr.cpp
        src/DbWorker.cpp
        src/ChatController.cpp
        src/Utils.cpp
)

# Qt6 Build Path
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(QmsrChat
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        .gitignore
    )
# Qt5 / Android Build Path
else()
    if(ANDROID)
        add_library(QmsrChat SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(QmsrChat
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(QmsrChat PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)
target_link_libraries(QmsrChat PRIVATE Qt${QT_VERSION_MAJOR}::Core)
target_link_libraries(QmsrChat PRIVATE Qt${QT_VERSION_MAJOR}::Quick)
target_link_libraries(QmsrChat PRIVATE Qt${QT_VERSION_MAJOR}::Qml)
target_link_libraries(QmsrChat PRIVATE protobuf::libprotobuf)
target_link_libraries(QmsrChat PRIVATE spdlog::spdlog)

# ================= 配置管理自动化核心逻辑 =================
# 定义配置文件源路径 (源码目录下的 config.ini)
set(CONFIG_FILE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/config.ini")

# 添加构建后钩子 (Post-Build Event)
# 作用：每次编译成功后，自动检测 config.ini 是否有变化，有则拷贝到 .exe 所在目录
if(EXISTS "${CONFIG_FILE_SRC}")
    add_custom_command(TARGET QmsrChat POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CONFIG_FILE_SRC}"
        "$<TARGET_FILE_DIR:QmsrChat>/config.ini"
        COMMENT "Auto-deploying config.ini to output binary directory..."
    )
else()
    message(WARNING "config.ini not found in ${CMAKE_CURRENT_SOURCE_DIR}. Run will fail if config is missing.")
endif()
# ==========================================================================

# macOS/iOS Bundle Identifier 设置
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.QmsrChat)
endif()

set_target_properties(QmsrChat PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS QmsrChat
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(QmsrChat)
endif()
