get_target_property(_engine_glm_include_dirs glm::glm-header-only INTERFACE_INCLUDE_DIRECTORIES)

set(_engine_reflection_options)
foreach(_definition IN LISTS _engine_glm_definitions)
    list(APPEND _engine_reflection_options "-D${_definition}")
endforeach()

file(GLOB_RECURSE ENGINE_COMPONENT_REFLECTION_HEADERS CONFIGURE_DEPENDS
    "${ENGINE_SOURCE_DIR}/Core/Public/Components/*.h"
)

list(SORT ENGINE_COMPONENT_REFLECTION_HEADERS)

reflection_generate(
    TARGET Engine MODULE BamCoreComponents TYPE_LIST_METADATA Component
    HEADERS ${ENGINE_COMPONENT_REFLECTION_HEADERS}
    HEADER_INCLUDE_ROOT "${ENGINE_SOURCE_DIR}/Core/Public"
    INCLUDE_DIRECTORIES ${ENGINE_PUBLIC_INCLUDE_DIRS} ${_engine_glm_include_dirs}
    COMPILE_OPTIONS ${_engine_reflection_options}
    VISIBILITY PRIVATE
)