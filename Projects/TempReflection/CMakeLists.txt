cmake_minimum_required(VERSION 3.21)

project(TempReflection LANGUAGES C CXX)

option(
    TEMP_REFLECTION_BUILD_GENERATOR
    "Build the Clang-based reflection generator"
    OFF
)

option(
    TEMP_REFLECTION_BUILD_EXAMPLE
    "Build the reflection example"
    OFF
)

set(
    TEMP_REFLECTION_GENERATOR_EXECUTABLE ""
    CACHE FILEPATH "Existing generator executable used when not building it"
)

set(
    TEMP_REFLECTION_CLANG_RESOURCE_DIR ""
    CACHE PATH "Resource directory belonging to the generator's Clang version"
)

# Runtime

add_library(TempReflectionRuntime STATIC
    src/Metadata.cpp
    src/Registry.cpp
)

add_library(TempReflection::Runtime ALIAS TempReflectionRuntime)

target_include_directories(TempReflectionRuntime PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(TempReflectionRuntime PUBLIC cxx_std_20)

set_target_properties(TempReflectionRuntime PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(TempReflectionRuntime
        PRIVATE /utf-8
        PUBLIC /Zc:preprocessor
    )
endif()

# Generator

if(TEMP_REFLECTION_BUILD_GENERATOR)
    find_package(LLVM CONFIG REQUIRED)
    find_package(Clang CONFIG REQUIRED)

    if(MSVC AND LLVM_ENABLE_DIA_SDK AND TARGET LLVMDebugInfoPDB)
        find_library(TEMP_REFLECTION_DIA_GUIDS_LIBRARY
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
                    "${TEMP_REFLECTION_DIA_GUIDS_LIBRARY}"
                )
            else()
                list(APPEND _fixed_pdb_dependencies "${_dependency}")
            endif()
        endforeach()

        set_property(TARGET LLVMDebugInfoPDB PROPERTY
            INTERFACE_LINK_LIBRARIES "${_fixed_pdb_dependencies}"
        )
    endif()

    add_executable(TempReflectionGenerator
        generator/main.cpp
        generator/MetadataParser.h
        generator/MetadataParser.cpp
        generator/ReflectionModel.h
        generator/ReflectionUnit.h
        generator/ReflectionUnit.cpp
        generator/RegistrationEmitter.h
        generator/RegistrationEmitter.cpp
    )

    target_compile_features(TempReflectionGenerator PRIVATE cxx_std_20)

    set_target_properties(TempReflectionGenerator PROPERTIES
        CXX_EXTENSIONS OFF
    )

    target_include_directories(TempReflectionGenerator SYSTEM PRIVATE
        ${LLVM_INCLUDE_DIRS}
        ${CLANG_INCLUDE_DIRS}
    )

    separate_arguments(
        TEMP_REFLECTION_LLVM_DEFINITIONS
        NATIVE_COMMAND
        "${LLVM_DEFINITIONS}"
    )

    target_compile_options(TempReflectionGenerator PRIVATE
        ${TEMP_REFLECTION_LLVM_DEFINITIONS}
    )

    target_link_libraries(TempReflectionGenerator PRIVATE
        clangTooling
        TempReflection::Runtime
    )

    if(MSVC)
        target_compile_options(TempReflectionGenerator PRIVATE
            /utf-8
            /Zc:preprocessor
        )
    endif()
endif()

# Code generation integration

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/TempReflectionGenerate.cmake")

# Example

if(TEMP_REFLECTION_BUILD_EXAMPLE)
    add_executable(TempReflectionExample examples/main.cpp)

    set_target_properties(TempReflectionExample PROPERTIES
        CXX_EXTENSIONS OFF
    )

    if(MSVC)
        target_compile_options(TempReflectionExample PRIVATE /utf-8)
    endif()

    temp_reflection_generate(
        TARGET TempReflectionExample
        MODULE Demo
        HEADERS
            examples/ExampleTypes.h
            examples/ExtraTypes.h
            examples/DiagnosticsTypes.h
    )

    temp_reflection_generate(
        TARGET TempReflectionExample
        MODULE Failure
        HEADERS examples/FailureTypes.h
    )

endif()