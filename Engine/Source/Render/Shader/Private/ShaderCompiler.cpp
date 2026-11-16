#include "ShaderCompiler.h"
#include "Logger.h"

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

std::filesystem::path ShaderCompiler::FindTintPath()
{
    static std::filesystem::path cachedPath;
    if (!cachedPath.empty() && std::filesystem::exists(cachedPath))
        return cachedPath;

    if (const char* tintEnv = std::getenv("TINT_EXECUTABLE"))
    {
        std::filesystem::path candidate = tintEnv;
        if (std::filesystem::exists(candidate))
            return cachedPath = candidate;
    }

    return cachedPath = "tint.exe";
}

bool ShaderCompiler::TranslateSpirvToWgsl(
    const std::filesystem::path& spirvPath,
    const std::filesystem::path& wgslPath,
    std::string& errorMessage)
{
    std::error_code ec;
    if (!std::filesystem::exists(spirvPath, ec))
    {
        errorMessage = "SPIR-V source file not found: " + spirvPath.string();
        return false;
    }

    if (!wgslPath.parent_path().empty())
        std::filesystem::create_directories(wgslPath.parent_path(), ec);
    if (ec)
    {
        errorMessage = "Could not create WGSL output directory: " + wgslPath.parent_path().string();
        return false;
    }

    const std::wstring tintCommand = L"\"" + FindTintPath().wstring() + L"\" --format wgsl \"" +
        spirvPath.wstring() + L"\" -o \"" + wgslPath.wstring() + L"\" 2>&1";
    const std::wstring fullTintCommand = L"\"" + tintCommand + L"\"";
    FILE* tintPipe = _wpopen(fullTintCommand.c_str(), L"rt");
    if (!tintPipe)
    {
        errorMessage = "Failed to launch Tint shader converter.";
        return false;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), tintPipe) != nullptr)
        errorMessage += buffer;

    const int tintExitCode = _pclose(tintPipe);
    if (tintExitCode != 0)
    {
        errorMessage += "Tint exited with code " + std::to_string(tintExitCode) + ".";
        return false;
    }

    if (!std::filesystem::exists(wgslPath, ec) || ec)
    {
        errorMessage += "WGSL output file was not created: " + wgslPath.string();
        return false;
    }

    const auto wgslSize = std::filesystem::file_size(wgslPath, ec);
    if (ec || wgslSize == 0)
    {
        errorMessage += "WGSL output file is empty or unreadable: " + wgslPath.string();
        return false;
    }

    return true;
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
    const std::filesystem::path finalTargetOutPath = (rhiType == ERHIType::WebGPU)
        ? (hlslPath.parent_path().parent_path() / "bin" / "wgsl" / (stem + ".wgsl"))
        : outPath;
    // ── [신규] 타임스탬프 디스크 캐시 적중 검사 (Up-to-Date 시 dxc.exe 실행 100% 바이패스) ──
    if (IsUpToDate(hlslPath, finalTargetOutPath))
    {
        if (ReadCompiledFile(finalTargetOutPath, result.Bytecode, result.ErrorMessage))
        {
            result.Success = true;
            result.OutputPath = finalTargetOutPath;
            return result;
        }
    }
    // ── 캐시 미적중 시: dxc.exe 명령행 인자 구성 및 프로세스 실행 ──
    std::wstring cmd = L"\"" + dxcPath.wstring() + L"\" ";
    if (isDx12)
        cmd += L"-D _DX12_=1 ";
    else
        cmd += L"-spirv ";
    cmd += L"-I \"" + hlslPath.parent_path().wstring() + L"\" ";
    cmd += L"-T " + std::wstring(profile.begin(), profile.end()) + L" ";
    cmd += L"-E " + std::wstring(entryPoint.begin(), entryPoint.end()) + L" ";
    cmd += L"-Fo \"" + outPath.wstring() + L"\" ";
    cmd += L"\"" + hlslPath.wstring() + L"\" 2>&1";
    const std::wstring fullCmd = L"\"" + cmd + L"\"";
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
    // WebGPU WGSL 트랜스레이션 처리
    if (rhiType == ERHIType::WebGPU)
    {
        const std::filesystem::path wgslOutDir = hlslPath.parent_path().parent_path() / "bin" / "wgsl";
        const std::filesystem::path wgslOutPath = wgslOutDir / (stem + ".wgsl");
        if (!TranslateSpirvToWgsl(outPath, wgslOutPath, result.ErrorMessage))
        {
            result.Bytecode.clear();
            return result;
        }
        if (!ReadCompiledFile(wgslOutPath, result.Bytecode, result.ErrorMessage))
            return result;
        result.OutputPath = wgslOutPath;
    }
    else
    {
        if (!ReadCompiledFile(outPath, result.Bytecode, result.ErrorMessage))
            return result;
        result.OutputPath = outPath;
    }
    result.Success = true;
    return result;
}

bool ShaderCompiler::IsUpToDate(const std::filesystem::path& hlslPath, const std::filesystem::path& outPath)
{
    std::error_code ec;
    if (!std::filesystem::exists(outPath, ec)) return false;
    const auto outSize = std::filesystem::file_size(outPath, ec);
    if (ec || outSize == 0) return false;
    const auto outTime = std::filesystem::last_write_time(outPath, ec);
    if (ec) return false;
    const auto hlslTime = std::filesystem::last_write_time(hlslPath, ec);
    if (ec || outTime < hlslTime) return false;
    // 같은 폴더 내의 모든 .hlsli 인클루드 헤더 파일의 수정 시각 대조
    const auto dir = hlslPath.parent_path();
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".hlsli")
        {
            const auto headerTime = entry.last_write_time(ec);
            if (!ec && outTime < headerTime)
                return false;
        }
    }
    return true;
}
bool ShaderCompiler::ReadCompiledFile(const std::filesystem::path& path, std::vector<uint8>& outBytecode, std::string& outErrorMessage)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        outErrorMessage = "Compiled output file could not be opened: " + path.string();
        return false;
    }
    const auto length = static_cast<std::streamoff>(file.tellg());
    if (length <= 0)
    {
        outErrorMessage = "Compiled output file is empty or unreadable: " + path.string();
        return false;
    }
    const auto fileSize = static_cast<std::uintmax_t>(length);
    if (fileSize > outBytecode.max_size() ||
        fileSize > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
    {
        outErrorMessage = "Compiled output file is too large: " + path.string();
        return false;
    }
    outBytecode.resize(static_cast<std::size_t>(fileSize));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(outBytecode.data()), static_cast<std::streamsize>(fileSize));
    if (!file || file.gcount() != static_cast<std::streamsize>(fileSize))
    {
        outBytecode.clear();
        outErrorMessage = "Compiled output file could not be read completely: " + path.string();
        return false;
    }
    return true;
}