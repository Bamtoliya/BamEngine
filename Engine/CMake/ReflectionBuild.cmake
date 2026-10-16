set(ENGINE_REFLECTION_HEADERS)

# Recheck existing headers when annotations are added or removed.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ENGINE_HEADERS})

foreach(_header IN LISTS ENGINE_HEADERS)
    file(STRINGS "${_header}" _reflection_markers
        LIMIT_COUNT 1
        REGEX "^[ \t]*(CLASS|STRUCT|ENUM|PROPERTY|FUNCTION)[ \t]*\\("
    )

    if(_reflection_markers)
        list(APPEND ENGINE_REFLECTION_HEADERS "${_header}")
    endif()
endforeach()

list(SORT ENGINE_REFLECTION_HEADERS)

if(NOT ENGINE_REFLECTION_HEADERS)
    message(FATAL_ERROR "No engine reflection headers were found")
endif()

set(_engine_parser_options -include "${ENGINE_PCH_HEADER}")

if(WIN32)
    list(APPEND _engine_parser_options
        -DWIN32
        -D_WINDOWS
        -fms-extensions
        "$<$<CONFIG:Debug>:-D_DEBUG>"
    )
endif()

# Conditional members must be generated separately for each build configuration.
set(_engine_generated_directory "${CMAKE_CURRENT_BINARY_DIR}/Generated/$<CONFIG>")

reflection_generate(
    TARGET Engine
    MODULE BamEngine
    TYPE_LIST_METADATA Component
    CXX_STANDARD 23
    REGISTRY_HEADER "${ENGINE_SOURCE_DIR}/Core/Public/Reflection/ReflectionRegistry.h"
    REGISTRY_TYPE Engine::ReflectionRegistry
    HEADERS ${ENGINE_REFLECTION_HEADERS}
    HEADER_INCLUDE_ROOT "${ENGINE_SOURCE_DIR}"
    OUTPUT_DIRECTORY "${_engine_generated_directory}"
    COMPILE_OPTIONS ${_engine_parser_options}
    VISIBILITY PRIVATE
)

# Legacy reflected headers rely on the same PCH as regular engine sources.
set_source_files_properties(
    "${_engine_generated_directory}/BamEngine.gen.cpp"
    PROPERTIES SKIP_PRECOMPILE_HEADERS OFF
)
