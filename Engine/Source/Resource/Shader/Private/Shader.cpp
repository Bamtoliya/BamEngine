#pragma once

#include "Shader.h"
#include "Renderer.h"
#include "RHIShader.h"

#pragma region Constructor&Destructor
EResult Shader::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg))) return EResult::Fail;
	CAST_DESC
	
	m_ShaderType = desc->ShaderType;
	m_EntryPoint = desc->EntryPoint;
	m_SpirvPath = desc->SpirvPath.empty() ? m_Path : desc->SpirvPath;
	m_NumSamplers = desc->NumSamplers;
	m_NumStorageTextures = desc->NumStorageTextures;
	m_NumStorageBuffers = desc->NumStorageBuffers;
	m_NumUniformBuffers = desc->NumUniformBuffers;


	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi) return EResult::Fail;
	tagRHIShaderDesc rhiShaderDesc = BuildRHIShaderDesc();
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

tagRHIShaderDesc Shader::BuildRHIShaderDesc() const
{
	tagRHIShaderDesc desc;
	desc.ShaderType = m_ShaderType;
	desc.EntryPoint = m_EntryPoint;
	desc.FilePath = m_SpirvPath;

	desc.NumSamplers = m_NumSamplers;
	desc.NumStorageTextures = m_NumStorageTextures;
	desc.NumStorageBuffers = m_NumStorageBuffers;
	desc.NumUniformBuffers = m_NumUniformBuffers;
	return desc;
}
#pragma endregion


#pragma region Save&Load
void Shader::Serialize(Archive& ar)
{
	Resource::Serialize(ar);
}

void Shader::Deserialize(Archive& ar)
{
	Resource::Deserialize(ar);

	tagRHIShaderDesc desc = BuildRHIShaderDesc();
	m_RHIShader = Renderer::Get().GetRHI()->CreateShader(desc);
}
#pragma endregion
