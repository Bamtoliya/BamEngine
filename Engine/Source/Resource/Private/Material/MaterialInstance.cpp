#pragma once

#include "Material.h"
#include "MaterialInstance.h"
#include "SamplerManager.h"

#pragma region Constructor&Destructor
EResult MaterialInstance::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	CAST_DESC
	m_BaseMaterial = desc->BaseMaterial;
	Safe_AddRef(m_BaseMaterial);
	return EResult::Success;
}

MaterialInstance* MaterialInstance::Create(void* arg)
{
	MaterialInstance* instance = new MaterialInstance();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

MaterialInstance* MaterialInstance::Create(Material* baseMaterial)
{
	DESC desc = {};
	desc.BaseMaterial = baseMaterial;
	return Create(&desc);
}

MaterialInstance* MaterialInstance::Clone(void* arg)
{
	return nullptr;
}

void MaterialInstance::Free()
{
	Safe_Release(m_BaseMaterial);
	Safe_Release(m_VertexShader);
	Safe_Release(m_PixelShader);
	__super::Free();
}
#pragma endregion

EResult MaterialInstance::Bind(uint32 slot)
{
	if (!m_BaseMaterial) return EResult::Fail;

	RHI* rhi = Renderer::Get().GetRHI();
	RenderPass* renderPass = rhi->GetCurrentRenderPass();

	// 1단계: Base Material의 텍스처 슬롯 순회
	for (auto& [name, baseSlot] : m_BaseMaterial->GetTextureSlots())
	{
		// 자신에게 오버라이드가 있으면 스킵 (2단계에서 처리)
		if (m_TextureSlots.contains(name))
			continue;

		RHITexture* texture = baseSlot.texture;
		if (!texture)
			texture = ResourceManager::Get().GetTexture(L"DefaultTexture")->GetRHITexture();
		RHISampler* sampler = SamplerManager::Get().GetSampler(baseSlot.SamplerKey);
		if (!sampler)
			sampler = SamplerManager::Get().GetDefaultSampler();
		rhi->BindTextureSampler(texture, sampler, baseSlot.slot);
	}

	// 2단계: 자신의 오버라이드 바인드
	for (auto& [name, textureSlot] : m_TextureSlots)
	{
		RHITexture* texture = textureSlot.texture;
		if (!texture)
			texture = ResourceManager::Get().GetTexture(L"DefaultTexture")->GetRHITexture();
		RHISampler* sampler = SamplerManager::Get().GetSampler(textureSlot.SamplerKey);
		if (!sampler)
			sampler = SamplerManager::Get().GetDefaultSampler();
		rhi->BindTextureSampler(texture, sampler, textureSlot.slot);
	}

	return EResult::Success;
}

void MaterialInstance::SetBaseMaterial(Material* material)
{
	if (m_BaseMaterial)
		Safe_Release(m_BaseMaterial);
	m_BaseMaterial = material;
	if (material)
		Safe_AddRef(m_BaseMaterial);
}


#pragma region Shader
Shader* MaterialInstance::GetVertexShader() const
{
	if (m_VertexShader) return m_VertexShader;
	return m_BaseMaterial->GetVertexShader();
}

Shader* MaterialInstance::GetPixelShader() const
{
	if (m_PixelShader) return m_PixelShader;
	return m_BaseMaterial->GetPixelShader();
}


void MaterialInstance::SetVertexShader(Shader* shader)
{
	if (m_VertexShader)
		Safe_Release(m_VertexShader);
	m_VertexShader = shader;
	Safe_AddRef(m_VertexShader);
}

void MaterialInstance::SetPixelShader(Shader* shader)
{
	if (m_PixelShader)
		Safe_Release(m_PixelShader);
	m_PixelShader = shader;
	Safe_AddRef(m_PixelShader);
}
#pragma endregion
