project(ReflectionCore LANGUAGES CXX)

file(GLOB_RECURSE REFLECTION_CORE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

file(GLOB_RECURSE REFLECTION_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(ReflectionCore SHARED
    ${REFLECTION_CORE_HEADERS}
    ${REFLECTION_CORE_SOURCES}
)

add_library(Reflection::Core ALIAS ReflectionCore)

target_include_directories(ReflectionCore
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(ReflectionCore
    PUBLIC
        cxx_std_23
)

target_compile_definitions(ReflectionCore
    PRIVATE
        REFLECTION_CORE_EXPORTS
)

if(MSVC)
    target_compile_options(ReflectionCore PRIVATE
        /utf-8
        /Zc:preprocessor
        /MP
    )
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" FILES ${REFLECTION_CORE_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${REFLECTION_CORE_SOURCES})

set_target_properties(ReflectionCore PROPERTIES
    FOLDER "Projects/Reflection"
)