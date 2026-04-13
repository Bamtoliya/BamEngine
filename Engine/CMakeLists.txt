project(Engine)

find_package(fmt CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(stb REQUIRED)

find_package(glslang CONFIG REQUIRED)
find_package(SPIRV-Tools CONFIG REQUIRED)
find_package(SPIRV-Tools-opt CONFIG REQUIRED)

find_package(spirv_cross_core CONFIG REQUIRED)
find_package(spirv_cross_glsl CONFIG REQUIRED)
find_package(spirv_cross_hlsl CONFIG REQUIRED)
find_package(spirv_cross_msl CONFIG REQUIRED)
find_package(spirv_cross_cpp CONFIG REQUIRED)
find_package(spirv_cross_reflect CONFIG REQUIRED)
find_package(spirv_cross_c CONFIG REQUIRED)

find_path(CPPCODEC_INCLUDE_DIRS "cppcodec/base64_rfc4648.hpp" REQUIRED)

include("${CMAKE_SOURCE_DIR}/Projects/ReflectionCMake/ReflectionBundles.cmake")

set(ENGINE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(ENGINE_PCH_HEADER "${ENGINE_SOURCE_DIR}/Core/Public/Engine_Includes.h")
set(GENERATED_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/Reflection/Private/Generated/EngineReflection.gen.cpp"
)

file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*.h")
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE ALL_SOURCE_FILES CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*")

list(APPEND ENGINE_SOURCES "${GENERATED_SOURCE}")

source_group(TREE "${ENGINE_SOURCE_DIR}" FILES ${ENGINE_HEADERS} ${ENGINE_SOURCES})

set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/Shader")
file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS
    "${SHADER_SOURCE_DIR}/*.vert"
    "${SHADER_SOURCE_DIR}/*.frag"
)

set(ENGINE_PUBLIC_INCLUDE_DIRS "")
foreach(FILE_PATH ${ALL_SOURCE_FILES})
    get_filename_component(DIR_PATH "${FILE_PATH}" DIRECTORY)
    if(DIR_PATH MATCHES "/Public$")
        list(APPEND ENGINE_PUBLIC_INCLUDE_DIRS "${DIR_PATH}")
    endif()
endforeach()

if(ENGINE_PUBLIC_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES ENGINE_PUBLIC_INCLUDE_DIRS)
endif()

add_library(Engine SHARED
    ${ENGINE_HEADERS}
    ${ENGINE_SOURCES}
)

add_library(BamEngine::Engine ALIAS Engine)

if(MSVC)
    target_compile_options(Engine PRIVATE
        $<$<CONFIG:Debug>:/ZI>
        $<$<CONFIG:Debug>:/FS>
        /Zc:preprocessor
        /MP
    )

    target_link_options(Engine PRIVATE
        $<$<CONFIG:Debug>:/INCREMENTAL>
    )
endif()

find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin)

set(SPIRV_OUTPUTS "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_DIR ${SHADER} DIRECTORY)
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPIRV_OUTPUT "${SHADER_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
        OUTPUT ${SPIRV_OUTPUT}
        COMMAND ${GLSLC} ${SHADER} -o ${SPIRV_OUTPUT}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
    )

    list(APPEND SPIRV_OUTPUTS ${SPIRV_OUTPUT})
endforeach()

add_custom_target(CompileShaders DEPENDS ${SPIRV_OUTPUTS})

add_reflection_bundle(
    TARGET Engine
    MODULE Engine
    SOURCE_ROOT "${ENGINE_SOURCE_DIR}"
    OUTPUT_CPP "${GENERATED_SOURCE}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_dependencies(Engine CompileShaders)

set_target_properties(Engine PROPERTIES
    UNITY_BUILD ON
)

target_compile_definitions(Engine PRIVATE ENGINE_EXPORTS)

target_include_directories(Engine
    PUBLIC
        ${ENGINE_PUBLIC_INCLUDE_DIRS}
        ${CPPCODEC_INCLUDE_DIRS}
    PRIVATE
        ${ENGINE_SOURCE_DIR}
)

target_precompile_headers(Engine PRIVATE ${ENGINE_PCH_HEADER})

target_link_libraries(Engine
    PUBLIC
        Reflection::Core
        fmt::fmt
        SDL3::SDL3
        glm::glm
        Vulkan::Vulkan
        glslang::glslang
        spirv-cross-c
)

# 나중에 Adapter target이 생기면 여기 추가
# target_link_libraries(Engine PUBLIC Reflection::BamAdapter)