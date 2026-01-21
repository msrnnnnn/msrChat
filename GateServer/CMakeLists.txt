cmake_minimum_required(VERSION 3.15)
project(GateServer)

# 1. 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 🔥 关键修改 A: 生成编译数据库
# 这行能救你的 VS Code，让它知道头文件到底在哪，消除红色波浪线
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 2. 查找依赖库
find_package(Boost REQUIRED COMPONENTS system thread filesystem)
message(STATUS "Boost found: ${Boost_INCLUDE_DIRS}")

# 查找 JsonCpp (Linux 下推荐优先用 PkgConfig，比 CONFIG 模式更稳)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# 查找 线程库 (标准跨平台写法，代替手动链 pthread)
find_package(Threads REQUIRED)

# 3. 收集源文件
# 你的目录结构是 src/*.cpp，这个写法是完全正确的
file(GLOB SOURCE_FILES "src/*.cpp")

# 4. 生成可执行文件
add_executable(GateServer ${SOURCE_FILES})

# 5. 配置头文件目录
# 🔥 关键修改 B: 使用 CMAKE_CURRENT_SOURCE_DIR
# 这样保证无论你是在项目根目录 cmake 还是在子目录 cmake，都能找到 include
target_include_directories(GateServer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(GateServer PRIVATE ${Boost_INCLUDE_DIRS})
target_include_directories(GateServer PRIVATE ${JSONCPP_INCLUDE_DIRS})

# 6. 链接库
target_link_libraries(GateServer PRIVATE ${Boost_LIBRARIES})
target_link_libraries(GateServer PRIVATE ${JSONCPP_LIBRARIES})
# 链接线程库 (自动识别是 pthread 还是其他)
target_link_libraries(GateServer PRIVATE Threads::Threads)

# 输出调试信息
message(STATUS "✅ Source Files: ${SOURCE_FILES}")
message(STATUS "✅ Include Dir: ${CMAKE_CURRENT_SOURCE_DIR}/include")