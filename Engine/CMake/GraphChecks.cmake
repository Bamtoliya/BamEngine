add_executable(GraphFoundationCheck examples/GraphFoundationCheck.cpp)
target_link_libraries(GraphFoundationCheck PRIVATE BamEngine::Engine)

add_executable(GraphReflectionCheck examples/GraphReflectionCheck.cpp)
target_link_libraries(GraphReflectionCheck PRIVATE BamEngine::Engine)
reflection_generate(
    TARGET GraphReflectionCheck
    MODULE GraphReflectionCheck
    CXX_STANDARD 23
    HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/GraphReflectionTypes.h"
        "${ENGINE_SOURCE_DIR}/Graph/Public/GraphLogicLibrary.h"
    VISIBILITY PRIVATE
)

add_executable(GraphExecutionCheck examples/GraphExecutionCheck.cpp)
target_link_libraries(GraphExecutionCheck PRIVATE BamEngine::Engine)
reflection_generate(
    TARGET GraphFoundationCheck
    MODULE GraphFoundationCheck
    CXX_STANDARD 23
    HEADERS
        "${ENGINE_SOURCE_DIR}/Graph/Public/GraphTypes.h"
        "${ENGINE_SOURCE_DIR}/Graph/Public/GraphPin.h"
        "${ENGINE_SOURCE_DIR}/Graph/Public/GraphNode.h"
        "${ENGINE_SOURCE_DIR}/Graph/Public/GraphLink.h"
        "${ENGINE_SOURCE_DIR}/Graph/Public/GraphData.h"
    VISIBILITY PRIVATE
)
reflection_generate(
    TARGET GraphExecutionCheck
    MODULE GraphExecutionCheck
    CXX_STANDARD 23
    HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/GraphReflectionTypes.h"
        "${ENGINE_SOURCE_DIR}/Graph/Public/GraphLogicLibrary.h"
    VISIBILITY PRIVATE
)

if(MSVC)
    target_compile_options(GraphFoundationCheck PRIVATE /utf-8 /Zc:preprocessor)
    target_compile_options(GraphReflectionCheck PRIVATE /utf-8 /Zc:preprocessor)
    target_compile_options(GraphExecutionCheck PRIVATE /utf-8 /Zc:preprocessor)
endif()

if(BUILD_TESTING)
    add_test(NAME GraphFoundationCheck COMMAND GraphFoundationCheck)
    add_test(NAME GraphReflectionCheck COMMAND GraphReflectionCheck)
    add_test(NAME GraphExecutionCheck COMMAND GraphExecutionCheck)
endif()

add_executable(ShaderGraphCheck examples/ShaderGraphCheck.cpp)
target_link_libraries(ShaderGraphCheck PRIVATE BamEngine::Engine)

if(MSVC)
    target_compile_options(ShaderGraphCheck PRIVATE /utf-8 /Zc:preprocessor)
endif()

if(BUILD_TESTING)
    add_test(NAME ShaderGraphCheck COMMAND ShaderGraphCheck)
endif()