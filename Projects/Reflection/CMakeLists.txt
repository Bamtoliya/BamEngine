cmake_minimum_required(VERSION 3.21)

project(Reflection VERSION 0.1.0 LANGUAGES C CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif()

option(REFLECTION_BUILD_EXAMPLE "Build the reflection example" OFF)

set(REFLECTION_GENERATOR_EXECUTABLE "" CACHE FILEPATH "Existing generator executable used when not building it")
set(REFLECTION_CLANG_RESOURCE_DIR "" CACHE PATH "Resource directory belonging to the generator's Clang version")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReflectionRuntime.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReflectionRuntimePackage.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReflectionGenerator.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReflectionGenerate.cmake")

if(REFLECTION_BUILD_EXAMPLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReflectionExample.cmake")
endif()