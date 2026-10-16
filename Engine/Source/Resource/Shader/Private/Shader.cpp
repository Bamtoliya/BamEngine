#pragma once

#include "Shader.h"
#include "Renderer.h"
#include "RHIShader.h"

#pragma region Constructor&Destructor
EResult Shader::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg))) return EResult::Fail;
	CAST_DESC
	
	m_ShaderType = desc->shaderType;
	m_EntryPoint = desc->entryPoint;
	m_SpirvPath = desc->spirvPath.empty() ? m_Path : desc->spirvPath;
	m_NumSamplers = desc->numSamplers;
	m_NumStorageTextures = desc->numStorageTextures;
	m_NumStorageBuffers = desc->numStorageBuffers;
	m_NumUniformBuffers = desc->numUniformBuffers;

	m_UniformBuffers = desc->uniformBuffers;
	m_StorageBuffers = desc->storageBuffers;
	m_Samplers = desc->samplers;
	m_StorageTextures = desc->storageTextures;
	m_StageInputs = desc->stageInputs;
	m_StageOutputs = desc->stageOutputs;


	RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;
	if (!rhi) return EResult::Success;
	RHIShaderDesc rhiShaderDesc = BuildRHIShaderDesc();
	m_RHIShader = rhi->CreateShader(rhiShaderDesc);

	return m_RHIShader ? EResult::Success : EResult::Fail;
}

Shader* Shader::Create(void* arg)
{
	Shader* instance = new Shader();
	if(IsFailure(instance->Initialize(arg)))
	{
		delete instance;
		return nullptr;
	}
	return instance;
}

void Shader::Free()
{
	Safe_Release(m_RHIShader);
}
#pragma endregion

#pragma region Getter

RHIShaderDesc Shader::BuildRHIShaderDesc() const
{
	RHIShaderDesc desc;
	desc.shaderType = m_ShaderType;
	desc.entryPoint = m_EntryPoint;
	desc.filePath = m_SpirvPath.empty() ? m_Path : m_SpirvPath;

	desc.numSamplers = m_NumSamplers;
	desc.numStorageTextures = m_NumStorageTextures;
	desc.numStorageBuffers = m_NumStorageBuffers;
	desc.numUniformBuffers = m_NumUniformBuffers;
	return desc;
}

EResult Shader::OnPostLoad()
{
	Safe_Release(m_RHIShader);
	if (m_SpirvPath.empty())
		m_SpirvPath = m_Path;
	RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;
	if (rhi)
	{
		m_RHIShader = rhi->CreateShader(BuildRHIShaderDesc());
		if (!m_RHIShader)
			return EResult::Fail;
	}
	return EResult::Success;
}
#pragma endregion