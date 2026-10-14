cmake_minimum_required(VERSION 3.21)

project(Archive VERSION 0.1.0 LANGUAGES CXX)

option(ARCHIVE_BUILD_EXAMPLE "Build the JSON archive example" OFF)
option(ARCHIVE_BUILD_GLM_EXAMPLE "Build the GLM consumer example" OFF)
option(ARCHIVE_BUILD_REFLECTION_EXAMPLE "Build the Reflection consumer example" OFF)
option(ARCHIVE_BUILD_GENERATED_EXAMPLE "Build the generated reflection archive example" OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveJson.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveJsonPackage.cmake")

if(ARCHIVE_BUILD_EXAMPLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveExample.cmake")
endif()

if(ARCHIVE_BUILD_GLM_EXAMPLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveGlmExample.cmake")
endif()

if(ARCHIVE_BUILD_REFLECTION_EXAMPLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveReflectionExample.cmake")

    if(ARCHIVE_BUILD_GENERATED_EXAMPLE)
        include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveGeneratedReflectionExample.cmake")
        include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveEngineComponentExample.cmake")
    endif()
endif()