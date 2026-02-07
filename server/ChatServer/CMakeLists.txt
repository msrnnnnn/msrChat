cmake_minimum_required(VERSION 3.10)
project(ChatServer)

set(CMAKE_CXX_STANDARD 17)

# Find packages
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread filesystem)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${JSONCPP_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/ChatServer.cpp
    src/CServer.cpp
    src/CSession.cpp
    src/ConfigMgr.cpp
    src/AsioIOServicePool.cpp
    src/LogicSystem.cpp
)

# Libraries
set(LIBRARIES
    ${Boost_LIBRARIES}
    ${JSONCPP_LIBRARIES}
)

add_executable(ChatServer ${SOURCES})
target_link_libraries(ChatServer ${LIBRARIES})
