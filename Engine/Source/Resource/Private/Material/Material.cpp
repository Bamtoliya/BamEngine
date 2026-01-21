#pragma once
#include "Material.h"

#pragma region Cosntructor&Destructor
EResult Material::Initialize(void* arg)
{
	//wstring* shaderPath = reinterpret_cast<wstring*>(arg);
	//if (!shaderPath) return EResult::Fail;
	//m_VertexShader = RHIShader::Create(RHIShader::EShaderType::Vertex, *shaderPath + L".vs");
	//if (!m_VertexShader) return EResult::Fail;
	//m_PixelShader = RHIShader::Create(RHIShader::EShaderType::Pixel, *shaderPath + L".ps");
	//if (!m_PixelShader) return EResult::Fail;
	return EResult::Success;
}
Material* Material::Create(void* arg)
{
	Material* instance = new Material();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}
Material* Material::Clone(void* arg)
{
	Material* instance = new Material(*this);
	if (!instance) return nullptr;
	instance->m_VertexShader = m_VertexShader;
	instance->m_PixelShader = m_PixelShader;
	return instance;
}
void Material::Free()
{
	Safe_Release(m_VertexShader);
	Safe_Release(m_PixelShader);
}
#pragma endregion
