#pragma once

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
	Material* baseMaterial = m_BaseMaterialHandle.Get();
	if (!rhi || !baseMaterial)
		return EResult::Fail;

	auto& rm = ResourceManager::Get();
	auto* defaultSampler = SamplerManager::Get().GetDefaultSampler();

	// 0단계: 아카이브 로드 후 내부 슬롯 캐시가 비어있으면 자동 복구
	if (!m_TextureBindings.empty() && FindTextureBindingIndexBySlot(m_TextureBindings[0].slot) < 0)
	{
		RebuildTextureBindingCache();
	}

	bool slotBound[4] = { false, false, false, false };

	// 1단계: Base Material의 텍스처 슬롯 순회
	for (auto& baseBinding : baseMaterial->GetTextureBindings())
	{
		bool hasOverride = HasTextureBindingBySlot(baseBinding.slot) || (!baseBinding.name.empty() && HasTextureBinding(baseBinding.name));
		if (hasOverride)
			continue;

		Texture* texture = baseBinding.texture.Get();
		if (!texture)
		{
			auto fallbackHandle = rm.GetResourceHandle<Texture>(L"Resources/Texture/white1x1.png");
			texture = fallbackHandle.Get();
		}
		RHISampler* sampler = baseBinding.hasCustomSampler
			? SamplerManager::Get().GetOrCreateSampler(baseBinding.samplerDesc)
			: defaultSampler;

		if (!texture || !sampler) return EResult::Fail;

		RHITexture* rhiTex = texture->GetRHITexture();
		if (rhiTex && IsFailure(rhi->BindTextureSampler(rhiTex, sampler, baseBinding.slot)))
			return EResult::Fail;

		if (baseBinding.slot < 4)
			slotBound[baseBinding.slot] = true;
	}

	// 2단계: 자신의 오버라이드 바인드
	for (auto& binding : m_TextureBindings)
	{
		Texture* texture = binding.texture.Get();
		if (!texture)
		{
			auto fallbackHandle = rm.GetResourceHandle<Texture>(L"Resources/Texture/white1x1.png");
			texture = fallbackHandle.Get();
		}
		RHISampler* sampler = binding.hasCustomSampler
			? SamplerManager::Get().GetOrCreateSampler(binding.samplerDesc)
			: defaultSampler;

		if (!texture || !sampler) return EResult::Fail;

		RHITexture* rhiTex = texture->GetRHITexture();
		if (rhiTex && IsFailure(rhi->BindTextureSampler(rhiTex, sampler, binding.slot)))
			return EResult::Fail;

		if (binding.slot < 4)
			slotBound[binding.slot] = true;
	}

	// 3단계: [상태 누수 방지] 지정되지 않은 빈 슬롯을 안전한 표준 더미 텍스처로 리셋
	static const wchar_t* defaultTextures[4] = {
		L"Resources/Texture/white1x1.png", // Slot 0: Diffuse (흰색)
		L"Resources/Texture/blue1x1.png",  // Slot 1: Normal (기본 평면 노멀)
		L"Resources/Texture/black1x1.png", // Slot 2: PBR (비금속/기본값)
		L"Resources/Texture/black1x1.png"  // Slot 3: Emission (완전한 0 무발광!)
	};

	for (uint32 s = 0; s < 4; ++s)
	{
		if (!slotBound[s])
		{
			auto dummyHandle = rm.GetResourceHandle<Texture>(defaultTextures[s]);
			if (auto* dummyTex = dummyHandle.Get())
			{
				if (auto* rhiDummy = dummyTex->GetRHITexture())
					rhi->BindTextureSampler(rhiDummy, defaultSampler, s);
			}
		}
	}

	return EResult::Success;
}

void MaterialInstance::SetBaseMaterial(const ResourceHandle<Material>& material)
{
	m_BaseMaterialHandle = material;
}

#pragma region Shader
Shader* MaterialInstance::GetVertexShader(ERenderPassType passType) const
{
	if (HasPassShader(passType) || (passType == ERenderPassType::Geometry && m_VertexShaderHandle))
		return MaterialInterface::GetVertexShader(passType);

	return m_BaseMaterialHandle ? m_BaseMaterialHandle.Get()->GetVertexShader(passType) : nullptr;
}

Shader* MaterialInstance::GetPixelShader(ERenderPassType passType) const
{
	if (HasPassShader(passType) || (passType == ERenderPassType::Geometry && m_PixelShaderHandle))
		return MaterialInterface::GetPixelShader(passType);

	return m_BaseMaterialHandle ? m_BaseMaterialHandle.Get()->GetPixelShader(passType) : nullptr;
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
	return HasFlag(m_OverrideFlags, EPipelineOverrideFlag::BlendMode) ? m_BlendMode : (m_BaseMaterialHandle ? m_BaseMaterialHandle.Get()->GetBlendMode() : EBlendMode::Opaque);
}
ECullMode  MaterialInstance::GetCullMode() const
{
	return HasFlag(m_OverrideFlags, EPipelineOverrideFlag::CullMode) ? m_CullMode : (m_BaseMaterialHandle ? m_BaseMaterialHandle.Get()->GetCullMode() : ECullMode::Back);
}
EFillMode  MaterialInstance::GetFillMode() const
{
	return HasFlag(m_OverrideFlags, EPipelineOverrideFlag::FillMode) ? m_FillMode : (m_BaseMaterialHandle ? m_BaseMaterialHandle.Get()->GetFillMode() : EFillMode::Solid);
}
EDepthMode MaterialInstance::GetDepthMode() const
{
	return HasFlag(m_OverrideFlags, EPipelineOverrideFlag::DepthMode) ? m_DepthMode : (m_BaseMaterialHandle ? m_BaseMaterialHandle.Get()->GetDepthMode() : EDepthMode::ReadWrite);
}
ECompareOp MaterialInstance::GetDepthCompareOp() const
{
	return HasFlag(m_OverrideFlags, EPipelineOverrideFlag::DepthCompareOp) ? m_DepthCompareOp : (m_BaseMaterialHandle ? m_BaseMaterialHandle.Get()->GetDepthCompareOp() : ECompareOp::Less);
}

void MaterialInstance::SetBlendMode(EBlendMode mode)
{
	m_BlendMode = mode;
	AddFlag(m_OverrideFlags, EPipelineOverrideFlag::BlendMode);
}

void MaterialInstance::SetCullMode(ECullMode mode)
{
	m_CullMode = mode;
	AddFlag(m_OverrideFlags, EPipelineOverrideFlag::CullMode);
}

void MaterialInstance::SetFillMode(EFillMode mode)
{
	m_FillMode = mode;
	AddFlag(m_OverrideFlags, EPipelineOverrideFlag::FillMode);
}

void MaterialInstance::SetDepthMode(EDepthMode mode)
{
	m_DepthMode = mode;
	AddFlag(m_OverrideFlags, EPipelineOverrideFlag::DepthMode);
}

void MaterialInstance::SetDepthCompareOp(ECompareOp op)
{
	m_DepthCompareOp = op;
	AddFlag(m_OverrideFlags, EPipelineOverrideFlag::DepthCompareOp);
}

#pragma endregion