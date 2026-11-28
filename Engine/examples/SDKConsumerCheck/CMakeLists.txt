cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

project(SDKConsumerCheck LANGUAGES CXX)

if(NOT DEFINED BAMENGINE_ROOT)
    message(FATAL_ERROR "BAMENGINE_ROOT is required.")
endif()

find_package(BamEngine CONFIG REQUIRED
    PATHS "${BAMENGINE_ROOT}/Engine/CMake"
    NO_DEFAULT_PATH
)

add_executable(SDKConsumerCheck main.cpp)

target_link_libraries(SDKConsumerCheck PRIVATE BamEngine::Engine)

get_property(_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(_multi_config)
    list(GET CMAKE_CONFIGURATION_TYPES 0 _config)
else()
    set(_config "${CMAKE_BUILD_TYPE}")
endif()

string(TOUPPER "${_config}" _config_upper)

get_target_property(_implib BamEngine::Engine
    "IMPORTED_IMPLIB_${_config_upper}")

get_target_property(_dll BamEngine::Engine
    "IMPORTED_LOCATION_${_config_upper}")

get_target_property(_definitions BamEngine::Engine
    INTERFACE_COMPILE_DEFINITIONS)

if(NOT EXISTS "${_implib}")
    message(FATAL_ERROR "Missing Engine import library: ${_implib}")
endif()

if(NOT EXISTS "${_dll}")
    message(FATAL_ERROR "Missing Engine DLL: ${_dll}")
endif()

message(STATUS "[SDK Check] Import library: ${_implib}")
message(STATUS "[SDK Check] DLL: ${_dll}")
message(STATUS "[SDK Check] Definitions: ${_definitions}")