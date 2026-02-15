cmake_minimum_required(VERSION 3.20)
project(winplay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Protobuf ---
find_package(Protobuf REQUIRED)

set(PROTO_GEN_DIR "${CMAKE_BINARY_DIR}/proto_gen")
file(MAKE_DIRECTORY "${PROTO_GEN_DIR}")

set(PROTO_SRC "${CMAKE_SOURCE_DIR}/proto/tracks.proto")
set(PROTO_GEN_CC "${PROTO_GEN_DIR}/tracks.pb.cc")
set(PROTO_GEN_H  "${PROTO_GEN_DIR}/tracks.pb.h")

add_custom_command(
    OUTPUT "${PROTO_GEN_CC}" "${PROTO_GEN_H}"
    COMMAND protobuf::protoc
        --cpp_out="${PROTO_GEN_DIR}"
        -I "${CMAKE_SOURCE_DIR}/proto"
        "${PROTO_SRC}"
    DEPENDS "${PROTO_SRC}"
    COMMENT "Generating protobuf sources from tracks.proto"
)

add_library(tracks_proto STATIC "${PROTO_GEN_CC}")
target_include_directories(tracks_proto PUBLIC "${PROTO_GEN_DIR}")
target_link_libraries(tracks_proto PUBLIC protobuf::libprotobuf)

# --- WINPLAY executable ---
add_executable(winplay
    src/main.cpp
    src/udp_receiver.cpp
    src/audio_player.cpp
    src/path_translator.cpp
    src/console_status.cpp
)

target_include_directories(winplay PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(winplay PRIVATE
    tracks_proto
    ws2_32
)
