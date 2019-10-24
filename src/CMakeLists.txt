cmake_minimum_required(VERSION 3.5)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# set(CMAKE_CXX_FLAGS "-fPIC -std=c++1z -Wall -DPROFILING -DMSHADOW_STAND_ALONE=1")
set(CMAKE_CXX_FLAGS "-fPIC -std=c++1z -Wall -DPROFILING")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb -g ")

# The default, new approach to config the protobuf is using cmake's "config" mode, where
# a set of function (like 'generate_function') can be used to generate the protobuf stubs
# easily. However, those functions are not stable and sometimes generate stubs in unexpected
# location. For now, it's better to use the lecagy "module" mode to fully control over
# protobuf stub generation.
#
# The protobuf installation may disabled the legacy "module" mode. In such a case even
# you specified set(protobuf_MODULE_COMPATIBLE ON), in "config" mode, the compatibility
# will be overriden. See:
# protobuf-options.cmake:6:option(protobuf_MODULE_COMPATIBLE "CMake build-in FindProtobuf.cmake module compatible" OFF)"
# 
# Therefore, we force the cmake system to use the "MODULE" mode.
#
# find_package(Protobuf CONFIG REQUIRED)
find_package(Protobuf MODULE REQUIRED)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
message(STATUS "Using protobuf ${protobuf_VERSION}")

find_package(OpenCV REQUIRED)

# TODO: cmake system in grpc is incomplete we don't check it for now.
# find_package(gRPC CONFIG REQUIRED)
# message(STATUS "Using gRPC ${gRPC_VERSION}")

set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/protos")
set(FUNCTION_TIER_PROTO "${PROTO_PATH}/function_tier.proto")
set(GENERATED_PROTOBUF_PATH "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_PROTOBUF_PATH})

set(FUNCTION_TIER_PB_CPP_FILE "${GENERATED_PROTOBUF_PATH}/function_tier.pb.cc")
set(FUNCTION_TIER_PB_H_FILE "${GENERATED_PROTOBUF_PATH}/function_tier.pb.h")
set(FUNCTION_TIER_GRPC_PB_CPP_FILE "${GENERATED_PROTOBUF_PATH}/function_tier.grpc.pb.cc")
set(FUNCTION_TIER_GRPC_PB_H_FILE "${GENERATED_PROTOBUF_PATH}/function_tier.grpc.pb.h")
add_custom_command(
    OUTPUT "${FUNCTION_TIER_PB_H_FILE}"
           "${FUNCTION_TIER_PB_CPP_FILE}"
           "${FUNCTION_TIER_GRPC_PB_H_FILE}"
           "${FUNCTION_TIER_GRPC_PB_CPP_FILE}"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS "--proto_path=${PROTO_PATH}"
         "--cpp_out=${GENERATED_PROTOBUF_PATH}"
         "${FUNCTION_TIER_PROTO}"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS "--proto_path=${PROTO_PATH}"
         "--grpc_out=${GENERATED_PROTOBUF_PATH}"
         "--plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}"
         "${FUNCTION_TIER_PROTO}"
    DEPENDS "${FUNCTION_TIER_PROTO}"
    )
set(FUNCTION_TIER_PROTO_HDRS ${FUNCTION_TIER_PB_H_FILE} ${FUNCTION_TIER_GRPC_PB_H_FILE})
set(FUNCTION_TIER_PROTO_SRCS ${FUNCTION_TIER_PB_CPP_FILE} ${FUNCTION_TIER_GRPC_PB_CPP_FILE})


add_executable(sospdemo main.cpp derecho-component/function_tier.cpp derecho-component/categorizer_tier.cpp derecho-component/blob.cpp grpc-component/client_logic.cpp grpc-component/function_tier-grpc.cpp mxnet-component/inference_engine.cpp derecho-component/server_logic.cpp ${FUNCTION_TIER_PROTO_SRCS} ${FUNCTION_TIER_PROTO_HDRS})
target_include_directories(sospdemo PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${GENERATED_PROTOBUF_PATH}>
)
target_link_libraries(sospdemo derecho mutils mxnet fabric pthread protobuf grpc++ ${OpenCV_LIBS})
