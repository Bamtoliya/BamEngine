#pragma once
#include "Shader.h"
#include "Renderer.h"


#pragma region Constructor&Destructor
EResult Shader::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	// Create RHI Shader
	{
		RHI* rhi = Renderer::Get().GetRHI();
		tagShaderInfo* shaderInfo = reinterpret_cast<tagShaderInfo*>(arg);
		// For simplicity, assuming RHI has a method CreateShader (not shown in previous snippets)
		// m_RHIShader = rhi->CreateShader(shaderInfo);
		// if (!m_RHIShader)
		//     return EResult::Fail;
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
	Base::Free();
	Safe_Release(m_RHIShader);
}
#pragma endregion
