add_library(GameplayScore MODULE
    Public/GameplayScoreApi.h
    Private/GameplayScoreModule.cpp
)

target_include_directories(GameplayScore PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Public")
target_link_libraries(GameplayScore PRIVATE BamEngine::Engine)
set_target_properties(GameplayScore PROPERTIES FOLDER "Game/Plugins" PREFIX "")

if(MSVC)
    target_compile_options(GameplayScore PRIVATE /Zc:preprocessor /utf-8)
endif()
