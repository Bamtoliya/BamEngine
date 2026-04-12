#pragma once

#include "ShaderImporter.h"
#include "Shader.h"
#include "ShaderReflection.h"
#include "ResourceManager.h"

namespace
{
    EShaderType InferShaderTypeFromPath(const std::filesystem::path& sourcePath)
    {
        // "default.frag.spv" -> stem() == "default.frag"
        const std::wstring shaderExt = sourcePath.stem().extension().wstring();

        if (shaderExt == L".vert") return EShaderType::Vertex;
        if (shaderExt == L".frag") return EShaderType::Pixel;
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
    if (sourcePath.extension() != L".spv")
        return EResult::InvalidArgument;

    vector<uint8> shaderBytecode;
    if (!ReadFileBytes(sourcePath, shaderBytecode))
        return EResult::FileNotFound;

    ShaderReflectionInfo info = ReflectSPIRV(shaderBytecode);

    
    filesystem::path outputPath = destDir.empty() ? sourcePath.parent_path() : destDir;
    outputPath /= sourcePath.filename();
    outputPath.replace_extension(L".bamshader");

    tagShaderDesc desc = {};
    desc.Path = outputPath.wstring();
    desc.Key = outputPath.stem().wstring();

    desc.SpirvPath = sourcePath.wstring();
    desc.EntryPoint = !info.DefaultEntryPoint.empty() ? info.DefaultEntryPoint : "main";
    desc.ShaderType = (info.DefaultShaderType != EShaderType::Unknown)
        ? info.DefaultShaderType
        : InferShaderTypeFromPath(sourcePath);

    desc.NumSamplers = info.NumSamplers;
    desc.NumStorageTextures = info.NumStorageTextures;
    desc.NumStorageBuffers = info.NumStorageBuffers;
    desc.NumUniformBuffers = info.NumUniformBuffers;

    Shader* shader = Shader::Create(&desc);
    if (!shader)
        return EResult::Fail;

    ResourceManager& rm = ResourceManager::Get();
    EResult saveResult = rm.SaveToBinaryFile(shader, outputPath.wstring());
    rm.DestroyResource(shader);

    return saveResult;
}