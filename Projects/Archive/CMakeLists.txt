cmake_minimum_required(VERSION 3.21)

project(Archive VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(ARCHIVE_BUILD_EXAMPLE "Build the JSON archive example" ON)

find_package(glaze CONFIG REQUIRED)

add_library(ArchiveJson INTERFACE)
add_library(Archive::Json ALIAS ArchiveJson)

target_include_directories(ArchiveJson INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

set_target_properties(ArchiveJson PROPERTIES EXPORT_NAME Json)

target_compile_features(ArchiveJson INTERFACE cxx_std_23)
target_link_libraries(ArchiveJson INTERFACE glaze::glaze)

if(MSVC)
    target_compile_options(ArchiveJson INTERFACE /utf-8)
endif()

if(ARCHIVE_BUILD_EXAMPLE)
    add_executable(ArchiveExample examples/main.cpp)
    target_link_libraries(ArchiveExample PRIVATE Archive::Json)
    set_target_properties(ArchiveExample PROPERTIES CXX_EXTENSIONS OFF)
endif()

option(ARCHIVE_BUILD_GLM_EXAMPLE "Build the GLM consumer example" OFF)

if(ARCHIVE_BUILD_GLM_EXAMPLE)
    find_package(glm CONFIG REQUIRED)

    add_executable(ArchiveGlmExample examples/GlmExample.cpp)
    target_link_libraries(ArchiveGlmExample PRIVATE Archive::Json glm::glm-header-only)
    set_target_properties(ArchiveGlmExample PROPERTIES CXX_EXTENSIONS OFF)
endif()

option(ARCHIVE_BUILD_REFLECTION_EXAMPLE "Build the Reflection consumer example" OFF)

if(ARCHIVE_BUILD_REFLECTION_EXAMPLE)
    if(NOT TARGET Reflection::Runtime)
        add_subdirectory(
            "${CMAKE_CURRENT_SOURCE_DIR}/../Reflection"
            "${CMAKE_CURRENT_BINARY_DIR}/reflection"
        )
    endif()

    if(NOT TARGET ReflectionArchive::Adapter)
        add_subdirectory(
            "${CMAKE_CURRENT_SOURCE_DIR}/../ReflectionArchive"
            "${CMAKE_CURRENT_BINARY_DIR}/reflection-archive"
        )
    endif()

    add_executable(ArchiveReflectionExample examples/ReflectionExample.cpp)
    target_link_libraries(ArchiveReflectionExample PRIVATE ReflectionArchive::Adapter)
    set_target_properties(ArchiveReflectionExample PROPERTIES CXX_EXTENSIONS OFF)

    if(ARCHIVE_BUILD_GLM_EXAMPLE)
        target_link_libraries(ArchiveReflectionExample PRIVATE glm::glm-header-only)
        target_compile_definitions(ArchiveReflectionExample PRIVATE ARCHIVE_EXAMPLE_GLM)
    endif()

    option(ARCHIVE_BUILD_GENERATED_EXAMPLE "Build the generated reflection archive example" OFF)

    if(ARCHIVE_BUILD_GENERATED_EXAMPLE)
        add_executable(ArchiveGeneratedReflectionExample examples/GeneratedReflectionExample.cpp)
        target_link_libraries(ArchiveGeneratedReflectionExample PRIVATE ReflectionArchive::Adapter)
        set_target_properties(ArchiveGeneratedReflectionExample PROPERTIES CXX_EXTENSIONS OFF)

        reflection_generate(
            TARGET ArchiveGeneratedReflectionExample
            MODULE ArchiveDemo
            HEADERS examples/GeneratedArchiveTypes.h
        )

                set(_engine_core_public "${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/Source/Core/Public")

        if(NOT TARGET glm::glm-header-only)
            find_package(glm CONFIG REQUIRED)
        endif()

        get_target_property(_engine_glm_include_dirs
            glm::glm-header-only INTERFACE_INCLUDE_DIRECTORIES)

        set(_engine_glm_definitions
            GLM_FORCE_DEPTH_ZERO_TO_ONE
            GLM_FORCE_LEFT_HANDED
            GLM_ENABLE_EXPERIMENTAL
        )

        set(_engine_glm_parser_options)
        foreach(_definition IN LISTS _engine_glm_definitions)
            list(APPEND _engine_glm_parser_options "-D${_definition}")
        endforeach()

        if(NOT TARGET EnTT::EnTT)
            find_package(EnTT CONFIG REQUIRED)
        endif()

        add_executable(ArchiveEngineComponentExample examples/EngineComponentExample.cpp)

        target_include_directories(ArchiveEngineComponentExample PRIVATE "${_engine_core_public}")

        target_link_libraries(ArchiveEngineComponentExample PRIVATE
            ReflectionArchive::Adapter
            glm::glm-header-only
            EnTT::EnTT
        )

        target_compile_definitions(ArchiveEngineComponentExample PRIVATE ${_engine_glm_definitions})
        set_target_properties(ArchiveEngineComponentExample PROPERTIES CXX_EXTENSIONS OFF)

        reflection_generate(
            TARGET ArchiveEngineComponentExample
            MODULE EngineComponents
            HEADERS
                "${_engine_core_public}/Components/NameComponent.h"
                "${_engine_core_public}/Components/TagComponent.h"
                "${_engine_core_public}/Components/IDComponent.h"
                "${_engine_core_public}/Components/FlagComponent.h"
                "${_engine_core_public}/Components/TransformComponent.h"
                "${_engine_core_public}/Components/WorldTransformComponent.h"
            INCLUDE_DIRECTORIES
                "${_engine_core_public}"
                ${_engine_glm_include_dirs}
            COMPILE_OPTIONS ${_engine_glm_parser_options}
        )

    endif()
endif()

set(_archive_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/Archive")

set(_engine_archive_public "${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/Source/Core/Archive/Public")

foreach(_consumer_target IN ITEMS ArchiveGlmExample ArchiveEngineComponentExample)
    if(TARGET "${_consumer_target}")
        target_include_directories("${_consumer_target}" PRIVATE "${_engine_archive_public}")
    endif()
endforeach()

if(TARGET ArchiveReflectionExample AND ARCHIVE_BUILD_GLM_EXAMPLE)
    target_include_directories(ArchiveReflectionExample PRIVATE "${_engine_archive_public}")
endif()

install(TARGETS ArchiveJson EXPORT ArchiveTargets)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/archive"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT Archive
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "ArchiveInterface.h" EXCLUDE
)

install(EXPORT ArchiveTargets
    FILE ArchiveTargets.cmake
    NAMESPACE Archive::
    DESTINATION "${_archive_package_dir}"
    COMPONENT Archive
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchiveConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ArchiveConfig.cmake"
    INSTALL_DESTINATION "${_archive_package_dir}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ArchiveConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ArchiveConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ArchiveConfigVersion.cmake"
    DESTINATION "${_archive_package_dir}"
    COMPONENT Archive
)