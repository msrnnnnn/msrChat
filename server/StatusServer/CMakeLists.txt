cmake_minimum_required(VERSION 3.15)
project(StatusServer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(PREPEND CMAKE_PREFIX_PATH "/usr/local")
set(Protobuf_USE_STATIC_LIBS ON)
set(gRPC_USE_STATIC_LIBS ON)

find_package(Boost REQUIRED COMPONENTS system thread filesystem)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(Threads REQUIRED)

set(SERVER_SOURCES
    src/StatusServer.cpp
    src/StatusServiceImpl.cpp
    ../GateServer/src/ConfigMgr.cpp
)

get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
get_target_property(protoc_location protobuf::protoc LOCATION)

set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../shared/message.proto")
set(PROTO_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set(PROTO_SRCS "${PROTO_SRC_DIR}/message.pb.cc" "${PROTO_SRC_DIR}/message.grpc.pb.cc")
set(PROTO_HDRS "${PROTO_SRC_DIR}/message.pb.h" "${PROTO_SRC_DIR}/message.grpc.pb.h")

add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
    COMMAND ${protoc_location}
    ARGS --grpc_out=${PROTO_SRC_DIR}
         --cpp_out=${PROTO_SRC_DIR}
         --plugin=protoc-gen-grpc=${grpc_cpp_plugin_location}
         -I "${CMAKE_CURRENT_SOURCE_DIR}/../../shared"
         ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
)

list(APPEND SERVER_SOURCES ${PROTO_SRCS})

add_executable(StatusServer ${SERVER_SOURCES})

target_include_directories(StatusServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../GateServer/include
    ${PROTO_SRC_DIR}
    ${Boost_INCLUDE_DIRS}
    ${Protobuf_INCLUDE_DIRS}
)

target_link_libraries(StatusServer PRIVATE
    ${Boost_LIBRARIES}
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc
    gRPC::grpc++_reflection
    Threads::Threads
)

set(CONFIG_FILE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/config.ini")
if(EXISTS "${CONFIG_FILE_SRC}")
    add_custom_command(TARGET StatusServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CONFIG_FILE_SRC}"
        "$<TARGET_FILE_DIR:StatusServer>/config.ini"
    )
endif()
