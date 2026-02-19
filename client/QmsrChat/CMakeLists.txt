cmake_minimum_required(VERSION 3.16)

project(QmsrChat VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

# [Architect Refactor] 统一管理所有源文件
# 无论是在 Qt5 还是 Qt6 下编译，都使用这份清单
set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp mainwindow.h mainwindow.ui
        logindialog.cpp logindialog.h logindialog.ui
        registerdialog.cpp registerdialog.h registerdialog.ui
        resetdialog.cpp resetdialog.h resetdialog.ui
        httpmanagement.cpp httpmanagement.h
        global.cpp global.h
        timerbtn.cpp timerbtn.h
        clickedlabel.cpp clickedlabel.h
        singleton.h
        resources.qrc
        config.ini  # 将配置文件加入项目视图，方便在 IDE 中编辑
)

# [Qt6 Build Path]
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(QmsrChat
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        .gitignore
        tcpmgr.h tcpmgr.cpp
        # 直接使用上面定义好的集合
    )
# [Qt5 / Android Build Path]
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

# ================= [Architect Add] 配置管理自动化核心逻辑 =================
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
