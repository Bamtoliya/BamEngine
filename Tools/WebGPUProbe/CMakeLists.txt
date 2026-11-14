cmake_minimum_required(VERSION 3.21)
project(BamEngineWebGPUProbe LANGUAGES CXX)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "WebGPUProbe must be configured with the Emscripten toolchain.")
endif()

add_executable(WebGPUProbe main.cpp)
target_compile_options(WebGPUProbe PRIVATE --use-port=emdawnwebgpu)
target_link_options(WebGPUProbe PRIVATE --use-port=emdawnwebgpu --closure=1)

set_target_properties(WebGPUProbe PROPERTIES
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/site"
)
