cmake_minimum_required(VERSION 3.14)
project(tracks LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Find dependencies ---

find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(ESSENTIA REQUIRED essentia)
pkg_check_modules(YAML_CPP REQUIRED yaml-cpp)

find_package(Boost REQUIRED COMPONENTS system program_options)


# --- Generate protobuf C++ sources ---

set(PROTO_SRC_DIR ${CMAKE_SOURCE_DIR}/proto)
set(PROTO_GEN_DIR ${CMAKE_BINARY_DIR}/proto_gen)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(PROTO_FILE ${PROTO_SRC_DIR}/tracks.proto)
set(PROTO_GEN_CC ${PROTO_GEN_DIR}/tracks.pb.cc)
set(PROTO_GEN_H  ${PROTO_GEN_DIR}/tracks.pb.h)

add_custom_command(
    OUTPUT ${PROTO_GEN_CC} ${PROTO_GEN_H}
    COMMAND protobuf::protoc
        --proto_path=${PROTO_SRC_DIR}
        --cpp_out=${PROTO_GEN_DIR}
        ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Generating protobuf sources from tracks.proto"
)

add_library(tracks_proto STATIC ${PROTO_GEN_CC})
target_include_directories(tracks_proto PUBLIC ${PROTO_GEN_DIR})
target_link_libraries(tracks_proto PUBLIC protobuf::libprotobuf)

# --- Core library: tracks_lib (everything except main.cpp) ---

add_library(tracks_lib STATIC
    src/config.cpp
    src/analyzer.cpp
    src/transport.cpp
    src/emitter.cpp
    src/events.cpp
)

target_include_directories(tracks_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${ESSENTIA_INCLUDE_DIRS}
)

target_link_directories(tracks_lib PUBLIC
    ${ESSENTIA_LIBRARY_DIRS}
)

target_link_libraries(tracks_lib PUBLIC
    tracks_proto
    ${ESSENTIA_LIBRARIES}
    ${YAML_CPP_LIBRARIES}
    Boost::system
    Boost::program_options
    pthread
)

# --- Main binary: tracks ---

add_executable(tracks src/main.cpp)
target_link_libraries(tracks PRIVATE tracks_lib)

# --- Test receiver: tracks-recv ---

add_executable(tracks-recv
    recv/main.cpp
)

target_include_directories(tracks-recv PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(tracks-recv PRIVATE
    tracks_proto
    Boost::system
    Boost::program_options
    pthread
)
