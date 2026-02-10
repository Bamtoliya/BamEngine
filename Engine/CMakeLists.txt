project("Engine")

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

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# 경로 변수 정의
set(ENGINE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(ENGINE_PCH_HEADER "${ENGINE_SOURCE_DIR}/Core/Public/Engine_Includes.h")

set(REFLECTION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/../ReflectionGenerator.py")
set(GENERATED_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/Private/Reflection/Reflection.gen.cpp")

file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*.h")
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${ENGINE_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE ALL_SOURCE_FILES "${ENGINE_SOURCE_DIR}/*")

# 실제 쉐이더 파일 위치: Resources/Shader/
set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/Shader")
file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS
    "${SHADER_SOURCE_DIR}/*.vert"
    "${SHADER_SOURCE_DIR}/*.frag"
)

set(ENGINE_PUBLIC_INCLUDE_DIRS "")
list(APPEND ENGINE_SOURCES ${GENERATED_SOURCE})

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

# [Hot Reload] Edit and Continue 기능 활성화 (/ZI)
if(MSVC)
    # 컴파일 옵션: 디버그 정보를 생성하여 실행 중 수정 가능하게 함 (/ZI)
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/ZI>)
    
    # 링크 옵션: 증분 링크 활성화 (/INCREMENTAL)
    target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL>)
endif()

add_custom_target(RunReflectionGenerator ALL
    COMMAND ${Python3_EXECUTABLE} "${REFLECTION_SCRIPT}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    COMMENT "Running Reflection Generator Script..."
    VERBATIM
)

find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin)

set(SPIRV_OUTPUTS "")
foreach(SHADER ${SHADER_SOURCES})
    # 쉐이더 소스와 같은 디렉토리에 .spv 출력
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
add_dependencies(Engine RunReflectionGenerator CompileShaders)

set_target_properties(Engine PROPERTIES UNITY_BUILD ON)

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
    Vulkan::Vulkan
    glslang::glslang
    spirv-cross-c
    #SPIRV-Tools::SPIRV-Tools         
    #SPIRV-Tools::SPIRV-Tools-opt     
)