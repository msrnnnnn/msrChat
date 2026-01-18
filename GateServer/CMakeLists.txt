cmake_minimum_required(VERSION 3.10) # 这里的版本号稍微放低一点，兼容老系统

project(GateServer VERSION 0.1 LANGUAGES CXX)

# 设置 C++ 标准，Beast 推荐至少 C++14，最好 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 开启所有警告，架构师的代码容不得沙子
add_compile_options(-Wall -Wextra)

# 1. 寻找依赖
# 在 Linux 下，FindBoost 会自动工作
find_package(Boost REQUIRED COMPONENTS system thread filesystem)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# 2. 收集源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 3. 定义可执行文件
add_executable(GateServer ${SOURCES})

# 4. 链接库
# 注意：Beast 是 header-only 的，但它依赖 Boost::system
target_link_libraries(GateServer PRIVATE
    Boost::system
    Boost::thread
    Boost::filesystem
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# 打印一下找到的 Boost 路径，让你放心
message(STATUS "Boost Include: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost Libs: ${Boost_LIBRARIES}")