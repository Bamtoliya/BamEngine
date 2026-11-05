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

void MaterialInstance::BuildMaterialConstants(MaterialConstants& outConstants) const
{
	// 1. BaseMaterial의 기본 상수 버퍼 데이터 상속
	if (m_BaseMaterialHandle && m_BaseMaterialHandle.Get())
	{
		m_BaseMaterialHandle.Get()->BuildMaterialConstants(outConstants);
	}
	else
	{
		outConstants = MaterialConstants{};
	}
	// 2. 인스턴스 자신의 텍스처 오버라이드 반영 (오버라이드가 등록된 슬롯만 플래그 갱신)
	for (const auto& binding : m_TextureBindings)
	{
		const uint32 flagBit = (binding.slot == 0) ? static_cast<uint32>(EMaterialFeatureFlags::HasDiffuse)
			: (binding.slot == 1) ? static_cast<uint32>(EMaterialFeatureFlags::HasNormal)
			: (binding.slot == 2) ? static_cast<uint32>(EMaterialFeatureFlags::HasPBR)
			: (binding.slot == 3) ? static_cast<uint32>(EMaterialFeatureFlags::HasEmissive) : 0u;
		if (flagBit == 0) continue;
		if (binding.texture && binding.texture.Get())
		{
			outConstants.materialFlags |= flagBit;
		}
	}
	// 3. 인스턴스 자신의 파라미터 오버라이드 반영
	auto findParam = [this](const string& key) -> const MaterialParameter* {
		auto it = m_Parameters.find(key);
		return (it != m_Parameters.end()) ? &it->second : nullptr;
		};
	if (const auto* p = findParam("BaseColor"); p && p->data.size() >= sizeof(vec4))
		outConstants.baseColor = *reinterpret_cast<const vec4*>(p->data.data());
	else if (const auto* p2 = findParam("Color"); p2 && p2->data.size() >= sizeof(vec4))
		outConstants.baseColor = *reinterpret_cast<const vec4*>(p2->data.data());
	if (const auto* p = findParam("Roughness"); p && p->data.size() >= sizeof(float))
		outConstants.roughness = *reinterpret_cast<const float*>(p->data.data());
	if (const auto* p = findParam("Metallic"); p && p->data.size() >= sizeof(float))
		outConstants.metallic = *reinterpret_cast<const float*>(p->data.data());
	if (const auto* p = findParam("Cavity"); p && p->data.size() >= sizeof(float))
		outConstants.cavity = *reinterpret_cast<const float*>(p->data.data());
	if (const auto* p = findParam("AO"); p && p->data.size() >= sizeof(float))
		outConstants.ao = *reinterpret_cast<const float*>(p->data.data());
	if (const auto* p = findParam("EmissiveColor"); p)
	{
		if (p->data.size() >= sizeof(vec4))
			outConstants.emissiveColor = vec3(*reinterpret_cast<const vec4*>(p->data.data()));
		else if (p->data.size() >= sizeof(vec3))
			outConstants.emissiveColor = *reinterpret_cast<const vec3*>(p->data.data());
	}
	else if (const auto* p2 = findParam("Emission"); p2)
	{
		if (p2->data.size() >= sizeof(vec4))
			outConstants.emissiveColor = vec3(*reinterpret_cast<const vec4*>(p2->data.data()));
		else if (p2->data.size() >= sizeof(vec3))
			outConstants.emissiveColor = *reinterpret_cast<const vec3*>(p2->data.data());
	}
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

	bool slotBound[4] = { false, false, false, false };

	// 1단계: Base Material의 텍스처 슬롯 순회 (인스턴스 오버라이드가 없는 경우만)
	for (auto& baseBinding : baseMaterial->GetTextureBindings())
	{
		bool hasOverride = HasTextureBindingBySlot(baseBinding.slot) || (!baseBinding.name.empty() && HasTextureBinding(baseBinding.name));
		if (hasOverride) continue;

		Texture* texture = baseBinding.texture.Get();
		if (!texture) continue;

		RHISampler* sampler = baseBinding.hasCustomSampler
			? SamplerManager::Get().GetOrCreateSampler(baseBinding.samplerDesc)
			: defaultSampler;
		if (!sampler) return EResult::Fail;

		RHITexture* rhiTex = texture->GetRHITexture();
		if (rhiTex && IsFailure(rhi->BindTextureSampler(rhiTex, sampler, baseBinding.slot)))
			return EResult::Fail;

		if (baseBinding.slot < 4)
			slotBound[baseBinding.slot] = true;
	}

	// 2단계: 자신의 오버라이드 텍스처 바인드
	for (auto& binding : m_TextureBindings)
	{
		Texture* texture = binding.texture.Get();
		if (!texture) continue;

		RHISampler* sampler = binding.hasCustomSampler
			? SamplerManager::Get().GetOrCreateSampler(binding.samplerDesc)
			: defaultSampler;
		if (!sampler) return EResult::Fail;

		RHITexture* rhiTex = texture->GetRHITexture();
		if (rhiTex && IsFailure(rhi->BindTextureSampler(rhiTex, sampler, binding.slot)))
			return EResult::Fail;

		if (binding.slot < 4)
			slotBound[binding.slot] = true;
	}

	// 3단계: [상태 누수 방지] 지정되지 않은 빈 슬롯을 nullptr로 해제
	for (uint32 s = 0; s < 4; ++s)
	{
		if (!slotBound[s])
			rhi->BindTexture(nullptr, s);
	}

	// 4단계: 머티리얼 상수 버퍼 바인딩
	MaterialConstants constants;
	BuildMaterialConstants(constants);
	return rhi->BindConstantBuffer(&constants, sizeof(MaterialConstants), slot);
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
	Material* baseMat = GetBaseMaterial();
	return baseMat ? baseMat->GetVertexShader(passType) : nullptr;
}
Shader* MaterialInstance::GetPixelShader(ERenderPassType passType) const
{
	if (HasPassShader(passType) || (passType == ERenderPassType::Geometry && m_PixelShaderHandle))
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
#pragma endregion
#pragma region Pipeline
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