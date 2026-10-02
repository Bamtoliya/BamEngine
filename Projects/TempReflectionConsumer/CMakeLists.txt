cmake_minimum_required(VERSION 3.21)
project(TempReflectionConsumer LANGUAGES CXX)

# 소비 프로젝트에서는 Runtime만 빌드합니다.
set(TEMP_REFLECTION_BUILD_GENERATOR OFF)
set(TEMP_REFLECTION_BUILD_EXAMPLE OFF)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/../TempReflection"
    "${CMAKE_CURRENT_BINARY_DIR}/temp-reflection"
)

add_executable(TempReflectionConsumer main.cpp)

set_target_properties(TempReflectionConsumer PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(TempReflectionConsumer PRIVATE /utf-8)
endif()

temp_reflection_generate(
    TARGET TempReflectionConsumer
    MODULE Consumer
    HEADERS AppTypes.h
)