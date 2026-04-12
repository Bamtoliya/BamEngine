#pragma once

#include "Material.h"
#include "MaterialInstance.h"
#include "SamplerManager.h"
#include "Renderer.h"
#include "ResourceManager.h"

#pragma region Constructor&Destructor
EResult MaterialInstance::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg))) return EResult::Fail;
	CAST_DESC
	m_BaseMaterialHandle = desc->BaseMaterialHandle;
	return EResult::Success;
}

MaterialInstance* MaterialInstance::Create(void* arg)
{
	MaterialInstance* instance = new MaterialInstance();
	if (IsFailure(instance->Initialize(arg)))
	{
		instance->Free();
		delete instance;
		return nullptr;
	}
	return instance;
}

MaterialInstance* MaterialInstance::Create(const ResourceHandle<Material>& baseMaterialHandle)
{
	DESC desc = {};
	desc.BaseMaterialHandle	= baseMaterialHandle;
	return Create(&desc);
}

MaterialInstance* MaterialInstance::Clone(void* arg)
{
	TODO("MaterialInstance Clone 구현 필요");
	return nullptr;
}

void MaterialInstance::Free()
{
	__super::Free();
}
#pragma endregion

EResult MaterialInstance::Bind(uint32 slot)
{
	if (!m_BaseMaterialHandle) return EResult::Fail;

	RHI* rhi = Renderer::Get().GetRHI();

	// 1단계: Base Material의 텍스처 슬롯 순회
	for (auto& [name, baseSlot] : m_BaseMaterialHandle.Get()->GetTextureSlots())
	{
		// 자신에게 오버라이드가 있으면 스킵 (2단계에서 처리)
		if (m_TextureSlots.contains(name))
			continue;

		Texture* texture = baseSlot.texture.Get();
		if (!texture)
			texture = ResourceManager::Get().GetResourceHandle<Texture>(L"Resources/Texture/magenta1x1.png").Get();
		RHISampler* sampler = baseSlot.sampler;
		if (!sampler)
			sampler = SamplerManager::Get().GetDefaultSampler();
		rhi->BindTextureSampler(texture->GetRHITexture(), sampler, baseSlot.slot);
	}

	// 2단계: 자신의 오버라이드 바인드
	for (auto& [name, textureSlot] : m_TextureSlots)
	{
		Texture* texture = textureSlot.texture.Get();
		if (!texture)
			texture = ResourceManager::Get().GetResourceHandle<Texture>(L"Resources/Texture/magenta1x1.png").Get();
		RHISampler* sampler = textureSlot.sampler;
		if (!sampler)
			sampler = SamplerManager::Get().GetDefaultSampler();
		rhi->BindTextureSampler(texture->GetRHITexture(), sampler, textureSlot.slot);
	}

	return EResult::Success;
}

void MaterialInstance::SetBaseMaterial(const ResourceHandle<Material>& material)
{
	m_BaseMaterialHandle = material;
}


#pragma region Shader
Shader* MaterialInstance::GetVertexShader() const
{
	if (m_VertexShaderHandle)
		return m_VertexShaderHandle.Get();
	return m_BaseMaterialHandle.Get()->GetVertexShader();
}

Shader* MaterialInstance::GetPixelShader() const
{
	if (m_PixelShaderHandle)
		return m_PixelShaderHandle.Get();
	return m_BaseMaterialHandle.Get()->GetPixelShader();
}

void MaterialInstance::SetVertexShaderHandle(const ResourceHandle<Shader>& shader)
{
	m_VertexShaderHandle = shader;
}

void MaterialInstance::SetPixelShaderHandle(const ResourceHandle<Shader>& shader)
{
	m_PixelShaderHandle = shader;
}

#pragma endregion


#pragma region Pipeline
EBlendMode MaterialInstance::GetBlendMode() const
{
	return m_BaseMaterialHandle ? m_BaseMaterialHandle.Get()->GetBlendMode() : EBlendMode::Opaque;
}
ECullMode  MaterialInstance::GetCullMode() const
{
	return m_BaseMaterialHandle ? m_BaseMaterialHandle.Get()->GetCullMode() : ECullMode::Back;
}
EFillMode  MaterialInstance::GetFillMode() const
{
	return m_BaseMaterialHandle ? m_BaseMaterialHandle.Get()->GetFillMode() : EFillMode::Solid;
}
EDepthMode MaterialInstance::GetDepthMode() const
{
	return m_BaseMaterialHandle ? m_BaseMaterialHandle.Get()->GetDepthMode() : EDepthMode::ReadWrite;
}
ECompareOp MaterialInstance::GetDepthCompareOp() const
{
	return m_BaseMaterialHandle ? m_BaseMaterialHandle.Get()->GetDepthCompareOp() : ECompareOp::Less;
}
#pragma endregion


void MaterialInstance::Deserialize(Archive& ar)
{
	Serialize(ar);
}

