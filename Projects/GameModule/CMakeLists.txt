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
)

if(MSVC)
    target_compile_options(GameModule PRIVATE /Zc:preprocessor /utf-8)
endif()