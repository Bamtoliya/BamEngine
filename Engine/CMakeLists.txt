project("Engine")

find_package(fmt CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Vulkan REQUIRED) 

# 경로 변수 정의
set(ENGINE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(ENGINE_PCH_HEADER "${ENGINE_SOURCE_DIR}/Core/Public/Engine_Includes.h")

file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*.h")
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE ALL_SOURCE_FILES "${ENGINE_SOURCE_DIR}/*")
set(ENGINE_PUBLIC_INCLUDE_DIRS "")

# 2. 각 파일의 경로를 검사합니다.
foreach(FILE_PATH ${ALL_SOURCE_FILES})
    # 파일명은 떼고 디렉토리 경로만 가져옵니다.
    get_filename_component(DIR_PATH "${FILE_PATH}" DIRECTORY)
    
    # 경로가 "/Public"으로 끝나는지 확인합니다. (Windows/Linux 경로 구분자 고려)
    if(DIR_PATH MATCHES "/Public")
        list(APPEND ENGINE_PUBLIC_INCLUDE_DIRS "${DIR_PATH}")
    endif()
endforeach()

# 3. 중복된 경로는 제거합니다.
if(ENGINE_PUBLIC_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES ENGINE_PUBLIC_INCLUDE_DIRS)
endif()

add_library(Engine SHARED
    ${ENGINE_HEADERS}
    ${ENGINE_SOURCES}
)

target_compile_definitions(Engine PRIVATE ENGINE_EXPORTS)

# PUBLIC: Engine 라이브러리를 사용하는 외부 프로젝트에도 노출
target_include_directories(Engine PUBLIC 
    PUBLIC 
        ${ENGINE_PUBLIC_INCLUDE_DIRS}
    PRIVATE
        ${ENGINE_SOURCE_DIR}
)

target_precompile_headers(Engine PRIVATE ${ENGINE_PCH_HEADER})

target_link_libraries(Engine PUBLIC
	fmt::fmt
	SDL3::SDL3
    glm::glm
    imgui::imgui
    Vulkan::Vulkan
)