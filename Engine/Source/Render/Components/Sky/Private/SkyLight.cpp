#pragma once

#include "SkyLight.h"
#include "Transform.h"
#include "SkyRenderer.h"
#include "ComponentRegistry.h"

REGISTER_COMPONENT(SkyLight)
#pragma region Construcotr&Desturctor
EResult SkyLight::Initialize(void* arg)
{
    if (IsFailure(__super::Initialize(arg))) return EResult::Fail;
    if (arg)
    {
        CAST_DESC
        m_GroundColor = desc->GroundColor;
        m_IndirectIntensity = desc->IndirectIntensity;
    }
    return EResult::Success;
}

SkyLight* SkyLight::Create(void* arg)
{
	SkyLight* instance = new SkyLight();
    if (IsFailure(instance->Initialize(arg)))
    {
		Safe_Release(instance);
		return nullptr;
    }
    return instance;
}

Component* SkyLight::Clone(GameObject* owner, void* arg)
{
    SkyLight* instance = new SkyLight();
    if (arg)
    {
        if (IsFailure(instance->Initialize(arg)))
        {
            Safe_Release(instance);
            return nullptr;
        }
    }
    else
    {
		DESC desc = {};
        desc.Owner = owner;
        desc.Active = m_Active;
        desc.Tag = m_Tag;

        desc.Type = m_Type;
        desc.Color = m_Color;
        desc.Intensity = m_Intensity;

        desc.Range = m_Range;
        desc.AttenuationMode = m_AttenuationMode;
        desc.AttenuationCoefficients = m_AttenuationCoefficients;

        desc.SpotInnerAngle = m_SpotInnerAngle;
        desc.SpotOuterAngle = m_SpotOuterAngle;
        desc.SpotFalloffExponent = m_SpotFalloffExponent;

        desc.LightingLayerMask = m_LightingLayerMask;
        desc.Flags = m_Flags;

        desc.GroundColor = m_GroundColor;
        desc.IndirectIntensity = m_IndirectIntensity;
        if (IsFailure(instance->Initialize(&desc)))
        {
            Safe_Release(instance);
            return nullptr;
        }
    }
    
    return instance;
}

void SkyLight::Free()
{
	__super::Free();
}
#pragma endregion



void SkyLight::LateUpdate(f32 dt)
{
    if (auto* sky = m_Owner->GetComponent<SkyRenderer>())
    {
        // Transform.Forward → SkyRenderer.SunDirection 동기화
        Transform* t = m_Owner->GetTransform();
        if (t)
            sky->SetSunDirection(t->GetForward());

        // ZenithColor → LightSource.Color 동기화 (역방향)
        m_Color = sky->GetZenithColor();
		m_Direction = sky->GetSunDirection();
    }

    __super::LateUpdate(dt);  // LightManager 제출
}

tagGPULight SkyLight::BuildGPULightDesc() const
{
    tagGPULight gpu = {};

    // Color = ZenithColor (SkyRenderer에서 동기화된 값)
    gpu.Color = m_Color;
    gpu.Intensity = m_Intensity;

    // AttenuationCoeff 슬롯 → GroundColor 재활용
    gpu.AttenuationCoeff = m_GroundColor;

    // Direction → SunDirection (SkyRenderer에서 동기화)
    gpu.Direction = glm::normalize(m_Direction);

    // IndirectIntensity → Range 슬롯 재활용
    gpu.Range = m_IndirectIntensity;

    gpu.PackedFlags = PackLightFlags(
        (uint32)ELightFlags::UseInDeferredRendering
        | (uint32)ELightFlags::AffectDiffuse,
        (uint32)ELightType::Sky,
        (uint32)EAttenuationMode::Disabled);

    return gpu;
}