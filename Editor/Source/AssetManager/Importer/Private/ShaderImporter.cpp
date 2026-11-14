#pragma once
#include "ShaderImporter.h"
#include "Shader.h"
#include "ShaderCompiler.h"
#include "ShaderReflection.h"
#include "ResourceManager.h"
#include "Logger.h"

namespace
{
    EShaderType InferShaderTypeFromPath(const std::filesystem::path& sourcePath)
    {
        // "default.frag.spv" -> stem() == "default.frag"
        const std::wstring shaderExt = sourcePath.stem().extension().wstring();

        if (shaderExt == L".vert") return EShaderType::Vertex;
        if (shaderExt == L".frag") return EShaderType::Fragment;
        if (shaderExt == L".comp") return EShaderType::Compute;
        if (shaderExt == L".geom") return EShaderType::Geometry;
        if (shaderExt == L".tesc") return EShaderType::Hull;
        if (shaderExt == L".tese") return EShaderType::Domain;

        return EShaderType::Unknown;
    }

    bool ReadFileBytes(const std::filesystem::path& filePath, vector<uint8>& outBytes)
    {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
            return false;

        const std::streamsize fileSize = file.tellg();
        if (fileSize <= 0)
        {
            outBytes.clear();
            return true;
        }

        outBytes.resize(static_cast<size_t>(fileSize));
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(outBytes.data()), fileSize);
        return file.good();
    }
}

EResult ShaderImporter::Import(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg)
{
    std::wstring ext = sourcePath.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    if (ext == L".hlsl")
    {
        const EShaderType shaderType = ShaderCompiler::DeduceShaderTypeFromPath(sourcePath);
        if (shaderType == EShaderType::Unknown)
            return EResult::InvalidArgument;
        const ERHIType rhiType = Renderer::IsCreated() ? Renderer::Get().GetRHIType() : ERHIType::DirectX12;
        const ShaderCompileResult compileResult = ShaderCompiler::Compile(sourcePath, shaderType, rhiType);
        if (!compileResult.Success)
        {
            ENGINE_LOG_ERROR("Failed to compile shader '{}' for import: {}",
                sourcePath.string(), compileResult.ErrorMessage);
            return EResult::Fail;
        }
        const std::wstring stemW = sourcePath.stem().wstring();
        ShaderDesc desc = {};
        desc.Key = L"Resources/Shader/" + stemW;
        desc.shaderType = shaderType;
        desc.dxilPath = L"Resources/Shader/bin/dxil/" + stemW + L".cso";
        desc.spirvPath = L"Resources/Shader/bin/spirv/" + stemW + L".spv";
        desc.wgslPath = L"Resources/Shader/bin/wgsl/" + stemW + L".wgsl";
        if (rhiType == ERHIType::WebGPU)
            desc.wgslPath = compileResult.OutputPath.wstring();
        desc.Path = (rhiType == ERHIType::DirectX12) ? desc.dxilPath :
            (rhiType == ERHIType::WebGPU ? desc.wgslPath : desc.spirvPath);
        ResourceManager& rm = ResourceManager::Get();
        auto existing = rm.GetResourceHandle<Shader>(desc.Key);
        if (existing.IsValid() && existing.Get())
        {
            return existing.Get()->UpdateBytecode(compileResult.Bytecode);
        }
        auto handle = rm.LoadResource<Shader>(&desc);
        return handle.IsValid() ? EResult::Success : EResult::Fail;
    }
    if (ext != L".spv")
        return EResult::InvalidArgument;

    vector<uint8> shaderBytecode;
    if (!ReadFileBytes(sourcePath, shaderBytecode))
        return EResult::FileNotFound;

    ShaderReflectionInfo info = ReflectSPIRV(shaderBytecode);

    const ERHIType rhiType = Renderer::IsCreated() ? Renderer::Get().GetRHIType() : ERHIType::DirectX12;
    filesystem::path outputPath = destDir.empty() ? sourcePath.parent_path() : destDir;
    outputPath /= sourcePath.filename();
    outputPath.replace_extension(L".bamshader");

    ShaderDesc desc = {};
    desc.Path = outputPath.wstring();
    desc.Key = outputPath.stem().wstring();

    desc.spirvPath = sourcePath.wstring();
    auto wgslPath = sourcePath;
    if (wgslPath.parent_path().filename() == L"spirv")
        wgslPath = wgslPath.parent_path().parent_path() / L"wgsl" / wgslPath.filename();
    wgslPath.replace_extension(L".wgsl");
    desc.wgslPath = wgslPath.wstring();
    if (rhiType == ERHIType::WebGPU)
    {
        std::string errorMessage;
        if (!ShaderCompiler::TranslateSpirvToWgsl(sourcePath, wgslPath, errorMessage))
        {
            ENGINE_LOG_ERROR("Failed to translate imported shader '{}' to WGSL: {}",
                sourcePath.string(), errorMessage);
            return EResult::Fail;
        }
    }
    desc.entryPoint = !info.DefaultEntryPoint.empty() ? info.DefaultEntryPoint : "main";
    desc.shaderType = (info.DefaultShaderType != EShaderType::Unknown)
        ? info.DefaultShaderType
        : InferShaderTypeFromPath(sourcePath);

    desc.numSamplers = info.NumSamplers;
    desc.numStorageTextures = info.NumStorageTextures;
    desc.numStorageBuffers = info.NumStorageBuffers;
    desc.numUniformBuffers = info.NumUniformBuffers;

    Shader* shader = Shader::Create(&desc);
    if (!shader)
        return EResult::Fail;

    ResourceManager& rm = ResourceManager::Get();
    EResult saveResult = rm.SaveToBinaryFile(shader, outputPath.wstring());
    rm.DestroyResource(shader);

    return saveResult;
}