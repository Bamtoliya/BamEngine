# --- CMake/ShaderBuild.cmake ---

# ==============================================================================
# region Shader Compilation (HLSL 분리형 구조 -> DXIL & SPIR-V)
# ==============================================================================
# 1. DXC 컴파일러 찾기
unset(DXC_COMPILER CACHE)
file(GLOB VULKAN_SDK_PATHS_FALLBACK "C:/VulkanSDK/*/Bin")
# 2. 환경변수와 직접 스캔한 경로를 모두 힌트로 던져주고 찾아냅니다.
find_program(DXC_COMPILER 
    NAMES dxc dxc.exe 
    HINTS 
        $ENV{VULKAN_SDK}/Bin
        ${VULKAN_SDK_PATHS_FALLBACK}
    NO_DEFAULT_PATH
)
# 3. 그래도 없으면 시스템 PATH 의존
if(NOT DXC_COMPILER)
    find_program(DXC_COMPILER NAMES dxc dxc.exe)
endif()
# 확인용 로그 출력
message(STATUS "디버그 - 현재 CMake가 인식한 VULKAN_SDK: '$ENV{VULKAN_SDK}'")
message(STATUS "디버그 - 최종적으로 찾은 DXC_COMPILER 경로: '${DXC_COMPILER}'")
# 2. 셰이더 소스 폴더 세팅 (.hlsl 파일만 자동 스캔)
set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Resources/Shader")
file(GLOB_RECURSE HLSL_SOURCES CONFIGURE_DEPENDS "${SHADER_SOURCE_DIR}/*.hlsl")
set(ALL_SHADER_OUTPUTS "")
# 3. 모든 .hlsl 파일을 순회
foreach(SHADER_FILE ${HLSL_SOURCES})
    get_filename_component(SHADER_NAME_WE ${SHADER_FILE} NAME_WE) # 확장자 제외 이름 (예: cubeVS)
    
    # 출력 폴더 세팅 (bin/dxil, bin/spirv 폴더 자동 생성)
    set(DXIL_OUT_DIR "${SHADER_SOURCE_DIR}/bin/dxil")
    set(SPIRV_OUT_DIR "${SHADER_SOURCE_DIR}/bin/spirv")
    file(MAKE_DIRECTORY "${DXIL_OUT_DIR}")
    file(MAKE_DIRECTORY "${SPIRV_OUT_DIR}")
    # 파일 이름을 대문자로 변환하여 비교 준비
    string(TOUPPER "${SHADER_NAME_WE}" UPPER_NAME)
    if(UPPER_NAME MATCHES "VS$" OR UPPER_NAME MATCHES "_VS$")
        # ---------------------------------------------------------
        # [ 파일명 끝이 VS 인 경우 -> Vertex Shader 로 컴파일 ]
        # ---------------------------------------------------------
        set(DXIL_OUT "${DXIL_OUT_DIR}/${SHADER_NAME_WE}.cso")
        set(SPIRV_OUT "${SPIRV_OUT_DIR}/${SHADER_NAME_WE}.spv")
        
        # DX12 컴파일 (-T vs_6_0 -E main)
        add_custom_command(
            OUTPUT ${DXIL_OUT}
            COMMAND ${DXC_COMPILER} -T vs_6_0 -E main -Fo ${DXIL_OUT} ${SHADER_FILE}
            DEPENDS ${SHADER_FILE}
            COMMENT "Compiling ${SHADER_NAME_WE} (DXIL, VS)..."
        )
        # Vulkan 컴파일 (-spirv -T vs_6_0 -E main)
        add_custom_command(
            OUTPUT ${SPIRV_OUT}
            COMMAND ${DXC_COMPILER} -spirv -T vs_6_0 -E main -Fo ${SPIRV_OUT} ${SHADER_FILE}
            DEPENDS ${SHADER_FILE}
            COMMENT "Compiling ${SHADER_NAME_WE} (SPIR-V, VS)..."
        )
        list(APPEND ALL_SHADER_OUTPUTS ${DXIL_OUT} ${SPIRV_OUT})
        
    elseif(UPPER_NAME MATCHES "PS$" OR UPPER_NAME MATCHES "_PS$")
        # ---------------------------------------------------------
        # [ 파일명 끝이 PS 인 경우 -> Pixel Shader 로 컴파일 ]
        # ---------------------------------------------------------
        set(DXIL_OUT "${DXIL_OUT_DIR}/${SHADER_NAME_WE}.cso")
        set(SPIRV_OUT "${SPIRV_OUT_DIR}/${SHADER_NAME_WE}.spv")
        
        # DX12 컴파일 (-T ps_6_0 -E main)
        add_custom_command(
            OUTPUT ${DXIL_OUT}
            COMMAND ${DXC_COMPILER} -T ps_6_0 -E main -Fo ${DXIL_OUT} ${SHADER_FILE}
            DEPENDS ${SHADER_FILE}
            COMMENT "Compiling ${SHADER_NAME_WE} (DXIL, PS)..."
        )
        # Vulkan 컴파일 (-spirv -T ps_6_0 -E main)
        add_custom_command(
            OUTPUT ${SPIRV_OUT}
            COMMAND ${DXC_COMPILER} -spirv -T ps_6_0 -E main -Fo ${SPIRV_OUT} ${SHADER_FILE}
            DEPENDS ${SHADER_FILE}
            COMMENT "Compiling ${SHADER_NAME_WE} (SPIR-V, PS)..."
        )
        list(APPEND ALL_SHADER_OUTPUTS ${DXIL_OUT} ${SPIRV_OUT})
        
    else()
        # 이름 규칙(VS, PS)을 지키지 않은 파일은 무시하고 경고 메시지 출력
        message(WARNING "Shader '${SHADER_NAME_WE}.hlsl' does not end with 'VS' or 'PS'. Skipping compilation.")
    endif()
endforeach()
# 4. 하나의 통합 타겟으로 생성 (빌드 종속성 추가)
add_custom_target(CompileShaders ALL DEPENDS ${ALL_SHADER_OUTPUTS})
# endregion
# ==============================================================================
