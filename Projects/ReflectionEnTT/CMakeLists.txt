cmake_minimum_required(VERSION 3.24)
project(ReflectionEnTT VERSION 0.1.0 LANGUAGES C CXX)

option(REFLECTION_ENTT_BUILD_GENERATOR "Build the Clang-based generator" OFF)
option(REFLECTION_ENTT_BUILD_EXAMPLES "Build executable examples" OFF)
option(REFLECTION_ENTT_BUILD_TESTS "Build integration checks" OFF)
set(REFLECTION_ENTT_GENERATOR_EXECUTABLE "" CACHE FILEPATH "Existing ReflectionEnTTGenerator executable")
set(REFLECTION_ENTT_CLANG_RESOURCE_DIR "" CACHE PATH "Clang resource directory matching the generator")

find_package(EnTT 3.16 CONFIG REQUIRED)
if(EnTT_VERSION VERSION_GREATER_EQUAL 4)
    message(FATAL_ERROR "This version of ReflectionEnTT supports EnTT 3.16.x; EnTT 4 has not been validated")
endif()

add_library(ReflectionEnTTRuntime INTERFACE)
add_library(ReflectionEnTT::Runtime ALIAS ReflectionEnTTRuntime)
target_include_directories(ReflectionEnTTRuntime INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>")
target_compile_features(ReflectionEnTTRuntime INTERFACE cxx_std_20)
target_link_libraries(ReflectionEnTTRuntime INTERFACE EnTT::EnTT)
set_target_properties(ReflectionEnTTRuntime PROPERTIES EXPORT_NAME Runtime)
if(MSVC)
    target_compile_options(ReflectionEnTTRuntime INTERFACE /Zc:preprocessor /utf-8)
endif()

if(REFLECTION_ENTT_BUILD_GENERATOR)
    find_package(LLVM CONFIG REQUIRED)
    find_package(Clang CONFIG REQUIRED)
    if(MSVC AND LLVM_ENABLE_DIA_SDK AND TARGET LLVMDebugInfoPDB)
        find_library(REFLECTION_ENTT_DIA_GUIDS_LIBRARY NAMES diaguids
            HINTS "${CMAKE_GENERATOR_INSTANCE}/DIA SDK/lib/amd64" REQUIRED)
        get_target_property(_pdb_dependencies LLVMDebugInfoPDB INTERFACE_LINK_LIBRARIES)
        set(_fixed_pdb_dependencies)
        foreach(_dependency IN LISTS _pdb_dependencies)
            get_filename_component(_dependency_name "${_dependency}" NAME)
            if(_dependency_name STREQUAL "diaguids.lib")
                list(APPEND _fixed_pdb_dependencies "${REFLECTION_ENTT_DIA_GUIDS_LIBRARY}")
            else()
                list(APPEND _fixed_pdb_dependencies "${_dependency}")
            endif()
        endforeach()
        set_property(TARGET LLVMDebugInfoPDB PROPERTY INTERFACE_LINK_LIBRARIES "${_fixed_pdb_dependencies}")
    endif()

    add_executable(ReflectionEnTTGenerator generator/main.cpp generator/MetadataParser.cpp
        generator/ReflectionUnit.cpp generator/RegistrationEmitter.cpp)
    target_link_libraries(ReflectionEnTTGenerator PRIVATE clangTooling ReflectionEnTT::Runtime)
    target_include_directories(ReflectionEnTTGenerator SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
    separate_arguments(_llvm_definitions NATIVE_COMMAND "${LLVM_DEFINITIONS}")
    target_compile_options(ReflectionEnTTGenerator PRIVATE ${_llvm_definitions})
    set_target_properties(ReflectionEnTTGenerator PROPERTIES CXX_EXTENSIONS OFF UNITY_BUILD OFF)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ReflectionEnTTGenerate.cmake")
if(REFLECTION_ENTT_BUILD_EXAMPLES OR REFLECTION_ENTT_BUILD_TESTS)
    add_executable(ReflectionEnTTExample examples/main.cpp)
    set_target_properties(ReflectionEnTTExample PROPERTIES CXX_EXTENSIONS OFF UNITY_BUILD OFF)
    reflection_entt_generate(TARGET ReflectionEnTTExample MODULE Demo HEADERS examples/ExampleTypes.h)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/ReflectionEnTT")
install(TARGETS ReflectionEnTTRuntime EXPORT ReflectionEnTTTargets)
install(DIRECTORY include/reflection_entt DESTINATION include FILES_MATCHING PATTERN "*.h")
install(EXPORT ReflectionEnTTTargets FILE ReflectionEnTTTargets.cmake
    NAMESPACE ReflectionEnTT:: DESTINATION "${_package_dir}")
configure_package_config_file(cmake/ReflectionEnTTConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/ReflectionEnTTConfig.cmake" INSTALL_DESTINATION "${_package_dir}")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/ReflectionEnTTConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ReflectionEnTTConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ReflectionEnTTConfigVersion.cmake" cmake/ReflectionEnTTGenerate.cmake
    DESTINATION "${_package_dir}")

if(REFLECTION_ENTT_BUILD_TESTS)
    enable_testing()
    if(TARGET ReflectionEnTTGenerator)
        set(_test_generator "$<TARGET_FILE:ReflectionEnTTGenerator>")
    else()
        set(_test_generator "${REFLECTION_ENTT_GENERATOR_EXECUTABLE}")
    endif()
    add_test(NAME ReflectionEnTT.Integration COMMAND ReflectionEnTTExample)
    add_test(NAME ReflectionEnTT.GeneratorFailures COMMAND "${CMAKE_COMMAND}"
        "-DGENERATOR=${_test_generator}" "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}" "-DRESOURCE_DIR=${REFLECTION_ENTT_CLANG_RESOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/GeneratorFailures.cmake")
endif()
