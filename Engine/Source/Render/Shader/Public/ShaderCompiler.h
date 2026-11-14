#pragma once

#include "Engine_Includes.h"
#include "ShaderReflection.h"
#include "RenderTypes.h"
#include "Renderer.h"

BEGIN(Engine)

struct ShaderCompileResult
{
    bool Success = false;
    std::vector<uint8> Bytecode;
    std::string ErrorMessage;
    std::filesystem::path OutputPath;
};

class ENGINE_API ShaderCompiler
{
public:
    // HLSL 파일을 DXIL, SPIR-V 또는 WebGPU용 WGSL로 컴파일합니다.
    static ShaderCompileResult Compile(
        const std::filesystem::path& hlslPath,
        EShaderType shaderType,
        ERHIType rhiType = ERHIType::DirectX12,
        const std::string& entryPoint = "main");

    // dxc.exe 컴파일러 실행 파일 경로 탐색
    static std::filesystem::path FindDxcPath();
    static std::filesystem::path FindTintPath();
    static bool TranslateSpirvToWgsl(
        const std::filesystem::path& spirvPath,
        const std::filesystem::path& wgslPath,
        std::string& errorMessage);

    // 파일명 접미사(VS, PS 등)를 통해 셰이더 스테이지 자동 추론
    static EShaderType DeduceShaderTypeFromPath(const std::filesystem::path& hlslPath);
};

END