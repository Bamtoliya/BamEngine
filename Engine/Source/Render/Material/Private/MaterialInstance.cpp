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
	MaterialInstance* instance = new MaterialInstance(*this);
	instance->m_Version = 0;
	if (arg)
	{
		auto* desc = static_cast<DESC*>(arg);
		if (!desc->Key.empty() || !desc->Path.empty())
		{
			instance->m_Key = NormalizePath(desc->Key.empty() ? desc->Path : desc->Key);
			instance->m_Path = NormalizePath(desc->Path);
		}
		if (desc->BaseMaterialHandle.IsValid())
			instance->m_BaseMaterialHandle = desc->BaseMaterialHandle;
	}
	instance->RebuildTextureBindingCache();
	return instance;
}

void MaterialInstance::Free()
{
	m_CachedBaseMaterialPtr = nullptr;
	m_CachedBaseConstantVersion = 0;
	__super::Free();
}
#pragma endregion

void MaterialInstance::BuildMaterialConstants(MaterialConstants& outConstants) const
{
	// 1. BaseMaterial의 기본 상수 버퍼 데이터 상속
	const auto* base = GetBaseMaterial();
	if (base)
	{
		base->BuildMaterialConstants(outConstants);
	}
	else
	{
		outConstants = MaterialConstants{};
	}
	const auto flagForSlot = [](uint32 slot)
	{
		switch (slot)
		{
		case 0: return static_cast<uint32>(EMaterialFeatureFlags::HasDiffuse);
		case 1: return static_cast<uint32>(EMaterialFeatureFlags::HasNormal);
		case 2: return static_cast<uint32>(EMaterialFeatureFlags::HasPBR);
		case 3: return static_cast<uint32>(EMaterialFeatureFlags::HasEmissive);
		default: return 0u;
		}
	};
	if (base)
	{
		for (const auto& binding : base->GetTextureBindings())
			if (HasTextureBindingBySlot(binding.slot) || (!binding.name.empty() && HasTextureBinding(binding.name)))
				outConstants.materialFlags &= ~flagForSlot(binding.slot);
	}
	for (const auto& binding : m_TextureBindings)
		if (binding.texture.Get()) outConstants.materialFlags |= flagForSlot(binding.slot);
	// 3. 인스턴스 자신의 파라미터 오버라이드 반영
	ApplyParameterConstants(outConstants);
	// 블렌드 모드와 알파 기준값은 인스턴스 오버라이드를 반영한 값으로 다시 담는다.
	outConstants.materialFlags = PackMaterialAlphaCutoff(outConstants.materialFlags,
		GetBlendMode() == EBlendMode::Masked, GetAlphaCutoff());
}

f32 MaterialInstance::GetAlphaCutoff() const
{
	if (const auto cutoff = FindAlphaCutoffParameter()) return *cutoff;
	const auto* base = GetBaseMaterial();
	return base ? base->GetAlphaCutoff() : DEFAULT_ALPHA_CUTOFF;
}

void MaterialInstance::BuildConstantBufferBlob(vector<uint8>& outBlob) const
{
	Material* baseMat = GetBaseMaterial();
	const MaterialBufferLayout& effectiveLayout = baseMat && !HasShaderGraph() && !GetBufferLayout().HasCustomParameters()
		? baseMat->GetBufferLayout() : GetBufferLayout();

	const size_t totalSize = std::clamp<size_t>(effectiveLayout.TotalBufferSize,
		sizeof(MaterialConstants), MaterialBufferLayout::MaxBufferSize);
	const size_t alignedSize = (totalSize + 15u) & ~static_cast<size_t>(15u);
	outBlob.assign(alignedSize, 0u);

	MaterialConstants baseConstants;
	BuildMaterialConstants(baseConstants);
	memcpy(outBlob.data(), &baseConstants, sizeof(MaterialConstants));

	if (baseMat) baseMat->ApplyCustomParametersToBlob(outBlob, effectiveLayout);
	ApplyCustomParametersToBlob(outBlob, effectiveLayout);
}

const vector<uint8>& MaterialInstance::GetOrBuildConstantBufferBlob(uint32 liveTextureFlagMask) const
{
	const Material* baseMat = GetBaseMaterial();
	const uint64 baseVersion = baseMat ? baseMat->GetConstantVersion() : 0;

	if (m_CachedConstantBlob.empty() || m_CachedConstantVersion != m_ConstantVersion ||
		m_CachedTextureFlagMask != liveTextureFlagMask ||
		m_CachedBaseMaterialPtr != baseMat || m_CachedBaseConstantVersion != baseVersion)
	{
		BuildConstantBufferBlob(m_CachedConstantBlob);
		m_CachedConstantVersion = m_ConstantVersion;
		m_CachedTextureFlagMask = liveTextureFlagMask;
		m_CachedBaseMaterialPtr = baseMat;
		m_CachedBaseConstantVersion = baseVersion;
	}
	return m_CachedConstantBlob;
}

EResult MaterialInstance::Bind(uint32 slot)
{
	if (!m_BaseMaterialHandle) return EResult::Fail;
	RHI* rhi = Renderer::Get().GetRHI();
	Material* baseMaterial = m_BaseMaterialHandle.Get();
	if (!rhi || !baseMaterial) return EResult::Fail;

	auto* defaultSampler = SamplerManager::Get().GetDefaultSampler();
	if (!defaultSampler) return EResult::Fail;

	// 0단계: 아카이브 로드 후 내부 슬롯 캐시가 비어있으면 자동 복구
	if (!m_TextureBindings.empty() && FindTextureBindingIndexBySlot(m_TextureBindings[0].slot) < 0)
		RebuildTextureBindingCache();

	const auto flagForSlot = [](uint32 texSlot) -> uint32
	{
		switch (texSlot)
		{
		case 0: return static_cast<uint32>(EMaterialFeatureFlags::HasDiffuse);
		case 1: return static_cast<uint32>(EMaterialFeatureFlags::HasNormal);
		case 2: return static_cast<uint32>(EMaterialFeatureFlags::HasPBR);
		case 3: return static_cast<uint32>(EMaterialFeatureFlags::HasEmissive);
		default: return 0u;
		}
	};

	bool slotBound[4] = { false, false, false, false };
	uint32 liveTextureFlagMask = 0u;

	// 1단계: Base Material의 텍스처 슬롯 순회 (인스턴스 오버라이드가 없는 경우만)
	for (auto& baseBinding : baseMaterial->GetTextureBindings())
	{
		bool hasOverride = HasTextureBindingBySlot(baseBinding.slot) || (!baseBinding.name.empty() && HasTextureBinding(baseBinding.name));
		if (hasOverride) continue;

		Texture* texture = baseBinding.texture.Get();
		if (!texture) continue;

		liveTextureFlagMask |= flagForSlot(baseBinding.slot);

		RHISampler* sampler = baseBinding.hasCustomSampler
			? SamplerManager::Get().GetOrCreateSampler(baseBinding.samplerDesc)
			: defaultSampler;
		if (!sampler) return EResult::Fail;

		RHITexture* rhiTex = texture->GetAvailableRHITexture();
		if (!rhiTex) continue;

		if (IsFailure(rhi->BindTextureSampler(rhiTex, sampler, baseBinding.slot)))
			return EResult::Fail;

		if (baseBinding.slot < 4)
			slotBound[baseBinding.slot] = true;
	}

	// 2단계: 자신의 오버라이드 텍스처 바인드
	for (auto& binding : m_TextureBindings)
	{
		Texture* texture = binding.texture.Get();
		if (!texture) continue;

		liveTextureFlagMask |= flagForSlot(binding.slot);

		RHISampler* sampler = binding.hasCustomSampler
			? SamplerManager::Get().GetOrCreateSampler(binding.samplerDesc)
			: defaultSampler;
		if (!sampler) return EResult::Fail;

		RHITexture* rhiTex = texture->GetAvailableRHITexture();
		if (!rhiTex) continue;

		if (IsFailure(rhi->BindTextureSampler(rhiTex, sampler, binding.slot)))
			return EResult::Fail;

		if (binding.slot < 4)
			slotBound[binding.slot] = true;
	}

	// 3단계: [상태 누수 방지] 지정되지 않은 빈 슬롯을 nullptr로 해제
	for (uint32 s = 0; s < 4; ++s)
	{
		if (!slotBound[s])
			(void)rhi->BindTexture(nullptr, s);
	}

	// 4단계: 캐시된 머티리얼 상수 버퍼 바인딩 (무할당 재사용)
	const vector<uint8>& constantBlob = GetOrBuildConstantBufferBlob(liveTextureFlagMask);
	return rhi->BindConstantBuffer(constantBlob.data(), static_cast<uint32>(constantBlob.size()), slot);
}

void MaterialInstance::SetBaseMaterial(const ResourceHandle<Material>& material)
{
	m_BaseMaterialHandle = material;
	MarkConstantBlobDirty();
}

#pragma region Shader
Shader* MaterialInstance::GetVertexShader(ERenderPassType passType) const
{
	const ERenderPassType primaryPass = GetPrimaryPassType();
	if (HasPassShader(passType) || ((passType == primaryPass || primaryPass == ERenderPassType::Custom) && m_VertexShaderHandle))
		return MaterialInterface::GetVertexShader(passType);
	Material* baseMat = GetBaseMaterial();
	return baseMat ? baseMat->GetVertexShader(passType) : nullptr;
}
Shader* MaterialInstance::GetPixelShader(ERenderPassType passType) const
{
	const ERenderPassType primaryPass = GetPrimaryPassType();
	if (HasPassShader(passType) || ((passType == primaryPass || primaryPass == ERenderPassType::Custom) && m_PixelShaderHandle))
		return MaterialInterface::GetPixelShader(passType);
	Material* baseMat = GetBaseMaterial();
	return baseMat ? baseMat->GetPixelShader(passType) : nullptr;
}
void MaterialInstance::SetVertexShaderHandle(const ResourceHandle<Shader>& shader)
{
	m_VertexShaderHandle = shader;
}
void MaterialInstance::SetPixelShaderHandle(const ResourceHandle<Shader>& shader)
{
	m_PixelShaderHandle = shader;
}
string MaterialInstance::GetShaderGraphPath() const
{
	if (!m_ShaderGraphPath.empty())
		return m_ShaderGraphPath;
	if (auto* baseMat = GetBaseMaterial())
		return baseMat->GetShaderGraphPath();
	return "";
}
#pragma endregion
#pragma region Pipeline
ERenderPassType MaterialInstance::GetPrimaryPassType() const
{
	if (HasFlag(m_OverrideFlags, EPipelineOverrideFlag::PrimaryPassType)) return m_PrimaryPassType;
	Material* baseMat = GetBaseMaterial();
	return baseMat ? baseMat->GetPrimaryPassType() : ERenderPassType::Geometry;
}
EBlendMode MaterialInstance::GetBlendMode() const
{
	if (HasFlag(m_OverrideFlags, EPipelineOverrideFlag::BlendMode)) return m_BlendMode;
	Material* baseMat = GetBaseMaterial();
	return baseMat ? baseMat->GetBlendMode() : EBlendMode::Opaque;
}
ECullMode MaterialInstance::GetCullMode() const
{
	if (HasFlag(m_OverrideFlags, EPipelineOverrideFlag::CullMode)) return m_CullMode;
	Material* baseMat = GetBaseMaterial();
	return baseMat ? baseMat->GetCullMode() : ECullMode::Back;
}
EFillMode MaterialInstance::GetFillMode() const
{
	if (HasFlag(m_OverrideFlags, EPipelineOverrideFlag::FillMode)) return m_FillMode;
	Material* baseMat = GetBaseMaterial();
	return baseMat ? baseMat->GetFillMode() : EFillMode::Solid;
}
EDepthMode MaterialInstance::GetDepthMode() const
{
	if (HasFlag(m_OverrideFlags, EPipelineOverrideFlag::DepthMode)) return m_DepthMode;
	Material* baseMat = GetBaseMaterial();
	return baseMat ? baseMat->GetDepthMode() : EDepthMode::ReadWrite;
}
ECompareOp MaterialInstance::GetDepthCompareOp() const
{
	if (HasFlag(m_OverrideFlags, EPipelineOverrideFlag::DepthCompareOp)) return m_DepthCompareOp;
	Material* baseMat = GetBaseMaterial();
	return baseMat ? baseMat->GetDepthCompareOp() : ECompareOp::Less;
}

EFrontFace MaterialInstance::GetFrontFace() const
{
	if (HasFlag(m_OverrideFlags, EPipelineOverrideFlag::FrontFace)) return m_FrontFace;
	Material* baseMat = GetBaseMaterial();
	return baseMat ? baseMat->GetFrontFace() : EFrontFace::Clockwise;
}

void MaterialInstance::SetPrimaryPassType(ERenderPassType passType)
{
	m_PrimaryPassType = passType;
	AddFlag(m_OverrideFlags, EPipelineOverrideFlag::PrimaryPassType);
}

void MaterialInstance::SetBlendMode(EBlendMode mode)
{
	m_BlendMode = mode;
	AddFlag(m_OverrideFlags, EPipelineOverrideFlag::BlendMode);
	MarkConstantBlobDirty();
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

void MaterialInstance::SetFrontFace(EFrontFace mode)
{
	m_FrontFace = mode;
	AddFlag(m_OverrideFlags, EPipelineOverrideFlag::FrontFace);
}

#pragma endregion
