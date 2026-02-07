cmake_minimum_required(VERSION 3.10)
project(ChatServer)

set(CMAKE_CXX_STANDARD 17)

# Find packages
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread filesystem)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(spdlog REQUIRED)

# MySQL Connector/C++
find_path(MYSQL_INCLUDE_DIR mysql_connection.h PATH_SUFFIXES jdbc mysql-cppconn-8)
find_library(MYSQL_LIBRARY NAMES mysqlcppconn)
if (NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "MySQL Connector C++ not found!")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${JSONCPP_INCLUDE_DIRS})
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${gRPC_INCLUDE_DIRS})
include_directories(${MYSQL_INCLUDE_DIR})

# Source files
set(SOURCES
    src/ChatServer.cpp
    src/CServer.cpp
    src/CSession.cpp
    src/ConfigMgr.cpp
    src/AsioIOServicePool.cpp
    src/LogicSystem.cpp
    src/StatusGrpcClient.cpp
    src/MysqlMgr.cpp
    src/MysqlDao.cpp
    message.pb.cc
    message.grpc.pb.cc
)

# Libraries
set(LIBRARIES
    ${Boost_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    protobuf::libprotobuf
    gRPC::grpc++
    ${MYSQL_LIBRARY}
    spdlog::spdlog
)

add_executable(ChatServer ${SOURCES})
target_link_libraries(ChatServer ${LIBRARIES})
