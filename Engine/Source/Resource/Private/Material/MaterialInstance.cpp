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
	__super::Free();
}
#pragma endregion


EResult MaterialInstance::Bind(uint32 slot)
{
	if (!m_BaseMaterial) return EResult::Fail;

	RHI* rhi = Renderer::Get().GetRHI();

	// 파이프라인 바인드
	RHIPipeline* pipeline = PipelineManager::Get().GetPipeline(GetPipelineKey());
	if (!pipeline)
		pipeline = PipelineManager::Get().GetDefaultPipeline();
	if (IsFailure(rhi->BindPipeline(pipeline)))
		return EResult::Fail;

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

const wstring& MaterialInstance::GetPipelineKey() const
{
	if (!m_PipelineKey.empty()) return m_PipelineKey;
	return m_BaseMaterial->GetPipelineKey();
}

void MaterialInstance::SetBaseMaterial(Material* material)
{
	if (m_BaseMaterial)
		Safe_Release(m_BaseMaterial);
	m_BaseMaterial = material;
	if (material)
		Safe_AddRef(m_BaseMaterial);
}
