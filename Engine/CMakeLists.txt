project(Engine)

# ==============================================================================
# 1. CMake Modules & Dependencies (모듈 및 종속성 세팅)
# ==============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

include(Dependencies)

# ==============================================================================
# 2. Source Files & Directories (소스 코드 및 경로 수집)
# ==============================================================================
set(ENGINE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(ENGINE_PCH_HEADER "${ENGINE_SOURCE_DIR}/Core/Public/Engine_Includes.h")

# 모든 파일 스캔 (GLOB_RECURSE)
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*.h")
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE ENGINE_INLINE CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*.inl")

option(BAMENGINE_BUILD_WGSL_SHADERS "Generate WGSL shader sidecars with a host Tint executable" OFF)

if(EMSCRIPTEN)
    list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/Render/RHI/Platform/(DirectX12|SDLGPU|Vulkan)/")
    list(FILTER ENGINE_INLINE EXCLUDE REGEX "/Render/RHI/Platform/(DirectX12|SDLGPU|Vulkan)/")
    list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/Core/Diagnostics/Private/CrashHandler_Win32\\.cpp$")
    list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/Render/Shader/Private/ShaderCompiler\\.cpp$")
    list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/Network/")
    list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/Simulation/Private/RLServer\\.cpp$")
    if(BAMENGINE_BUILD_WGSL_SHADERS)
        message(FATAL_ERROR "Generate WGSL shader sidecars with a host build, not the Emscripten toolchain.")
    endif()
endif()

# 소스 목록 취합 및 필터 폴더(Tree) 구조 생성
list(APPEND ENGINE_SOURCES ${ENGINE_INLINE})
source_group(TREE "${ENGINE_SOURCE_DIR}" FILES ${ENGINE_HEADERS} ${ENGINE_SOURCES})

# Public 인클루드 디렉토리 자동 추출 로직
set(ENGINE_PUBLIC_INCLUDE_DIRS "")
foreach(FILE_PATH ${ENGINE_HEADERS} ${ENGINE_INLINE})
    get_filename_component(DIR_PATH "${FILE_PATH}" DIRECTORY)
    if(DIR_PATH MATCHES "/Public(/|$)" AND NOT DIR_PATH MATCHES "/Private(/|$)")
        list(APPEND ENGINE_PUBLIC_INCLUDE_DIRS "${DIR_PATH}")
    endif()
endforeach()

if(ENGINE_PUBLIC_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES ENGINE_PUBLIC_INCLUDE_DIRS)
endif()

# ==============================================================================
# 3. Main Target Creation (엔진 라이브러리 타겟 생성)
# ==============================================================================
if(EMSCRIPTEN)
    add_library(Engine STATIC ${ENGINE_HEADERS} ${ENGINE_SOURCES})
else()
    add_library(Engine SHARED ${ENGINE_HEADERS} ${ENGINE_SOURCES})
endif()
add_library(BamEngine::Engine ALIAS Engine)

# ==============================================================================
# 4. Target Properties & Compile Options (타겟 속성 및 컴파일 플래그)
# ==============================================================================
# Unity Build 최적화 설정
set_target_properties(Engine PROPERTIES UNITY_BUILD ON)

# 컴파일 정의 (전처리기 매크로)
target_compile_definitions(Engine PRIVATE
    ENGINE_EXPORTS
)

if(BAMENGINE_WITH_EDITOR)
    target_compile_definitions(Engine PUBLIC BAM_EDITOR=1)
endif()

set(_engine_glm_definitions
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_LEFT_HANDED
    GLM_ENABLE_EXPERIMENTAL
)

target_compile_definitions(Engine PUBLIC ${_engine_glm_definitions})

# MSVC 전용 컴파일러 옵션
if(MSVC)
    target_compile_options(Engine PRIVATE
        $<$<CONFIG:Debug>:/ZI>
        $<$<CONFIG:Debug>:/FS>
        /MP
        /bigobj
        /Zc:preprocessor
        /utf-8
    )
    target_link_options(Engine PRIVATE
        $<$<CONFIG:Debug>:/INCREMENTAL>
    )
endif()

# ==============================================================================
# 5. Target Includes & Linking (헤더 참조 및 외부 라이브러리 링크)
# ==============================================================================
# 프리컴파일드 헤더(PCH) 적용
target_precompile_headers(Engine PRIVATE ${ENGINE_PCH_HEADER})

# 인클루드 디렉토리 연결
target_include_directories(Engine
    PUBLIC
        ${ENGINE_PUBLIC_INCLUDE_DIRS}
        ${CPPCODEC_INCLUDE_DIRS}
    PRIVATE
        ${ENGINE_SOURCE_DIR}
        "${CMAKE_SOURCE_DIR}/ThirdParty/FMOD/inc"
)

# 외부 라이브러리 연결
target_link_libraries(Engine
    PUBLIC
        Reflection::Runtime
        ReflectionArchive::Adapter
        EnTT::EnTT
        fmt::fmt
        SDL3::SDL3
        glm::glm
        lz4::lz4
        spirv-cross-c
)

if(NOT EMSCRIPTEN)
    target_link_libraries(Engine PUBLIC
        Vulkan::Vulkan
        glslang::glslang
    )
endif()

if(WIN32)
    target_link_libraries(Engine PUBLIC
        Microsoft::DirectX-Headers
        d3d12.lib
        dxgi.lib
        dxguid.lib
        ws2_32.lib
        dbghelp.lib
        $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/ThirdParty/FMOD/lib/x64/fmodL_vc.lib>
        $<$<NOT:$<CONFIG:Debug>>:${CMAKE_SOURCE_DIR}/ThirdParty/FMOD/lib/x64/fmod_vc.lib>
    )
endif()

# ==============================================================================
# 6. Custom Build Steps & Checks
# ==============================================================================
if(NOT EMSCRIPTEN)
    include(ShaderBuild)
endif()
include(ReflectionBuild)

if(NOT EMSCRIPTEN)
    include(LocalizationExport)
    add_dependencies(Engine CompileShaders)
endif()

if(BAMENGINE_BUILD_REFLECTION_CHECK)
    include(Checks)
endif()

option(BAMENGINE_BUILD_GRAPH_CHECK "Build graph document and connection validation check" OFF)
if(BAMENGINE_BUILD_GRAPH_CHECK)
    include(GraphChecks)
endif()

# ==============================================================================
# 7. Post-Build FMOD DLL Copy
# ==============================================================================
if(WIN32)
    add_custom_command(TARGET Engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/ThirdParty/FMOD/lib/x64/fmodL.dll,${CMAKE_SOURCE_DIR}/ThirdParty/FMOD/lib/x64/fmod.dll>"
            "$<TARGET_FILE_DIR:Engine>/"
        COMMENT "Copying FMOD runtime DLL to target output directory"
    )
endif()