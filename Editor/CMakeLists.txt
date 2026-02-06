Project("Editor")

find_package(fmt CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(Vulkan REQUIRED) 

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(EDITOR_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(EDITOR_PCH_HEADER "${EDITOR_SOURCE_DIR}/Editor_Includes.h")
set(THIRDPARTY_LIBRARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/")
set(LOCALIZATION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/../Utility/CSVToJsonConverter/localizationcsvtojson.py")
file(GLOB_RECURSE EDITOR_HEADERS CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*.h")
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE EDITOR_ALL_SOURCE_FILES "${EDITOR_SOURCE_DIR}/*")

add_executable(Editor
    ${EDITOR_HEADERS}
    ${EDITOR_SOURCES}
)

# [Hot Reload] Edit and Continue 기능 활성화 (/ZI)
if(MSVC)
    # 컴파일 옵션: 디버그 정보를 생성하여 실행 중 수정 가능하게 함 (/ZI)
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/ZI>)
    
    # 링크 옵션: 증분 링크 활성화 (/INCREMENTAL)
    target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL>)
endif()

set(EDITOR_PUBLIC_INCLUDE_DIRS "")

# 각 파일의 경로를 검사하여 Public 폴더를 찾습니다.
foreach(FILE_PATH ${EDITOR_ALL_SOURCE_FILES})
    # 파일명은 떼고 디렉토리 경로만 가져옵니다.
    get_filename_component(DIR_PATH "${FILE_PATH}" DIRECTORY)
    
    # 경로가 "/Public"으로 끝나는지 확인합니다.
    if(DIR_PATH MATCHES "/Public")
        list(APPEND EDITOR_PUBLIC_INCLUDE_DIRS "${DIR_PATH}")
    endif()
endforeach()

# 중복된 경로는 제거합니다.
if(EDITOR_PUBLIC_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES EDITOR_PUBLIC_INCLUDE_DIRS)
endif()

target_include_directories(Editor PRIVATE 
	${EDITOR_SOURCE_DIR}
	${EDITOR_PUBLIC_INCLUDE_DIRS}
    ${THIRDPARTY_LIBRARY_DIR}
)

add_custom_target(RunLocalizationGenerator ALL
    COMMAND ${Python3_EXECUTABLE} "${LOCALIZATION_SCRIPT}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    COMMENT "Running Localization Generator Script..."
    VERBATIM
)

set_target_properties(Engine PROPERTIES UNITY_BUILD ON)

add_dependencies(Engine RunLocalizationGenerator)

target_precompile_headers(Editor PRIVATE ${EDITOR_PCH_HEADER})

target_link_libraries(Editor PRIVATE
    Engine
    imgui::imgui
    imguizmo::imguizmo
)

if(MSVC)
    set_target_properties(Editor PROPERTIES 
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()

