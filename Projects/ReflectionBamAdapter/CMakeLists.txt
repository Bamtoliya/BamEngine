project(ReflectionBamAdapter LANGUAGES CXX)

file(GLOB_RECURSE REFLECTION_BAM_ADAPTER_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

file(GLOB_RECURSE REFLECTION_BAM_ADAPTER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(ReflectionBamAdapter STATIC
    ${REFLECTION_BAM_ADAPTER_HEADERS}
    ${REFLECTION_BAM_ADAPTER_SOURCES}
)

add_library(Reflection::BamAdapter ALIAS ReflectionBamAdapter)

target_include_directories(ReflectionBamAdapter
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(ReflectionBamAdapter
    PUBLIC
        cxx_std_23
)

target_link_libraries(ReflectionBamAdapter
    PUBLIC
        Reflection::Core
        BamEngine::Engine
)

set_target_properties(ReflectionBamAdapter PROPERTIES
    FOLDER "Projects/Reflection"
)