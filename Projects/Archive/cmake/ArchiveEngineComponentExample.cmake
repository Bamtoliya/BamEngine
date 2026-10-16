set(_engine_core_public "${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/Source/Core/Public")
set(_engine_archive_public "${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/Source/Core/Archive/Public")

if(NOT TARGET glm::glm-header-only)
    find_package(glm CONFIG REQUIRED)
endif()

if(NOT TARGET EnTT::EnTT)
    find_package(EnTT CONFIG REQUIRED)
endif()

get_target_property(_engine_glm_include_dirs glm::glm-header-only INTERFACE_INCLUDE_DIRECTORIES)

set(_engine_glm_definitions GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_LEFT_HANDED GLM_ENABLE_EXPERIMENTAL)

set(_engine_glm_parser_options)
foreach(_definition IN LISTS _engine_glm_definitions)
    list(APPEND _engine_glm_parser_options "-D${_definition}")
endforeach()

file(GLOB_RECURSE _archive_engine_component_test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/engine-component-tests/*.cpp"
)

add_executable(ArchiveEngineComponentExample examples/EngineComponentExample.cpp ${_archive_engine_component_test_sources})

target_include_directories(ArchiveEngineComponentExample PRIVATE "${_engine_core_public}" "${_engine_archive_public}")

target_link_libraries(ArchiveEngineComponentExample PRIVATE
    ReflectionArchive::Adapter glm::glm-header-only EnTT::EnTT
)

target_compile_definitions(ArchiveEngineComponentExample PRIVATE ${_engine_glm_definitions})
set_target_properties(ArchiveEngineComponentExample PROPERTIES CXX_EXTENSIONS OFF UNITY_BUILD OFF)

reflection_generate(
    TARGET ArchiveEngineComponentExample MODULE EngineComponents
    HEADERS
        "${_engine_core_public}/Components/NameComponent.h"
        "${_engine_core_public}/Components/TagComponent.h"
        "${_engine_core_public}/Components/IDComponent.h"
        "${_engine_core_public}/Components/FlagComponent.h"
        "${_engine_core_public}/Components/TransformComponent.h"
        "${_engine_core_public}/Components/WorldTransformComponent.h"
    INCLUDE_DIRECTORIES "${_engine_core_public}" ${_engine_glm_include_dirs}
    COMPILE_OPTIONS ${_engine_glm_parser_options}
)

archive_add_example_test(ArchiveEngineComponentExample)
