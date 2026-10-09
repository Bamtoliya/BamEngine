project(Editor)

# ==============================================================================
# 1. CMake Modules & Dependencies
# ==============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

# 외부 라이브러리 및 Python 로드
include(Dependencies)

# ==============================================================================
# 2. Source Files & Directories Setup
# ==============================================================================
set(EDITOR_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")

# 모든 소스 파일 스캔
file(GLOB_RECURSE EDITOR_HEADERS CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*.h")
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE EDITOR_INLINE CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*.inl")
file(GLOB_RECURSE EDITOR_ALL_FILES CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*")

# 소스 목록 취합 및 IDE 트리 구조 생성
list(APPEND EDITOR_SOURCES ${EDITOR_INLINE})
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
# 6. Custom Build Steps & Checks
# ==============================================================================
include(LocalizationBuild)

add_dependencies(Editor GenerateLocalization)

if(BAMENGINE_BUILD_REFLECTION_CHECK)
    include(Checks)
endif()