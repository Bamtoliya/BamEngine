project(Engine)

find_package(fmt CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(stb REQUIRED)
find_package(lz4 CONFIG REQUIRED)

find_package(directx-headers CONFIG REQUIRED)

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

# [수정] 1. 구버전 리플렉션 번들 스크립트 인클루드 삭제
# include("${CMAKE_SOURCE_DIR}/Projects/ReflectionCMake/ReflectionBundles.cmake")

set(ENGINE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(ENGINE_PCH_HEADER "${ENGINE_SOURCE_DIR}/Core/Public/Engine_Includes.h")

# [수정] 2. 새로 생성되는 리플렉션 파일 경로로 변경
set(GENERATED_SOURCE
    "${ENGINE_SOURCE_DIR}/Core/Reflection/Private/Reflection.gen.cpp"
)

file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*.h")
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE ENGINE_INLINE CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*.inl")

# [수정] 3. 구버전 Codegen 폴더 대신 새로 만든 AST 폴더의 파이썬 파일들을 추적
file(GLOB_RECURSE REFLECTION_AST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Projects/ReflectionAST/*.py")
file(GLOB_RECURSE ALL_SOURCE_FILES CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*")

list(APPEND ENGINE_SOURCES "${GENERATED_SOURCE}")
list(APPEND ENGINE_SOURCES ${ENGINE_INLINE})
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
    $<TARGET_OBJECTS:ReflectionBamAdapterObjects>
 )

add_library(BamEngine::Engine ALIAS Engine)

if(MSVC)
    target_compile_options(Engine PRIVATE
        $<$<CONFIG:Debug>:/ZI>
        $<$<CONFIG:Debug>:/FS>
        #/Zc:preprocessor
        /MP
        /bigobj
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

# 1. 올바른 Resources 폴더 경로(../) 지정
set(SHADER_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/Shader/default.hlsl")
# 2. 런타임에 쉽게 로드하도록 결과물(.cso)도 Resources/Shader 폴더 안에 생성
set(VS_OUTPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/Shader/default_vs.cso")
set(PS_OUTPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/Shader/default_ps.cso")
add_custom_command(
    OUTPUT ${VS_OUTPUT_FILE}
    COMMAND fxc.exe /T vs_5_0 /E VSMain /Fo ${VS_OUTPUT_FILE} ${SHADER_SOURCE_FILE}
    DEPENDS ${SHADER_SOURCE_FILE}
    COMMENT "Compiling Vertex Shader (HLSL)..."
)
add_custom_command(
    OUTPUT ${PS_OUTPUT_FILE}
    COMMAND fxc.exe /T ps_5_0 /E PSMain /Fo ${PS_OUTPUT_FILE} ${SHADER_SOURCE_FILE}
    DEPENDS ${SHADER_SOURCE_FILE}
    COMMENT "Compiling Pixel Shader (HLSL)..."
)
# 3. 이름 중복 방지를 위해 CompileHLSLShaders 로 명명
add_custom_target(CompileHLSLShaders ALL DEPENDS ${VS_OUTPUT_FILE} ${PS_OUTPUT_FILE})

# [수정] 4. 기존 add_reflection_bundle 매크로 대신 새로운 AST 제너레이터를 호출하는 타겟 생성
add_custom_command(
    OUTPUT "${GENERATED_SOURCE}"
    COMMAND python "${CMAKE_SOURCE_DIR}/Projects/ReflectionAST/main.py" --source-root "${ENGINE_SOURCE_DIR}" --output "${GENERATED_SOURCE}"
    DEPENDS ${ENGINE_HEADERS} ${REFLECTION_AST_SOURCES}
    COMMENT "Generating Reflection Data via AST..."
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Projects/ReflectionAST"
)

# [수정] 5. 쉐이더 컴파일과 더불어 리플렉션 코드 생성이 먼저 되도록 종속성 추가
add_custom_target(GenerateReflectionAST DEPENDS "${GENERATED_SOURCE}")
add_dependencies(Engine CompileShaders CompileHLSLShaders GenerateReflectionAST)

set_target_properties(Engine PROPERTIES
    UNITY_BUILD OFF
)

set_source_files_properties(
    Source/ECS/XXX.cpp   # entt 사용 파일들
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE
)

target_compile_definitions(Engine PRIVATE
    ENGINE_EXPORTS
    BAMENGINE_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

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
        Reflection::BamAdapter
        fmt::fmt
        SDL3::SDL3
        glm::glm
        Microsoft::DirectX-Headers
        Vulkan::Vulkan
        glslang::glslang
        spirv-cross-c
        lz4::lz4
        # --- DirectX 12 라이브러리 추가 ---
        d3d12.lib
        dxgi.lib
        dxguid.lib
)