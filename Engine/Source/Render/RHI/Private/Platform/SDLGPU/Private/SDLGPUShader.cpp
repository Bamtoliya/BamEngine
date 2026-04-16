#pragma once

#include "SDLGPUShader.h"
#include "SDLGPURHI.h"

#pragma region Constructor&Destructor
EResult SDLGPUShader::Initialize(const DESC& desc)
{
    SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI());

    m_ShaderType = desc.ShaderType;
    m_EntryPoint = desc.EntryPoint;
    m_ShaderBytecode = desc.ShaderBytecode;

    if (!desc.FilePath.empty())
    {
        std::filesystem::path shaderPath(desc.FilePath);

        if (shaderPath.is_relative())
        {
            shaderPath = std::filesystem::path(BAMENGINE_PROJECT_ROOT) / shaderPath;
        }

        shaderPath = std::filesystem::weakly_canonical(shaderPath);

        std::ifstream shaderFile(shaderPath, std::ios::binary | std::ios::ate);
        if (!shaderFile.is_open())
        {
            fmt::print(stderr, "Shader open failed\n");
            fmt::print(stderr, "  project  : {}\n", std::filesystem::path(BAMENGINE_PROJECT_ROOT).string());
            fmt::print(stderr, "  requested: {}\n", std::filesystem::path(desc.FilePath).string());
            fmt::print(stderr, "  resolved : {}\n", shaderPath.string());
            return EResult::FileNotFound;
        }

        const size_t fileSize = static_cast<size_t>(shaderFile.tellg());
        m_ShaderBytecode.resize(fileSize);
        shaderFile.seekg(0, std::ios::beg);
        shaderFile.read(reinterpret_cast<char*>(m_ShaderBytecode.data()), static_cast<std::streamsize>(fileSize));
        shaderFile.close();
    }

    ShaderReflectionInfo info = ReflectSPIRV(m_ShaderBytecode);

    SDL_GPUShaderCreateInfo createInfo = {};
    createInfo.entrypoint = m_EntryPoint.c_str();
    createInfo.code = m_ShaderBytecode.data();
    createInfo.code_size = m_ShaderBytecode.size();
    createInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    createInfo.num_samplers = info.NumSamplers;
    createInfo.num_storage_textures = info.NumStorageTextures;
    createInfo.num_storage_buffers = info.NumStorageBuffers;
    createInfo.num_uniform_buffers = info.NumUniformBuffers;

    switch (m_ShaderType)
    {
    case EShaderType::Vertex:
        createInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        break;
    case EShaderType::Pixel:
        createInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        break;
    case EShaderType::Compute:
        return EResult::NotImplemented;
    default:
        return EResult::NotImplemented;
    }

    m_Shader = SDL_CreateGPUShader(device, &createInfo);
    return m_Shader ? EResult::Success : EResult::Fail;
}
SDLGPUShader* SDLGPUShader::Create(RHI* rhi, const DESC& desc)
{
	SDLGPUShader* instance = new SDLGPUShader(rhi);
	if (IsFailure(instance->Initialize(desc)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}
void SDLGPUShader::Free()
{
	if (m_Shader)
	{
		SDL_ReleaseGPUShader(static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI()), m_Shader);
		m_Shader = nullptr;
	}
	m_ShaderBytecode.clear();
	__super::Free();
}