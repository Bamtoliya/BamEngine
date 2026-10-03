cmake_minimum_required(VERSION 3.21)
project(ReflectionConsumer LANGUAGES CXX)

# 소비 프로젝트에서는 Runtime만 빌드합니다.
set(REFLECTION_BUILD_GENERATOR OFF)
set(REFLECTION_BUILD_EXAMPLE OFF)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/../Reflection"
    "${CMAKE_CURRENT_BINARY_DIR}/reflection"
)

add_library(ConsumerReflection STATIC)
add_executable(ReflectionConsumer main.cpp)

set_target_properties(ConsumerReflection ReflectionConsumer PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(ConsumerReflection PRIVATE /utf-8)
    target_compile_options(ReflectionConsumer PRIVATE /utf-8)
endif()

reflection_generate(
    TARGET ConsumerReflection
    MODULE Consumer
    VISIBILITY PUBLIC
    HEADERS AppTypes.h
)

target_link_libraries(ReflectionConsumer PRIVATE ConsumerReflection)