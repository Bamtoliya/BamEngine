cmake_minimum_required(VERSION 3.21)

project(Archive VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(CTest)
endif()

option(ARCHIVE_BUILD_EXAMPLE "Build the JSON archive example" OFF)
option(ARCHIVE_BUILD_GLM_EXAMPLE "Build the GLM consumer example" OFF)
option(ARCHIVE_BUILD_REFLECTION_EXAMPLE "Build the Reflection consumer example" OFF)
option(ARCHIVE_BUILD_GENERATED_EXAMPLE "Build the generated reflection archive example" OFF)
option(ARCHIVE_BUILD_ENGINE_COMPONENT_EXAMPLE "Build the BamEngine component consumer example" OFF)

function(archive_add_example_test target)
    if(BUILD_TESTING)
        set(_test_directory "${CMAKE_CURRENT_BINARY_DIR}/tests/${target}")
        file(MAKE_DIRECTORY "${_test_directory}")
        add_test(NAME "${target}" COMMAND "${target}")
        set_tests_properties("${target}" PROPERTIES WORKING_DIRECTORY "${_test_directory}")
    endif()
endfunction()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveJson.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveJsonPackage.cmake")

if(ARCHIVE_BUILD_EXAMPLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveExample.cmake")
endif()

if(ARCHIVE_BUILD_GLM_EXAMPLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveGlmExample.cmake")
endif()

if(ARCHIVE_BUILD_REFLECTION_EXAMPLE OR ARCHIVE_BUILD_GENERATED_EXAMPLE OR ARCHIVE_BUILD_ENGINE_COMPONENT_EXAMPLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveReflectionDependencies.cmake")
endif()

if(ARCHIVE_BUILD_REFLECTION_EXAMPLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveReflectionExample.cmake")
endif()

if(ARCHIVE_BUILD_GENERATED_EXAMPLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveGeneratedReflectionExample.cmake")
endif()

if(ARCHIVE_BUILD_ENGINE_COMPONENT_EXAMPLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveEngineComponentExample.cmake")
endif()
