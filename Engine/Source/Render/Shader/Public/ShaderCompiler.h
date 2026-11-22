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
        const std::string& entryPoint = "main",
        bool forceRecompile = false);

    // dxc.exe 컴파일러 실행 파일 경로 탐색
    static std::filesystem::path FindDxcPath();
    static std::filesystem::path FindTintPath();
    static bool TranslateSpirvToWgsl(
        const std::filesystem::path& spirvPath,
        const std::filesystem::path& wgslPath,
        std::string& errorMessage);

    // 파일명 접미사(VS, PS 등)를 통해 셰이더 스테이지 자동 추론
    static EShaderType DeduceShaderTypeFromPath(const std::filesystem::path& hlslPath);

    // 파일명이 RTCS로 끝나는 셰이더는 DXR 인라인 RayQuery를 사용하는 Compute 셰이더다.
    // cs_6_5로 DXIL만 생성하며 SPIR-V·WGSL 경로는 지원하지 않는다.
    static bool IsDirectX12OnlyShader(const std::filesystem::path& hlslPath);

private:
    // 산출물이 소스(.hlsl) 및 인클루드 헤더(.hlsli)보다 최신인지 검사합니다.
    static bool IsUpToDate(const std::filesystem::path& hlslPath, const std::filesystem::path& outPath);
    // 컴파일된 바이너리 파일을 읽어 바이트코드 버퍼를 채웁니다.
    static bool ReadCompiledFile(const std::filesystem::path& path, std::vector<uint8>& outBytecode, std::string& outErrorMessage);
};

END