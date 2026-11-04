# Projects/GameModule/CMakeLists.txt
project(GameModule)

set(GAME_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")

file(GLOB_RECURSE GAME_HEADERS CONFIGURE_DEPENDS "${GAME_SOURCE_DIR}/*.h")
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${GAME_SOURCE_DIR}/*.cpp")

add_library(GameModule SHARED
    ${GAME_HEADERS}
    ${GAME_SOURCES}
)

target_link_libraries(GameModule PRIVATE
    BamEngine::Engine
)

set_target_properties(GameModule PROPERTIES
    FOLDER "Game"
    UNITY_BUILD OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MSVC)
    target_compile_options(GameModule PRIVATE /Zc:preprocessor /utf-8)
endif()

# ==============================================================================
# GameModule 전용 프리컴파일드 헤더 (PCH) 설정
# ==============================================================================
target_precompile_headers(GameModule PRIVATE
    <vector>
    <string>
    <string_view>
    <unordered_map>
    <functional>
    <Engine_API.h>
    <Logger.h>
    <SystemManager.h>
    <Reflection/ReflectionRegistry.h>
)

# ==============================================================================
# GameModule 전용 리플렉션 자동 생성 (항상 구동하여 헤더 누락 방지)
# ==============================================================================
set(_game_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/Generated/$<CONFIG>")
target_include_directories(GameModule PRIVATE "${_game_generated_dir}")

set(GAME_REFLECTION_HEADERS "${GAME_SOURCE_DIR}/GameGameplayLibrary.h")

reflection_generate(
    TARGET GameModule
    MODULE GameModule
    TYPE_LIST_METADATA Component
    CXX_STANDARD 23
    HEADERS ${GAME_REFLECTION_HEADERS}
    HEADER_INCLUDE_ROOT "${GAME_SOURCE_DIR}"
    OUTPUT_DIRECTORY "${_game_generated_dir}"
    VISIBILITY PRIVATE
)