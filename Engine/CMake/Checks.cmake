add_executable(SceneReflectionCheck examples/SceneReflectionCheck.cpp)
target_link_libraries(SceneReflectionCheck PRIVATE BamEngine::Engine)

if(MSVC)
    target_compile_options(SceneReflectionCheck PRIVATE /utf-8)
endif()