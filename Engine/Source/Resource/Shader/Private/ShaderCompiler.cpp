#include "ShaderCompiler.h"
#include "Logger.h"
#include <fstream>
#include <cstdlib>

BEGIN(Engine)

std::filesystem::path ShaderCompiler::FindDxcPath()
{
    static std::filesystem::path cachedPath;
    if (!cachedPath.empty() && std::filesystem::exists(cachedPath))
        return cachedPath;

    std::error_code ec;

    // 1. VULKAN_SDK 환경변수 검사
    const char* vulkanSdkEnv = std::getenv("VULKAN_SDK");
    if (vulkanSdkEnv)
    {
        std::filesystem::path candidate = std::filesystem::path(vulkanSdkEnv) / "Bin" / "dxc.exe";
        if (std::filesystem::exists(candidate, ec))
            return cachedPath = candidate;
    }

    // 2. C:/VulkanSDK/*/Bin/dxc.exe 기본 설치 경로 스캔
    const std::filesystem::path defaultSdkRoot = "C:/VulkanSDK";
    if (std::filesystem::exists(defaultSdkRoot, ec))
    {
        for (const auto& entry : std::filesystem::directory_iterator(defaultSdkRoot, ec))
        {
            if (entry.is_directory())
            {
                std::filesystem::path candidate = entry.path() / "Bin" / "dxc.exe";
                if (std::filesystem::exists(candidate, ec))
                    return cachedPath = candidate;
            }
        }
    }

    // 3. 시스템 PATH 폴더의 dxc.exe fallback
    return cachedPath = "dxc.exe";
}

EShaderType ShaderCompiler::DeduceShaderTypeFromPath(const std::filesystem::path& hlslPath)
{
    std::string stem = hlslPath.stem().string();
    for (char& c : stem) c = static_cast<char>(toupper(c));

    if (stem.ends_with("VS") || stem.ends_with("_VS"))
        return EShaderType::Vertex;
    if (stem.ends_with("PS") || stem.ends_with("_PS"))
        return EShaderType::Fragment;
    if (stem.ends_with("CS") || stem.ends_with("_CS"))
        return EShaderType::Compute;

    return EShaderType::Unknown;
}

ShaderCompileResult ShaderCompiler::Compile(
    const std::filesystem::path& hlslPath,
    EShaderType shaderType,
    ERHIType rhiType,
    const std::string& entryPoint)
{
    ShaderCompileResult result;
    std::error_code ec;

    if (!std::filesystem::exists(hlslPath, ec))
    {
        result.ErrorMessage = "HLSL source file not found: " + hlslPath.string();
        return result;
    }

    const std::filesystem::path dxcPath = FindDxcPath();
    const std::string stem = hlslPath.stem().string();

    // 셰이더 타겟 프로파일 결정
    std::string profile;
    switch (shaderType)
    {
    case EShaderType::Vertex: profile = "vs_6_0"; break;
    case EShaderType::Fragment: profile = "ps_6_0"; break;
    case EShaderType::Compute: profile = "cs_6_0"; break;
    default:
        result.ErrorMessage = "Unsupported shader type for: " + hlslPath.string();
        return result;
    }

    // 출력 디렉터리 및 확장자 결정
    const bool isDx12 = (rhiType == ERHIType::DirectX12);
    const std::filesystem::path outDir = isDx12
        ? hlslPath.parent_path().parent_path() / "bin" / "dxil"
        : hlslPath.parent_path().parent_path() / "bin" / "spirv";
    std::filesystem::create_directories(outDir, ec);

    const std::filesystem::path outPath = outDir / (stem + (isDx12 ? ".cso" : ".spv"));

    // dxc.exe 명령행 인자 구성
    std::wstring cmd = L"\"" + dxcPath.wstring() + L"\" ";
    if (isDx12)
        cmd += L"-D _DX12_=1 ";
    else
        cmd += L"-spirv ";

    // .hlsli 인클루드 경로 지정
    cmd += L"-I \"" + hlslPath.parent_path().wstring() + L"\" ";
    cmd += L"-T " + std::wstring(profile.begin(), profile.end()) + L" ";
    cmd += L"-E " + std::wstring(entryPoint.begin(), entryPoint.end()) + L" ";
    cmd += L"-Fo \"" + outPath.wstring() + L"\" ";
    cmd += L"\"" + hlslPath.wstring() + L"\" 2>&1";

    // Windows cmd.exe /c 따옴표 탈락 방지를 위해 전체 명령을 한 번 더 큰따옴표로 감쌈
    const std::wstring fullCmd = L"\"" + cmd + L"\"";

    // 컴파일 프로세스 실행 및 에러 메시지 스트리밍 수집
    FILE* pipe = _wpopen(fullCmd.c_str(), L"rt");
    if (!pipe)
    {
        result.ErrorMessage = "Failed to launch DXC compiler process.";
        return result;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        result.ErrorMessage += buffer;

    const int exitCode = _pclose(pipe);
    if (exitCode != 0)
    {
        result.Success = false;
        return result;
    }

    // 생성된 컴파일 바이너리 파일 읽기
    std::ifstream outFile(outPath, std::ios::binary | std::ios::ate);
    if (!outFile.is_open())
    {
        result.ErrorMessage = "Compiled output file could not be opened: " + outPath.string();
        return result;
    }

    const size_t fileSize = static_cast<size_t>(outFile.tellg());
    result.Bytecode.resize(fileSize);
    outFile.seekg(0, std::ios::beg);
    outFile.read(reinterpret_cast<char*>(result.Bytecode.data()), static_cast<std::streamsize>(fileSize));
    outFile.close();

    result.Success = true;
    result.OutputPath = outPath;
    return result;
}

END