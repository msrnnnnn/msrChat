cmake_minimum_required(VERSION 3.16)
project(QmsrChat VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# 锁定 UI 文件的真实物理搜索路径
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================
# Platform Detection and Compiler-Specific Settings
# TODO: scope add_compile_options/definitions to target via target_compile_options/definitions
# =============================================================
if(WIN32)
    message(STATUS "Building on Windows (MSVC/MinGW)")
    add_compile_definitions(_WIN32_WINNT=0x0601 WINVER=0x0601)

    if(MSVC)
        add_compile_options(/W3 /utf-8)
        add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            add_compile_options(/Od /Zi /MDd /fsanitize=address)
        else()
            add_compile_options(/O2 /Oi /GL)
        endif()
    else()
        add_compile_options(-fno-omit-frame-pointer)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            add_compile_options(-g -O0)
        else()
            add_compile_options(-O3)
        endif()
    endif()

elseif(UNIX AND NOT APPLE)
    message(STATUS "Building on Linux (GCC/Clang)")
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
        )
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    else()
        add_compile_options(-O3 -march=native)
    endif()
    find_package(Threads REQUIRED)
    link_libraries(Threads::Threads)
    add_compile_definitions(STD_FILESYSTEM_EXPERIMENTAL=0)

elseif(APPLE)
    message(STATUS "Building on macOS")
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

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Quick Sql QuickWidgets QuickControls2 QuickLayouts)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Core Quick Qml Sql QuickWidgets QuickControls2 QuickLayouts Test)

find_package(Protobuf REQUIRED)

set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/proto/Message.proto")
protobuf_generate_cpp(PROTO_SOURCES PROTO_HEADERS ${PROTO_FILE})

# =============================================================
# 源文件清单 (抛弃一切幻想，全部使用客观绝对路径)
# =============================================================
set(PROJECT_SOURCES
        src/main.cpp
        ${PROTO_SOURCES}

        # UI 相关 (锁定在 ui 子目录)
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/mainwindow.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/logindialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/registerdialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/resetdialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/chatdialog.ui
        resources.qrc

        # 头文件
        include/MainWindow.h
        include/LoginDialog.h
        include/RegisterDialog.h
        include/ResetDialog.h
        include/ChatDialog.h
        include/Global.h
        include/UserMgr.h
        include/TcpMgr.h
        include/TcpProtocolParser.h
        include/TcpWorker.h
        include/FileRecvMgr.h
        include/FileSendMgr.h
        include/singleton.h
        include/DPIHelper.h
        include/Utils.h
        include/TimerBtn.h
        include/ClickedLabel.h
        include/ChatListModel.h
        include/ImageDownloadMgr.h
        include/DbService.h
        include/DbWorker.h
        include/RingBuffer.h
        include/ChatController.h
        include/AuthUiHelpers.h
        include/ProtocolStructs.h

        # QML 视图 (严格锁定在根目录)
        ${CMAKE_CURRENT_SOURCE_DIR}/ChatView.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/MessageBubble.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/ImageBubble.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/ImageViewer.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/MessageActionMenu.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/EditMessageDialog.qml

        # 实现文件
        src/MainWindow.cpp
        src/LoginDialog.cpp
        src/RegisterDialog.cpp
        src/ResetDialog.cpp
        src/ChatDialog.cpp
        src/UserMgr.cpp
        src/TcpMgr.cpp
        src/TcpProtocolParser.cpp
        src/TcpWorker.cpp
        src/FileRecvMgr.cpp
        src/FileSendMgr.cpp
        src/TimerBtn.cpp
        src/ClickedLabel.cpp
        src/ChatListModel.cpp
        src/ImageDownloadMgr.cpp
        src/DbService.cpp
        src/DbWorker.cpp
        src/ChatController.cpp
        src/Utils.cpp
)

# Windows 平台：添加可执行文件图标
if(WIN32)
    list(APPEND PROJECT_SOURCES resources/win32/app.rc)
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(QmsrChat
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
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

target_link_libraries(QmsrChat PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::QuickWidgets
    Qt${QT_VERSION_MAJOR}::QuickControls2
    Qt${QT_VERSION_MAJOR}::QuickLayouts
    protobuf::libprotobuf
)

target_include_directories(QmsrChat PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

# ================= 配置管理自动化核心逻辑 =================
set(CONFIG_FILE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/config.ini")
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
    qt_import_qml_plugins(QmsrChat)
    qt_finalize_executable(QmsrChat)
endif()

# =============================================================
# Google Test (Unit Tests) — 通过 FetchContent 自动下载，无需 third_party 目录
# =============================================================
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

add_executable(QmsrChatTests
    tests/test_RingBuffer.cpp
    tests/test_ChatListModel.cpp
    tests/test_ImageDownloadMgr.cpp
    src/ChatListModel.cpp
    include/ChatListModel.h
    src/ImageDownloadMgr.cpp
    include/ImageDownloadMgr.h
)

# AUTOMOC for the test target: ChatListModel.h has Q_OBJECT, so its moc file
# (moc_ChatListModel.cpp) must be compiled into the test binary. Set both
# per-target and per-file to ensure AUTOMOC triggers (the global
# CMAKE_AUTOMOC=ON didn't auto-apply to this target's new sources).
set_target_properties(QmsrChatTests PROPERTIES AUTOMOC ON)
set_source_files_properties(include/ChatListModel.h include/ImageDownloadMgr.h PROPERTIES AUTOMOC ON)

target_include_directories(QmsrChatTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(QmsrChatTests PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Test
    gtest
    gtest_main
)

include(GoogleTest)
gtest_discover_tests(QmsrChatTests)
