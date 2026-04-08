#pragma once

#include "Shader.h"
#include "Renderer.h"
#include "RHIShader.h"

#pragma region Constructor&Destructor
EResult Shader::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg))) return EResult::Fail;

	CAST_DESC
	RHI* rhi = Renderer::Get().GetRHI();
	tagRHIShaderDesc rhiShaderDesc;
	rhiShaderDesc.ShaderType = desc->ShaderType;
	rhiShaderDesc.FilePath = desc->Path;
	rhiShaderDesc.EntryPoint = desc->EntryPoint;
	m_RHIShader = rhi->CreateShader(rhiShaderDesc);
	if (!m_RHIShader)
	{
		return EResult::Fail;
	}
	return EResult::Success;
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


#pragma region Shader
void Shader::Deserialize(Archive& ar)
{
	//Shader* shader = ResourceManager::Get().GetShader(m_Tag);
	//if (shader)
	//{
	//	m_RHIShader->SetNativeHandle(shader->GetRHIShader()->GetNativeHandle());
	//}
	//else
	//{
	//
	//}
}
#pragma endregion
