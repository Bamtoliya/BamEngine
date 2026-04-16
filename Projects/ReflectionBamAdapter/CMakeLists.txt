project(ReflectionBamAdapter LANGUAGES CXX)

find_package(glm CONFIG REQUIRED)

file(GLOB_RECURSE REFLECTION_BAM_ADAPTER_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

file(GLOB_RECURSE REFLECTION_BAM_ADAPTER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(ReflectionBamAdapterObjects OBJECT
    ${REFLECTION_BAM_ADAPTER_HEADERS}
    ${REFLECTION_BAM_ADAPTER_SOURCES}
)

add_library(ReflectionBamAdapter INTERFACE)
add_library(Reflection::BamAdapter ALIAS ReflectionBamAdapter)

target_include_directories(ReflectionBamAdapterObjects
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Public"
        "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Public/Interface"
        "${CMAKE_SOURCE_DIR}/Engine/Source/Resource/Public"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Archive/Public"
)

target_include_directories(ReflectionBamAdapter
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Public"
        "${CMAKE_SOURCE_DIR}/Engine/Source/Core/Public/Interface"
        "${CMAKE_SOURCE_DIR}/Engine/Source/Resource/Public"
)

target_compile_features(ReflectionBamAdapterObjects
    PUBLIC
        cxx_std_23
)

target_compile_features(ReflectionBamAdapter
    INTERFACE
        cxx_std_23
)

if(MSVC)
    target_compile_options(ReflectionBamAdapterObjects PRIVATE
        /utf-8
        /Zc:preprocessor
        /MP
    )
endif()

target_link_libraries(ReflectionBamAdapterObjects
    PUBLIC
        Reflection::Core
        glm::glm
)

target_link_libraries(ReflectionBamAdapter
    INTERFACE
        Reflection::Core
        glm::glm
)

set_target_properties(ReflectionBamAdapterObjects PROPERTIES
    FOLDER "Projects/Reflection"
    POSITION_INDEPENDENT_CODE ON
)

set_target_properties(ReflectionBamAdapter PROPERTIES
    FOLDER "Projects/Reflection"
)

target_compile_definitions(ReflectionBamAdapterObjects PRIVATE
    ENGINE_EXPORTS
    BAMENGINE_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" FILES ${REFLECTION_BAM_ADAPTER_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${REFLECTION_BAM_ADAPTER_SOURCES})