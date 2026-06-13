cmake_minimum_required(VERSION 3.16)
project(QmsrChat VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================
# 1. 依赖查找
# =============================================================
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Network Quick Sql QuickControls2 QuickLayouts)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network Core Quick Qml Sql QuickControls2 QuickLayouts Test)
find_package(Protobuf REQUIRED)

# =============================================================
# 2. Proto 编译（共享 proto/ 目录）
# =============================================================
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../proto/Message.proto")
protobuf_generate_cpp(PROTO_SOURCES PROTO_HEADERS ${PROTO_FILE})

# =============================================================
# 3. 源文件清单
# =============================================================
set(PROJECT_SOURCES
        src/main.cpp
        ${PROTO_SOURCES}

        # UI 相关
        resources.qrc

        # 头文件
        include/Global.h
        include/UserMgr.h
        include/TcpMgr.h
        include/TcpProtocolParser.h
        include/TcpWorker.h
        include/FileRecvMgr.h
        include/FileSendMgr.h
        include/Singleton.h
        include/Utils.h
        include/ChatListModel.h
        include/ImageDownloadMgr.h
        include/DbService.h
        include/DbWorker.h
        include/RingBuffer.h
        include/ChatController.h
        include/MessageActions.h
        include/FileCoordinator.h
        include/ProtocolStructs.h
        include/AuthController.h

        # QML 视图 (严格锁定在根目录)
        ${CMAKE_CURRENT_SOURCE_DIR}/CardTopAccent.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/AuthBanner.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/AuthCardHeader.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/PasswordField.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/ChatView.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/MessageBubble.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/ImageBubble.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/ImageViewer.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/MessageActionMenu.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/EditMessageDialog.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/MessageDelegate.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/ChatHeader.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/MessageInputArea.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/ImagePreviewBar.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/FileProgressPanel.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/EmptyState.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/AuthWindow.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/ChatWindow.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/LoginView.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/RegisterView.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/ResetView.qml

        # 实现文件
        src/UserMgr.cpp
        src/TcpMgr.cpp
        src/TcpProtocolParser.cpp
        src/TcpWorker.cpp
        src/FileRecvMgr.cpp
        src/FileSendMgr.cpp
        src/ChatListModel.cpp
        src/ImageDownloadMgr.cpp
        src/DbService.cpp
        src/DbWorker.cpp
        src/ChatController.cpp
        src/MessageActions.cpp
        src/FileCoordinator.cpp
        src/Utils.cpp
        src/AuthController.cpp
)

# Windows 平台：添加可执行文件图标
if(WIN32)
    list(APPEND PROJECT_SOURCES resources/win32/app.rc)
endif()

# =============================================================
# 4. 创建可执行文件
# =============================================================
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

# =============================================================
# 5. Target 作用域编译选项（Phase 6: 替代全局 add_compile_options）
# =============================================================
target_compile_definitions(QmsrChat PRIVATE _WIN32_WINNT=0x0601 WINVER=0x0601)

if(WIN32 AND MSVC)
    target_compile_options(QmsrChat PRIVATE /W3 /utf-8)
    target_compile_definitions(QmsrChat PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(QmsrChat PRIVATE /Od /Zi /MDd /fsanitize=address)
    else()
        target_compile_options(QmsrChat PRIVATE /O2 /Oi /GL)
    endif()
elseif(WIN32)
    # MinGW
    target_compile_options(QmsrChat PRIVATE -fno-omit-frame-pointer)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(QmsrChat PRIVATE -g -O0)
    else()
        target_compile_options(QmsrChat PRIVATE -O3)
    endif()
elseif(UNIX AND NOT APPLE)
    target_compile_options(QmsrChat PRIVATE -fno-omit-frame-pointer -Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(QmsrChat PRIVATE -g3 -O0 -fsanitize=address)
        target_link_options(QmsrChat PRIVATE -fsanitize=address)
    else()
        target_compile_options(QmsrChat PRIVATE -O3 -march=native)
    endif()
elseif(APPLE)
    target_compile_options(QmsrChat PRIVATE -fno-omit-frame-pointer -Wall)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(QmsrChat PRIVATE -g -O0)
    else()
        target_compile_options(QmsrChat PRIVATE -O3)
    endif()
endif()

# =============================================================
# 6. 链接库
# =============================================================
target_link_libraries(QmsrChat PRIVATE
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::Sql
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
# 7. Google Test (Unit Tests) — 可选构建
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

    add_executable(QmsrChatTests
        tests/test_RingBuffer.cpp
        tests/test_ChatListModel.cpp
        tests/test_ImageDownloadMgr.cpp
        src/ChatListModel.cpp
        include/ChatListModel.h
        src/ImageDownloadMgr.cpp
        include/ImageDownloadMgr.h
        src/DbWorker.cpp
        include/DbWorker.h
        src/DbService.cpp
        include/DbService.h
    )

    set_target_properties(QmsrChatTests PROPERTIES AUTOMOC ON)

    target_include_directories(QmsrChatTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(QmsrChatTests PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::Test
        gtest
        gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(QmsrChatTests DISCOVERY_MODE PRE_TEST)
endif()
