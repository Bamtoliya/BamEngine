#pragma once
#include "LightSource.h"
#include "GameObject.h"
#include "Transform.h"
#include "LightManager.h"
#include "ComponentRegistry.h"

REGISTER_COMPONENT(LightSource);
#pragma region Constructor&Destructor
EResult LightSource::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg)))
		return EResult::Fail;

	if (arg)
	{
		CAST_DESC

			m_Type = desc->Type;
		m_Color = glm::max(desc->Color, vec3(0.0f));
		m_Intensity = glm::max(0.0f, desc->Intensity);

		m_Range = glm::max(0.0f, desc->Range);
		m_AttenuationMode = desc->AttenuationMode;
		m_AttenuationCoefficients = glm::max(desc->AttenuationCoefficients, vec3(0.0f));

		m_SpotInnerAngle = glm::clamp(desc->SpotInnerAngle, 0.0f, 89.0f);
		m_SpotOuterAngle = glm::clamp(desc->SpotOuterAngle, m_SpotInnerAngle, 89.0f);
		m_SpotFalloffExponent = glm::max(0.01f, desc->SpotFalloffExponent);

		m_LightingLayerMask = desc->LightingLayerMask;
		m_Flags = desc->Flags;
	}

	return EResult::Success;
}

EResult LightSource::LateInitialize(void* arg)
{
	return EResult::Success;
}

LightSource* LightSource::Create(void* arg)
{
	LightSource* instance = new LightSource();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

Component* LightSource::Clone(GameObject* owner, void* arg)
{
	LightSource* instance = new LightSource();
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

		if (IsFailure(instance->Initialize(&desc)))
		{
			Safe_Release(instance);
			return nullptr;
		}
	}

	return instance;
}

void LightSource::Free()
{
	LightManager::Get().RemoveLightSource(this);
	__super::Free();
}

#pragma endregion

#pragma region Loop
void LightSource::LateUpdate(f32 dt)
{
	__super::LateUpdate(dt);
	if (m_Active && m_Owner && m_Owner->IsActive())
		LightManager::Get().AddLightSource(this);
}
#pragma endregion

#pragma region GPU
tagGPULight LightSource::BuildGPULightDesc() const
{
	tagGPULight out = {};

	if (!m_Active || !m_Owner || !m_Owner->IsActive())
		return out;

	Transform* transform = m_Owner->GetTransform();

	out.Position = transform ? transform->GetWorldPosition() : vec3(0.0f);

	vec3 forward = transform ? transform->GetForward() : vec3(0.0f, 0.0f, 1.0f);
	out.Direction = glm::dot(forward, forward) > 0.000001f ? glm::normalize(forward) : vec3(0.0f, 0.0f, 1.0f);

	out.Intensity = glm::max(0.0f, m_Intensity);
	out.Range = glm::max(0.0f, m_Range);
	out.Color = glm::max(m_Color, vec3(0.0f));

	out.AttenuationCoeff = glm::max(m_AttenuationCoefficients, vec3(0.0f));

	f32 innerAngle = glm::clamp(m_SpotInnerAngle, 0.0f, 89.0f);
	f32 outerAngle = glm::clamp(m_SpotOuterAngle, innerAngle, 89.0f);
	out.SpotInnerCos = glm::cos(glm::radians(innerAngle));
	out.SpotOuterCos = glm::cos(glm::radians(outerAngle));
	out.SpotFalloff = glm::max(0.01f, m_SpotFalloffExponent);

	out.PackedFlags = PackLightFlags(
		static_cast<uint32>(m_Flags),
		static_cast<uint32>(m_Type),
		static_cast<uint32>(m_AttenuationMode)
	);
	return out;
}
#pragma endregion

#pragma region Shadow
tagCameraBuffer LightSource::BuildShadowCameraBuffer() const
{
	tagCameraBuffer buffer = {};
	if (!m_Active || !m_Owner || !m_Owner->IsActive())
		return buffer;
	Transform* transform = m_Owner->GetTransform();
	vec3 position = transform ? transform->GetWorldPosition() : vec3(0.0f);
	vec3 forward = transform ? transform->GetForward() : vec3(0.0f, 0.0f, 1.0f);
	mat4 view = glm::lookAt(position, position + forward, vec3(0.0f, 1.0f, 0.0f));
	// 단일 방향성 라이트의 경우, 범위를 크게 잡아서 전체 씬을 커버하도록 할 수 있습니다.
	// 실제로는 씬의 크기에 따라 적절히 조정하는 것이 좋습니다.
	f32 shadowRange = m_ShadowRange;
	mat4 proj = glm::ortho(-shadowRange, shadowRange, -shadowRange, shadowRange, -shadowRange, shadowRange * 2.0f);
	buffer.viewMatrix = view;
	buffer.projMatrix = proj;
	buffer.viewProjMatrix = proj * view;
	buffer.cameraPosition = position;
	return buffer;
}
tagLightShadowData LightSource::BuildShadowData() const
{
	tagLightShadowData data = {};
	if (!m_Active || !m_Owner || !m_Owner->IsActive()) return data;
	data.LightViewProjMatrix = BuildShadowCameraBuffer().viewProjMatrix;
	data.ShadowParams = vec4(m_ShadowBias, m_ShadowSlopeBias, m_ShadowNormalBias, 0.0f);
	return data;
}
#pragma endregion
