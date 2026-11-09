set(_engine_source "${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/Source")
set(_engine_entity_public "${_engine_source}/World/Entity/Public")
set(_engine_transform_public "${_engine_source}/World/Transform/Public")
set(_engine_component_include_dirs
    "${_engine_source}/Core/Public"
    "${_engine_source}/Core/Archive/Public"
    "${_engine_source}/Core/Interface/Public"
    "${_engine_source}/Core/Reflection/Public"
    "${_engine_entity_public}"
    "${_engine_transform_public}"
)

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

target_include_directories(ArchiveEngineComponentExample PRIVATE ${_engine_component_include_dirs})

target_link_libraries(ArchiveEngineComponentExample PRIVATE
    ReflectionArchive::Adapter glm::glm-header-only EnTT::EnTT
)

target_compile_definitions(ArchiveEngineComponentExample PRIVATE ${_engine_glm_definitions})
set_target_properties(ArchiveEngineComponentExample PROPERTIES CXX_EXTENSIONS OFF UNITY_BUILD OFF)

reflection_generate(
    TARGET ArchiveEngineComponentExample MODULE EngineComponents
    HEADERS
        "${_engine_entity_public}/NameComponent.h"
        "${_engine_entity_public}/TagComponent.h"
        "${_engine_entity_public}/IDComponent.h"
        "${_engine_entity_public}/FlagComponent.h"
        "${_engine_transform_public}/TransformComponent.h"
        "${_engine_transform_public}/WorldTransformComponent.h"
    INCLUDE_DIRECTORIES ${_engine_glm_include_dirs}
    COMPILE_OPTIONS ${_engine_glm_parser_options}
)

archive_add_example_test(ArchiveEngineComponentExample)
