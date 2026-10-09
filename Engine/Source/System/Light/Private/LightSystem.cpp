#include "LightSystem.h"
#include "Components/Render/LightComponent.h"
#include "Components/WorldTransformComponent.h"
#include "Components/FlagComponent.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <cmath>

namespace
{
	bool IsFiniteLightVector(const glm::vec3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	bool IsFiniteLightMatrix(const glm::mat4& matrix)
	{
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
			{
				if (!std::isfinite(matrix[column][row]))
					return false;
			}
		}

		return true;
	}

	bool IsValidLightSettings(const Engine::LightComponent& light)
	{
		if (static_cast<Engine::uint32>(light.LightType) > static_cast<Engine::uint32>(Engine::ELightType::Sky))
			return false;

		if (!IsFiniteLightVector(light.Color) || !std::isfinite(light.Intensity))
			return false;

		if (light.LightType == Engine::ELightType::Sky)
			return IsFiniteLightVector(light.GroundColor) && std::isfinite(light.IndirectIntensity);

		if (static_cast<Engine::uint32>(light.AttenuationMode) >
			static_cast<Engine::uint32>(Engine::EAttenuationMode::Disabled))
		{
			return false;
		}

		if (!std::isfinite(light.Range) || !IsFiniteLightVector(light.AttenuationCoefficients))
			return false;

		if (light.LightType == Engine::ELightType::Spot)
		{
			return std::isfinite(light.SpotInnerAngle) && std::isfinite(light.SpotOuterAngle) &&
				std::isfinite(light.SpotFalloffExponent);
		}

		return true;
	}
}

BEGIN(Engine)

LightSystem* LightSystem::Create()
{
	return new LightSystem();
}

void LightSystem::OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*> activeScenes, f32 dt)
{
	UpdateLights(globalRegistry);

	for (Scene* scene : activeScenes)
	{
		if (scene != nullptr)
			UpdateLights(scene->GetRegistry());
	}
}

void LightSystem::UpdateLights(entt::registry& registry)
{
	auto view = registry.view<LightComponent>();

	for (auto entity : view)
	{
		auto& light = view.get<LightComponent>(entity);
		light.InvalidateCache();

		if (!light.IsEnabled)
			continue;

		if (const auto* flag = registry.try_get<FlagComponent>(entity))
		{
			if (!HasFlag(flag->flags, EEntityFlag::Active) || HasFlag(flag->flags, EEntityFlag::Dead))
				continue;
		}

		const auto* transform = registry.try_get<WorldTransformComponent>(entity);

		if (transform == nullptr || !IsFiniteLightMatrix(transform->worldMatrix) || !IsValidLightSettings(light))
			continue;

		const vec3 position(transform->worldMatrix[3]);
		vec3 direction(transform->worldMatrix[2]);
		const f32 directionLength = glm::length(direction);

		if (!std::isfinite(directionLength) || directionLength <= 0.000001f)
		{
			if (light.LightType != ELightType::Point)
				continue;

			// Point 광원에서는 방향을 사용하지 않습니다.
			direction = vec3(0.f, 0.f, 1.f);
		}
		else
		{
			direction /= directionLength;
		}

		GPULight result{};
		result.Position = position;
		result.Direction = direction;
		result.Color = glm::max(light.Color, vec3(0.f));
		result.Intensity = glm::max(light.Intensity, 0.f);

		// 실제 그림자 캐시가 만들어진 경우에만 아래에서 다시 설정합니다.
		ELightFlags effectiveFlags = light.Flags & ~ELightFlags::CastShadows;
		EAttenuationMode effectiveAttenuation = light.AttenuationMode;

		if (light.LightType == ELightType::Sky)
		{
			// 기존 SkyLight와 셰이더의 슬롯 사용 방식을 유지합니다.
			result.AttenuationCoeff = glm::max(light.GroundColor, vec3(0.f));
			result.Range = glm::max(light.IndirectIntensity, 0.f);
			effectiveAttenuation = EAttenuationMode::Disabled;
		}
		else
		{
			result.Range = glm::max(light.Range, 0.f);
			result.AttenuationCoeff = glm::max(light.AttenuationCoefficients, vec3(0.f));

			if (light.LightType == ELightType::Spot)
			{
				const f32 innerAngle = glm::clamp(light.SpotInnerAngle, 0.f, 89.f);
				const f32 outerAngle = glm::clamp(light.SpotOuterAngle, innerAngle, 89.f);

				result.SpotInnerCos = glm::cos(glm::radians(innerAngle));
				result.SpotOuterCos = glm::cos(glm::radians(outerAngle));
				result.SpotFalloff = glm::max(light.SpotFalloffExponent, 0.01f);
			}
		}

		const bool wantsDirectionalShadow = light.LightType == ELightType::Directional &&
			HasFlag(light.Flags, ELightFlags::CastShadows);

		const bool validShadowSettings = std::isfinite(light.ShadowRange) && light.ShadowRange > 0.f &&
			std::isfinite(light.ShadowBias) && std::isfinite(light.ShadowSlopeBias) &&
			std::isfinite(light.ShadowNormalBias);

		if (wantsDirectionalShadow && validShadowSettings)
		{
			vec3 up(0.f, 1.f, 0.f);

			// 광원 방향과 Up이 평행하면 lookAt 행렬을 만들 수 없습니다.
			if (std::abs(glm::dot(direction, up)) > 0.999f)
				up = vec3(1.f, 0.f, 0.f);

			CameraBuffer shadow{};
			shadow.viewMatrix = glm::lookAtLH(position, position + direction, up);
			shadow.invViewMatrix = glm::inverse(shadow.viewMatrix);

			const f32 range = light.ShadowRange;
			shadow.projMatrix = glm::orthoLH_ZO(-range, range, -range, range, -range, range * 2.f);
			shadow.invProjMatrix = glm::inverse(shadow.projMatrix);
			shadow.viewProjMatrix = shadow.projMatrix * shadow.viewMatrix;
			shadow.invViewProjMatrix = shadow.invViewMatrix * shadow.invProjMatrix;

			const bool validMatrices = IsFiniteLightMatrix(shadow.viewMatrix) &&
				IsFiniteLightMatrix(shadow.invViewMatrix) && IsFiniteLightMatrix(shadow.projMatrix) &&
				IsFiniteLightMatrix(shadow.invProjMatrix) && IsFiniteLightMatrix(shadow.viewProjMatrix) &&
				IsFiniteLightMatrix(shadow.invViewProjMatrix);

			if (validMatrices)
			{
				light.m_ShadowViewMatrix = shadow.viewMatrix;
				light.m_ShadowViewMatrixInv = shadow.invViewMatrix;
				light.m_ShadowProjMatrix = shadow.projMatrix;
				light.m_ShadowProjMatrixInv = shadow.invProjMatrix;
				light.m_ShadowViewProjMatrix = shadow.viewProjMatrix;
				light.m_ShadowViewProjMatrixInv = shadow.invViewProjMatrix;
				light.m_ShadowParams = vec4(light.ShadowBias, light.ShadowSlopeBias, light.ShadowNormalBias, 0.f);
				light.m_IsShadowCacheValid = true;
				effectiveFlags |= ELightFlags::CastShadows;
			}
		}

		result.PackedFlags = PackLightFlags(
			static_cast<uint32>(effectiveFlags), static_cast<uint32>(light.LightType),
			static_cast<uint32>(effectiveAttenuation));

		light.m_GPULight = result;
		light.m_IsCacheValid = true;
	}
}

END