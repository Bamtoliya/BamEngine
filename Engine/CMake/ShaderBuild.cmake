# --- CMake/ShaderBuild.cmake ---

# ==============================================================================
# region Shader Compilation (HLSL 분리형 구조 -> DXIL & SPIR-V)
# ==============================================================================
# 1. DXC 컴파일러 찾기 (캐시 유지)
file(GLOB VULKAN_SDK_PATHS_FALLBACK "C:/VulkanSDK/*/Bin")
find_program(DXC_COMPILER
    NAMES dxc dxc.exe
    HINTS
        $ENV{VULKAN_SDK}/Bin
        ${VULKAN_SDK_PATHS_FALLBACK}
    NO_DEFAULT_PATH
)
if(NOT DXC_COMPILER)
    find_program(DXC_COMPILER NAMES dxc dxc.exe)
endif()

message(STATUS "디버그 - 현재 CMake가 인식한 VULKAN_SDK: '$ENV{VULKAN_SDK}'")
message(STATUS "디버그 - 최종적으로 찾은 DXC_COMPILER 경로: '${DXC_COMPILER}'")

# 2. 셰이더 소스 폴더 세팅 (.hlsl, .hlsli 스캔)
set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/Shader")
file(GLOB_RECURSE HLSL_SOURCES CONFIGURE_DEPENDS "${SHADER_SOURCE_DIR}/*.hlsl")
file(GLOB_RECURSE HLSL_INCLUDES CONFIGURE_DEPENDS "${SHADER_SOURCE_DIR}/*.hlsli")

# 셰이더 내부의 #include 구문이 변경될 때 CMake가 의존성을 다시 계산하도록 등록
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${HLSL_SOURCES} ${HLSL_INCLUDES})

# 출력 폴더는 루프 밖에서 1회만 생성
set(DXIL_OUT_DIR "${SHADER_SOURCE_DIR}/bin/dxil")
set(SPIRV_OUT_DIR "${SHADER_SOURCE_DIR}/bin/spirv")
file(MAKE_DIRECTORY "${DXIL_OUT_DIR}" "${SPIRV_OUT_DIR}")

# 특정 셰이더 파일이 직접/간접적으로 #include 하는 헤더(.hlsli) 목록만 추출하는 함수
function(collect_hlsl_dependencies ENTRY_FILE OUT_DEPENDENCIES)
    set(_visited "${ENTRY_FILE}")
    set(_queue "${ENTRY_FILE}")

    while(_queue)
        list(POP_FRONT _queue _current_file)
        get_filename_component(_current_dir "${_current_file}" DIRECTORY)

        file(STRINGS "${_current_file}" _include_lines
            REGEX "^[ \t]*#[ \t]*include[ \t]+\"[^\"]+\""
        )

        foreach(_line IN LISTS _include_lines)
            if(_line MATCHES "^[ \t]*#[ \t]*include[ \t]+\"([^\"]+)\"")
                get_filename_component(_inc_path "${_current_dir}/${CMAKE_MATCH_1}" ABSOLUTE)
                if(EXISTS "${_inc_path}")
                    list(FIND _visited "${_inc_path}" _already_visited)
                    if(_already_visited EQUAL -1)
                        list(APPEND _visited "${_inc_path}")
                        list(APPEND _queue "${_inc_path}")
                    endif()
                endif()
            endif()
        endforeach()
    endwhile()

    set(${OUT_DEPENDENCIES} ${_visited} PARENT_SCOPE)
endfunction()

set(ALL_SHADER_OUTPUTS "")

# 3. 모든 .hlsl 파일을 순회하며 실제 참조하는 헤더만 DEPENDS에 연결
foreach(SHADER_FILE ${HLSL_SOURCES})
    get_filename_component(SHADER_NAME_WE "${SHADER_FILE}" NAME_WE)
    string(TOUPPER "${SHADER_NAME_WE}" UPPER_NAME)

    collect_hlsl_dependencies("${SHADER_FILE}" _shader_dependencies)

    if(UPPER_NAME MATCHES "VS$" OR UPPER_NAME MATCHES "_VS$")
        set(DXIL_OUT "${DXIL_OUT_DIR}/${SHADER_NAME_WE}.cso")
        set(SPIRV_OUT "${SPIRV_OUT_DIR}/${SHADER_NAME_WE}.spv")

        add_custom_command(
            OUTPUT "${DXIL_OUT}"
            COMMAND "${DXC_COMPILER}" -D _DX12_=1 -T vs_6_0 -E main -Fo "${DXIL_OUT}" "${SHADER_FILE}"
            DEPENDS ${_shader_dependencies} "${CMAKE_CURRENT_LIST_FILE}"
            COMMENT "Compiling ${SHADER_NAME_WE} (DXIL, VS)..."
        )
        add_custom_command(
            OUTPUT "${SPIRV_OUT}"
            COMMAND "${DXC_COMPILER}" -spirv -T vs_6_0 -E main -Fo "${SPIRV_OUT}" "${SHADER_FILE}"
            DEPENDS ${_shader_dependencies} "${CMAKE_CURRENT_LIST_FILE}"
            COMMENT "Compiling ${SHADER_NAME_WE} (SPIR-V, VS)..."
        )
        list(APPEND ALL_SHADER_OUTPUTS "${DXIL_OUT}" "${SPIRV_OUT}")

    elseif(UPPER_NAME MATCHES "PS$" OR UPPER_NAME MATCHES "_PS$")
        set(DXIL_OUT "${DXIL_OUT_DIR}/${SHADER_NAME_WE}.cso")
        set(SPIRV_OUT "${SPIRV_OUT_DIR}/${SHADER_NAME_WE}.spv")

        add_custom_command(
            OUTPUT "${DXIL_OUT}"
            COMMAND "${DXC_COMPILER}" -D _DX12_=1 -T ps_6_0 -E main -Fo "${DXIL_OUT}" "${SHADER_FILE}"
            DEPENDS ${_shader_dependencies} "${CMAKE_CURRENT_LIST_FILE}"
            COMMENT "Compiling ${SHADER_NAME_WE} (DXIL, PS)..."
        )
        add_custom_command(
            OUTPUT "${SPIRV_OUT}"
            COMMAND "${DXC_COMPILER}" -spirv -T ps_6_0 -E main -Fo "${SPIRV_OUT}" "${SHADER_FILE}"
            DEPENDS ${_shader_dependencies} "${CMAKE_CURRENT_LIST_FILE}"
            COMMENT "Compiling ${SHADER_NAME_WE} (SPIR-V, PS)..."
        )
        list(APPEND ALL_SHADER_OUTPUTS "${DXIL_OUT}" "${SPIRV_OUT}")

    else()
        message(WARNING "Shader '${SHADER_NAME_WE}.hlsl' does not end with 'VS' or 'PS'. Skipping compilation.")
    endif()
endforeach()

# 4. 하나의 통합 타겟으로 생성 (빌드 종속성 추가)
add_custom_target(CompileShaders ALL DEPENDS ${ALL_SHADER_OUTPUTS})
# endregion
# ==============================================================================