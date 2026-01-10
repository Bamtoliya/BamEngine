Project("Editor")

set(EDITOR_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(EDITOR_PCH_HEADER "${EDITOR_SOURCE_DIR}/Editor_Includes.h")

file(GLOB_RECURSE EDITOR_HEADERS CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*.h")
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS "${EDITOR_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE EDITOR_ALL_SOURCE_FILES "${EDITOR_SOURCE_DIR}/*")

add_executable(Editor
    ${EDITOR_HEADERS}
    ${EDITOR_SOURCES}
)

set(EDITOR_PUBLIC_INCLUDE_DIRS "")

# 각 파일의 경로를 검사하여 Public 폴더를 찾습니다.
foreach(FILE_PATH ${EDITOR_ALL_SOURCE_FILES})
    # 파일명은 떼고 디렉토리 경로만 가져옵니다.
    get_filename_component(DIR_PATH "${FILE_PATH}" DIRECTORY)
    
    # 경로가 "/Public"으로 끝나는지 확인합니다.
    if(DIR_PATH MATCHES "/Public$")
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
)

target_precompile_headers(Editor PRIVATE ${EDITOR_PCH_HEADER})

target_link_libraries(Editor PRIVATE Engine)

