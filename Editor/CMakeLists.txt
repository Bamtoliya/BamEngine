project(Editor)

include("${CMAKE_SOURCE_DIR}/Projects/ReflectionCMake/ReflectionBundles.cmake")

find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(efsw CONFIG REQUIRED)

set(EDITOR_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")

# 1. Engine과 동일한 위치(Source/Generated)에 Reflection 파일이 생성되도록 경로 수정
set(EDITOR_GENERATED_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Source/Generated/EditorReflection.gen.cpp")

set(LOCALIZATION_DIR "${CMAKE_SOURCE_DIR}/Resources/Data/Localization")
set(LOCALIZATION_SCRIPT "${CMAKE_SOURCE_DIR}/Utility/CSVToJsonConverter/localizationcsvtojson.py")
file(GLOB LOCALIZATION_CSV_FILES CONFIGURE_DEPENDS "${LOCALIZATION_DIR}/*.csv")

set(LOCALIZATION_OUTPUTS
    "${LOCALIZATION_DIR}/English.json"
    "${LOCALIZATION_DIR}/Korean.json"
)

add_custom_command(
    OUTPUT ${LOCALIZATION_OUTPUTS}
    COMMAND ${Python3_EXECUTABLE} "${LOCALIZATION_SCRIPT}" --input "${LOCALIZATION_DIR}" --output "${LOCALIZATION_DIR}"
    DEPENDS
        "${LOCALIZATION_SCRIPT}"
        ${LOCALIZATION_CSV_FILES}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating localization JSON files"
)

add_custom_target(GenerateLocalization ALL
    DEPENDS ${LOCALIZATION_OUTPUTS}
)

file(GLOB_RECURSE EDITOR_HEADERS CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*.h")
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE EDITOR_INLINE CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*.inl") # 2. Engine처럼 .inl 지원 추가
file(GLOB_RECURSE EDITOR_ALL_FILES CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*")
file(GLOB_RECURSE REFLECTION_CODEGEN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/Projects/ReflectionCodegen/*.py"
)

list(APPEND EDITOR_SOURCES "${EDITOR_GENERATED_SOURCE}")
list(APPEND EDITOR_SOURCES ${EDITOR_INLINE}) # .inl 파일 소스 그룹에 추가

# Visual Studio 폴더 트리를 소스 디렉토리와 동일하게 구성
source_group(TREE "${EDITOR_SOURCE_DIR}" FILES ${EDITOR_HEADERS} ${EDITOR_SOURCES})

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

add_executable(Editor
    ${EDITOR_HEADERS}
    ${EDITOR_SOURCES}
)

add_reflection_bundle(
    TARGET Editor
    MODULE Editor
    SOURCE_ROOT "${EDITOR_SOURCE_DIR}"
    OUTPUT_CPP "${EDITOR_GENERATED_SOURCE}"
    DEPENDS
        "${CMAKE_CURRENT_LIST_FILE}"
        ${EDITOR_HEADERS}
        ${REFLECTION_CODEGEN_SOURCES}
    EXTRA_ARGS
        --strip-namespace Editor
        --strip-namespace Engine
        --namespace-fallback Editor
        --namespace-fallback Engine
        --resource-handle-template ResourceHandle
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

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

add_dependencies(Editor GenerateLocalization)

# 3. Engine과 동일하게 MSVC 최적화 및 빌드 속도 향상 옵션 일치화
if(MSVC)
    set_target_properties(Editor PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    
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

# 4. Engine처럼 Unity Build 활성화 (빌드 속도 향상)
set_target_properties(Editor PROPERTIES
    UNITY_BUILD ON
    FOLDER "Applications"
)
