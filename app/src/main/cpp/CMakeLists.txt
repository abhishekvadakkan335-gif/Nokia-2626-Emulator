cmake_minimum_required(VERSION 3.22.1)
project(nokia2626 C)

add_library(unicorn SHARED IMPORTED)

set_target_properties(unicorn PROPERTIES
    IMPORTED_LOCATION
    "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libunicorn.so")

add_library(nokia_engine SHARED native_bridge.c full_probe.c)

target_include_directories(nokia_engine PRIVATE include)

target_link_libraries(nokia_engine unicorn)
