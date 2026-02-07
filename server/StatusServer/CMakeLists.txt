cmake_minimum_required(VERSION 3.10)
project(StatusServer)

set(CMAKE_CXX_STANDARD 17)

# Find packages
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread filesystem)
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(spdlog REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}) # for pb files in root
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(/usr/include/mysql-cppconn-8) # Typical path for mysql connector
include_directories(/usr/include/jsoncpp) # Typical path for jsoncpp

# Source files
set(SOURCES
    src/StatusServer.cpp
    src/StatusServiceImpl.cpp
    src/ConfigMgr.cpp
    src/RedisMgr.cpp
    src/MysqlMgr.cpp
    src/MysqlDao.cpp
    src/AsioIOServicePool.cpp
    message.pb.cc
    message.grpc.pb.cc
)

# Libraries
set(LIBRARIES
    ${Boost_LIBRARIES}
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf
    mysqlcppconn
    hiredis
    jsoncpp
    spdlog::spdlog
)

add_executable(StatusServer ${SOURCES})
target_link_libraries(StatusServer ${LIBRARIES})
