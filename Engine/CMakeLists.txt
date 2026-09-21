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
set(GENERATED_SOURCE "${ENGINE_SOURCE_DIR}/Core/Reflection/Private/Reflection.gen.cpp")

# 모든 파일 스캔 (GLOB_RECURSE)
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*.h")
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE ENGINE_INLINE CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*.inl")
file(GLOB_RECURSE ALL_SOURCE_FILES CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*")
file(GLOB_RECURSE REFLECTION_AST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Projects/ReflectionAST/*.py")

# 소스 목록 취합 및 필터 폴더(Tree) 구조 생성
list(APPEND ENGINE_SOURCES "${GENERATED_SOURCE}" ${ENGINE_INLINE})
source_group(TREE "${ENGINE_SOURCE_DIR}" FILES ${ENGINE_HEADERS} ${ENGINE_SOURCES})

# Public 인클루드 디렉토리 자동 추출 로직
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

# ==============================================================================
# 3. Main Target Creation (엔진 라이브러리 타겟 생성)
# ==============================================================================
add_library(Engine SHARED
    ${ENGINE_HEADERS}
    ${ENGINE_SOURCES}
    $<TARGET_OBJECTS:ReflectionBamAdapterObjects>
)
add_library(BamEngine::Engine ALIAS Engine)

# ==============================================================================
# 4. Target Properties & Compile Options (타겟 속성 및 컴파일 플래그)
# ==============================================================================
# Unity Build 최적화 설정
set_target_properties(Engine PROPERTIES UNITY_BUILD OFF)
set_source_files_properties(Source/ECS/XXX.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE)

# 컴파일 정의 (전처리기 매크로)
target_compile_definitions(Engine PRIVATE
    ENGINE_EXPORTS
    BAMENGINE_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

# MSVC 전용 컴파일러 옵션
if(MSVC)
    target_compile_options(Engine PRIVATE
        $<$<CONFIG:Debug>:/ZI>
        $<$<CONFIG:Debug>:/FS>
        /MP
        /bigobj
        /Zc:preprocessor
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
)

# 외부 라이브러리 연결
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
        d3d12.lib
        dxgi.lib
        dxguid.lib
)

# ==============================================================================
# 6. Custom Build Steps (커스텀 타겟 및 종속성 연결)
# ==============================================================================
# 외부 스크립트 모듈 실행 (타겟 생성)
include(ShaderBuild)
include(ReflectionBuild)

# [가장 중요] 엔진 빌드 시작 '전'에 셰이더와 리플렉션 코드를 먼저 굽도록 종속성 강제
add_dependencies(Engine CompileShaders GenerateReflectionAST)