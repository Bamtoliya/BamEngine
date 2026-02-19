#pragma once

#include "SDLGPUShader.h"
#include "SDLGPURHI.h"

#include <spirv_cross/spirv_cross_c.h>

#pragma region SPIRV Reflection
struct ShaderResourceCounts
{
	uint32 numSamplers = 0;
	uint32 numStorageTextures = 0;
	uint32 numStorageBuffers = 0;
	uint32 numUniformBuffers = 0;
};

static ShaderResourceCounts ReflectSPIRV(const vector<uint8>& bytecode)
{
	ShaderResourceCounts counts = {};

	const uint32_t* spirvData = reinterpret_cast<const uint32_t*>(bytecode.data());
	size_t wordCount = bytecode.size() / sizeof(uint32_t);

	spvc_context context = nullptr;
	spvc_parsed_ir ir = nullptr;
	spvc_compiler compiler = nullptr;
	spvc_resources resources = nullptr;
	const spvc_reflected_resource* resourceList = nullptr;
	size_t resourceCount = 0;

	if (spvc_context_create(&context) != SPVC_SUCCESS)
		return counts;

	if (spvc_context_parse_spirv(context, spirvData, wordCount, &ir) != SPVC_SUCCESS)
	{
		spvc_context_destroy(context);
		return counts;
	}

	if (spvc_context_create_compiler(context, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler) != SPVC_SUCCESS)
	{
		spvc_context_destroy(context);
		return counts;
	}

	if (spvc_compiler_create_shader_resources(compiler, &resources) != SPVC_SUCCESS)
	{
		spvc_context_destroy(context);
		return counts;
	}

	// sampler2D (combined image sampler)
	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, &resourceList, &resourceCount);
	counts.numSamplers = static_cast<uint32>(resourceCount);

	// storage image
	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_IMAGE, &resourceList, &resourceCount);
	counts.numStorageTextures = static_cast<uint32>(resourceCount);

	// storage buffer (SSBO)
	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, &resourceList, &resourceCount);
	counts.numStorageBuffers = static_cast<uint32>(resourceCount);

	// uniform buffer
	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &resourceList, &resourceCount);
	counts.numUniformBuffers = static_cast<uint32>(resourceCount);

	spvc_context_destroy(context);
	return counts;
}
#pragma endregion


#pragma region Constructor&Destructor
EResult SDLGPUShader::Initialize(const DESC& desc)
{
	SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI());

	m_ShaderType = desc.ShaderType;
	m_EntryPoint = desc.EntryPoint;
	m_ShaderBytecode = desc.ShaderBytecode;
	if (!desc.FilePath.empty())
	{
		ifstream shaderFile(desc.FilePath, ios::binary | ios::ate);
		if (!shaderFile.is_open())
		{
			return EResult::FileNotFound;
		}
		size_t fileSize = shaderFile.tellg();
		m_ShaderBytecode.resize(fileSize);
		shaderFile.seekg(0, ios::beg);
		shaderFile.read(reinterpret_cast<char*>(m_ShaderBytecode.data()), fileSize);
		shaderFile.close();
	}

	ShaderResourceCounts counts = ReflectSPIRV(m_ShaderBytecode);

	SDL_GPUShaderCreateInfo createInfo = {};
	createInfo.entrypoint = m_EntryPoint.c_str();
	createInfo.code = m_ShaderBytecode.data();
	createInfo.code_size = m_ShaderBytecode.size();
	createInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
	createInfo.num_samplers = counts.numSamplers;
	createInfo.num_storage_textures = counts.numStorageTextures;
	createInfo.num_storage_buffers = counts.numStorageBuffers;
	createInfo.num_uniform_buffers = counts.numUniformBuffers;

	switch (m_ShaderType)
	{
	case EShaderType::Vertex:
		createInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
		break;
	case EShaderType::Pixel:
		createInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
		break;
	case EShaderType::Compute:
		return EResult::Success;
		break;
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