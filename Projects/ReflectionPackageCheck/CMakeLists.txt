cmake_minimum_required(VERSION 3.21)
project(ReflectionPackageCheck LANGUAGES CXX)

find_package(Reflection 0.1 CONFIG REQUIRED)

if(NOT COMMAND reflection_generate)
    message(FATAL_ERROR "Installed package does not provide reflection_generate()")
endif()

add_library(PackageReflection STATIC)
add_executable(ReflectionPackageCheck main.cpp)

set_target_properties(PackageReflection ReflectionPackageCheck PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(PackageReflection PRIVATE /utf-8)
    target_compile_options(ReflectionPackageCheck PRIVATE /utf-8)
endif()

reflection_generate(
    TARGET PackageReflection
    MODULE PackageCheck
    VISIBILITY PUBLIC
    HEADERS PackageTypes.h
)

target_link_libraries(ReflectionPackageCheck PRIVATE PackageReflection)