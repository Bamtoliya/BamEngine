project(Editor)

# ==============================================================================
# 1. CMake Modules & Dependencies
# ==============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

# 기존 구버전 리플렉션 매크로 로드
include("${CMAKE_SOURCE_DIR}/Projects/ReflectionCMake/ReflectionBundles.cmake")

# 외부 라이브러리 및 Python 로드
include(Dependencies)

# ==============================================================================
# 2. Source Files & Directories Setup
# ==============================================================================
set(EDITOR_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(EDITOR_GENERATED_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Source/Generated/EditorReflection.gen.cpp")

# 모든 소스 파일 스캔
file(GLOB_RECURSE EDITOR_HEADERS CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*.h")
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE EDITOR_INLINE CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*.inl")
file(GLOB_RECURSE EDITOR_ALL_FILES CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*")
file(GLOB_RECURSE REFLECTION_CODEGEN_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Projects/ReflectionAST/*.py")

# 소스 목록 취합 및 IDE 트리 구조 생성
list(APPEND EDITOR_SOURCES "${EDITOR_GENERATED_SOURCE}" ${EDITOR_INLINE})
source_group(TREE "${EDITOR_SOURCE_DIR}" FILES ${EDITOR_HEADERS} ${EDITOR_SOURCES})

# Public 인클루드 디렉토리 추출
set(EDITOR_PUBLIC_INCLUDE_DIRS "")
foreach(FILE_PATH ${EDITOR_ALL_FILES})
    get_filename_component(DIR_PATH "${FILE_PATH}" DIRECTORY)
    if(DIR_PATH MATCHES "/Public$")
        list(APPEND EDITOR_PUBLIC_INCLUDE_DIRS "${DIR_PATH}")
    endif()
endforeach()

if(EDITOR_PUBLIC_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES EDITOR_PUBLIC_INCLUDE_DIRS)
endif()

# ==============================================================================
# 3. Main Target Creation
# ==============================================================================
add_executable(Editor
    ${EDITOR_HEADERS}
    ${EDITOR_SOURCES}
)

# ==============================================================================
# 4. Target Properties & Compilation Settings
# ==============================================================================
set_target_properties(Editor PROPERTIES
    UNITY_BUILD ON
    FOLDER "Applications"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(Editor PRIVATE
        $<$<CONFIG:Debug>:/ZI>
        $<$<CONFIG:Debug>:/FS>
        /Zc:preprocessor
        /MP
    )
    target_link_options(Editor PRIVATE
        $<$<CONFIG:Debug>:/INCREMENTAL>
    )
endif()

# ==============================================================================
# 5. Target Includes & Linking
# ==============================================================================
target_include_directories(Editor
    PUBLIC
        ${EDITOR_PUBLIC_INCLUDE_DIRS}
    PRIVATE
        ${EDITOR_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/ThirdParty/
)

target_link_libraries(Editor
    PRIVATE
        BamEngine::Engine
        TextureConverter
        imgui::imgui
        imguizmo::imguizmo
        assimp::assimp
        efsw::efsw
)

# ==============================================================================
# 6. Custom Build Steps (Reflection & Localization)
# ==============================================================================
# 로컬라이제이션 및 리플렉션 모듈 실행 (반드시 타겟 생성 이후에 와야 함)
include(LocalizationBuild)
include(ReflectionBuild)

# 에디터 빌드 전 다국어 파일(JSON) 생성을 먼저 하도록 종속성 연결
add_dependencies(Editor GenerateLocalization)