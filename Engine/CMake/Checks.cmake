add_executable(SceneReflectionCheck examples/SceneReflectionCheck.cpp)
target_link_libraries(SceneReflectionCheck PRIVATE BamEngine::Engine)

add_executable(SceneArchiveCheck examples/SceneArchiveCheck.cpp)
target_link_libraries(SceneArchiveCheck PRIVATE BamEngine::Engine)

add_executable(UILayoutCheck examples/UILayoutCheck.cpp)
target_link_libraries(UILayoutCheck PRIVATE BamEngine::Engine)

add_executable(UIInteractionCheck examples/UIInteractionCheck.cpp)
target_link_libraries(UIInteractionCheck PRIVATE BamEngine::Engine)

add_executable(InputManagerCheck examples/InputManagerCheck.cpp)
target_link_libraries(InputManagerCheck PRIVATE BamEngine::Engine)

if(BUILD_TESTING)
    add_test(NAME InputManagerCheck COMMAND InputManagerCheck)
    add_test(NAME UIInteractionCheck COMMAND UIInteractionCheck)
    add_test(NAME UILayoutCheck COMMAND UILayoutCheck)
    add_test(NAME SceneReflectionCheck COMMAND SceneReflectionCheck)
    add_test(NAME SceneArchiveCheck COMMAND SceneArchiveCheck)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/scene-archive-check")
    set_tests_properties(SceneArchiveCheck PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/scene-archive-check")
endif()

if(MSVC)
    target_compile_options(UIInteractionCheck PRIVATE /utf-8)
    target_compile_options(UILayoutCheck PRIVATE /utf-8)
    target_compile_options(SceneReflectionCheck PRIVATE /utf-8)
    target_compile_options(SceneArchiveCheck PRIVATE /utf-8)
endif()
