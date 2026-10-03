cmake_minimum_required(VERSION 3.21)

project(Reflection VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(
    REFLECTION_BUILD_GENERATOR
    "Build the Clang-based reflection generator"
    OFF
)

option(
    REFLECTION_BUILD_EXAMPLE
    "Build the reflection example"
    OFF
)

set(
    REFLECTION_GENERATOR_EXECUTABLE ""
    CACHE FILEPATH "Existing generator executable used when not building it"
)

set(
    REFLECTION_CLANG_RESOURCE_DIR ""
    CACHE PATH "Resource directory belonging to the generator's Clang version"
)

# Runtime

add_library(ReflectionRuntime STATIC
    src/Metadata.cpp
    src/Registry.cpp
)

add_library(Reflection::Runtime ALIAS ReflectionRuntime)

target_include_directories(ReflectionRuntime PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_compile_features(ReflectionRuntime PUBLIC cxx_std_20)

set_target_properties(ReflectionRuntime PROPERTIES
    CXX_EXTENSIONS OFF
    EXPORT_NAME Runtime
)

if(MSVC)
    target_compile_options(ReflectionRuntime
        PRIVATE /utf-8
        PUBLIC /Zc:preprocessor
    )
endif()

# Runtime package

set(_reflection_package_dir
    "${CMAKE_INSTALL_LIBDIR}/cmake/Reflection"
)

install(TARGETS ReflectionRuntime
    EXPORT ReflectionTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/$<CONFIG>"
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/reflection"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT ReflectionTargets
    FILE ReflectionTargets.cmake
    NAMESPACE Reflection::
    DESTINATION "${_reflection_package_dir}"
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReflectionConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ReflectionConfig.cmake"
    INSTALL_DESTINATION "${_reflection_package_dir}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ReflectionConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ReflectionConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ReflectionConfigVersion.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReflectionGenerate.cmake"
    DESTINATION "${_reflection_package_dir}"
)

# Generator

if(REFLECTION_BUILD_GENERATOR)
    find_package(LLVM CONFIG REQUIRED)
    find_package(Clang CONFIG REQUIRED)

    if(MSVC AND LLVM_ENABLE_DIA_SDK AND TARGET LLVMDebugInfoPDB)
        find_library(REFLECTION_DIA_GUIDS_LIBRARY
            NAMES diaguids
            HINTS "${CMAKE_GENERATOR_INSTANCE}/DIA SDK/lib/amd64"
            REQUIRED
        )

        get_target_property(_pdb_dependencies
            LLVMDebugInfoPDB INTERFACE_LINK_LIBRARIES
        )

        set(_fixed_pdb_dependencies)
        foreach(_dependency IN LISTS _pdb_dependencies)
            get_filename_component(_dependency_name "${_dependency}" NAME)

            if(_dependency_name STREQUAL "diaguids.lib")
                list(APPEND _fixed_pdb_dependencies
                    "${REFLECTION_DIA_GUIDS_LIBRARY}"
                )
            else()
                list(APPEND _fixed_pdb_dependencies "${_dependency}")
            endif()
        endforeach()

        set_property(TARGET LLVMDebugInfoPDB PROPERTY
            INTERFACE_LINK_LIBRARIES "${_fixed_pdb_dependencies}"
        )
    endif()

    add_executable(ReflectionGenerator
        generator/main.cpp
        generator/MetadataParser.h
        generator/MetadataParser.cpp
        generator/ReflectionModel.h
        generator/ReflectionUnit.h
        generator/ReflectionUnit.cpp
        generator/RegistrationEmitter.h
        generator/RegistrationEmitter.cpp
    )

    target_compile_features(ReflectionGenerator PRIVATE cxx_std_20)

    set_target_properties(ReflectionGenerator PROPERTIES
        CXX_EXTENSIONS OFF
    )

    target_include_directories(ReflectionGenerator SYSTEM PRIVATE
        ${LLVM_INCLUDE_DIRS}
        ${CLANG_INCLUDE_DIRS}
    )

    separate_arguments(
        REFLECTION_LLVM_DEFINITIONS
        NATIVE_COMMAND
        "${LLVM_DEFINITIONS}"
    )

    target_compile_options(ReflectionGenerator PRIVATE
        ${REFLECTION_LLVM_DEFINITIONS}
    )

    target_link_libraries(ReflectionGenerator PRIVATE
        clangTooling
        Reflection::Runtime
    )

    if(MSVC)
        target_compile_options(ReflectionGenerator PRIVATE
            /utf-8
            /Zc:preprocessor
        )
    endif()
endif()

# Code generation integration

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReflectionGenerate.cmake")

# Example

if(REFLECTION_BUILD_EXAMPLE)
    add_executable(ReflectionExample examples/main.cpp)

    set_target_properties(ReflectionExample PROPERTIES
        CXX_EXTENSIONS OFF
    )

    if(MSVC)
        target_compile_options(ReflectionExample PRIVATE /utf-8)
    endif()

    reflection_generate(
        TARGET ReflectionExample
        MODULE Demo
        HEADERS
            examples/ExampleTypes.h
            examples/ExtraTypes.h
            examples/DiagnosticsTypes.h
    )

    reflection_generate(
        TARGET ReflectionExample
        MODULE Failure
        HEADERS examples/FailureTypes.h
    )

endif()