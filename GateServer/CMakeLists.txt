cmake_minimum_required(VERSION 3.15)

# 项目名称
project(GateServer)

# 1. 设置 C++ 标准 (C++17 是 Asio/Beast 的推荐标准)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. 查找依赖库
# 2.1 查找 Boost (Asio 和 Beast 大部分是头文件，但在 Linux 上通常建议链接 system 和 thread)
find_package(Boost REQUIRED COMPONENTS system thread filesystem)

# 2.2 查找 JsonCpp
# 方法 A: 尝试标准查找
find_package(jsoncpp)
# 方法 B: 如果标准查找失败，尝试用 PkgConfig (Linux 常用)
if(NOT jsoncpp_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JSONCPP REQUIRED jsoncpp)
endif()

# 3. 包含头文件目录
# 包含项目自身的 include 目录
include_directories(${CMAKE_SOURCE_DIR}/include)
# 包含 Boost 头文件
include_directories(${Boost_INCLUDE_DIRS})
# 包含 JsonCpp 头文件
if(JSONCPP_INCLUDE_DIRS)
    include_directories(${JSONCPP_INCLUDE_DIRS})
endif()

# 4. 收集源文件
# 扫描 src 目录下所有的 .cpp 文件
file(GLOB SOURCE_FILES "src/*.cpp")

# 5. 生成可执行文件
add_executable(GateServer ${SOURCE_FILES})

# 6. 链接库
target_link_libraries(GateServer PRIVATE ${Boost_LIBRARIES})

# 链接 JsonCpp (根据查找结果适配)
if(TARGET JsonCpp::JsonCpp)
    target_link_libraries(GateServer PRIVATE JsonCpp::JsonCpp)
elseif(JSONCPP_LIBRARIES)
    target_link_libraries(GateServer PRIVATE ${JSONCPP_LIBRARIES})
else()
    target_link_libraries(GateServer PRIVATE jsoncpp)
endif()

# 链接线程库 (Linux 必须)
target_link_libraries(GateServer PRIVATE pthread)

# 输出提示信息
message(STATUS "GateServer configured with C++17")