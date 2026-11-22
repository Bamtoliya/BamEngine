#include "LightSystem.h"
#include "ComponentEnable.h"
#include "LightComponent.h"
#include "WorldTransformComponent.h"
#include "FlagComponent.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

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

EResult LightSystem::Initialize(void* arg)
{
	(void)arg;
	return EResult::Success;
}

LightSystem* LightSystem::Create(void* arg)
{
	LightSystem* instance = new LightSystem();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void LightSystem::Free()
{
}

void LightSystem::OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*>& activeScenes, f32 dt)
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

		if (!IsComponentEnabled(registry, entity, light))
			continue;

		if (const auto* flag = registry.try_get<FlagComponent>(entity))
		{
			if (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, EEntityFlag::Dead)) continue;
		}

		const auto* transform = registry.try_get<WorldTransformComponent>(entity);

		if (transform == nullptr || !IsFiniteLightMatrix(transform->worldMatrix) || !IsValidLightSettings(light))
			continue;

		const vec3 position(transform->worldMatrix[3]);
		// Sky Light의 방향은 하늘 쪽(엔티티의 위)이고, 나머지 광원은 빛이 나아가는 쪽(엔티티의 전방)이다.
		const bool isSkyLight = light.LightType == ELightType::Sky;
		vec3 direction(transform->worldMatrix[isSkyLight ? 1 : 2]);
		const f32 directionLength = glm::length(direction);

		if (!std::isfinite(directionLength) || directionLength <= 0.000001f)
		{
			if (light.LightType != ELightType::Point && !isSkyLight)
				continue;

			// Point 광원에서는 방향을 사용하지 않습니다. Sky Light는 월드 위쪽을 사용합니다.
			direction = isSkyLight ? vec3(0.f, 1.f, 0.f) : vec3(0.f, 0.f, 1.f);
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
		result.ShadowStrength = glm::clamp(light.ShadowStrength, 0.0f, 1.0f);

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

		// 캐스케이드는 뷰마다 다르므로 여기서는 설정만 저장하고 Renderer와 조명 패스가 뷰의 카메라로 만든다.
		if (wantsDirectionalShadow && validShadowSettings)
		{
			DirectionalShadowSettings settings;
			settings.Position = position;
			settings.Direction = direction;
			settings.FallbackRange = light.ShadowRange;
			settings.MaxDistance = light.ShadowMaxDistance;
			settings.SplitLambda = light.CascadeSplitLambda;
			settings.ShadowParams = vec4(light.ShadowBias, light.ShadowSlopeBias, light.ShadowNormalBias, 0.f);

			DirectionalShadowCascades fallback;
			if (BuildDirectionalShadowCascades(settings, nullptr, fallback))
			{
				light.m_DirectionalShadow = settings;
				light.m_IsShadowCacheValid = true;
				effectiveFlags |= ELightFlags::CastShadows;
			}
		}

		// ── 2. Spot Light Shadow (Perspective) 독립 블록 ──
		const bool wantsSpotShadow = light.LightType == ELightType::Spot &&
			HasFlag(light.Flags, ELightFlags::CastShadows);

		if (wantsSpotShadow && validShadowSettings)
		{
			vec3 up(0.f, 1.f, 0.f);
			if (std::abs(glm::dot(direction, up)) > 0.999f) up = vec3(1.f, 0.f, 0.f);

			const f32 innerAngle = glm::clamp(light.SpotInnerAngle, 0.f, 89.f);
			const f32 outerAngle = glm::clamp(light.SpotOuterAngle, innerAngle, 89.f);
			const f32 fov = glm::clamp(glm::radians(outerAngle * 2.0f), glm::radians(1.0f), glm::radians(179.0f));
			const f32 nearZ = 0.05f;
			const f32 farZ = glm::max(light.Range, 0.1f);

			const mat4 spotView = glm::lookAtLH(position, position + direction, up);
			const mat4 spotProj = glm::perspectiveLH_ZO(fov, 1.0f, nearZ, farZ);
			const mat4 spotViewProj = spotProj * spotView;

			if (IsFiniteLightMatrix(spotViewProj))
			{
				CameraBuffer camBuf{};
				camBuf.viewMatrix = spotView;
				camBuf.invViewMatrix = glm::inverse(spotView);
				camBuf.projMatrix = spotProj;
				camBuf.invProjMatrix = glm::inverse(spotProj);
				camBuf.viewProjMatrix = spotViewProj;
				camBuf.invViewProjMatrix = glm::inverse(spotViewProj);
				camBuf.cameraPosition = position;

				light.m_SpotShadowCameraBuffer = camBuf;
				light.m_ShadowParams = vec4(light.ShadowBias, light.ShadowSlopeBias, light.ShadowNormalBias, 1.0f);
				light.m_IsShadowCacheValid = true;
				effectiveFlags |= ELightFlags::CastShadows;
			}
		}

		// ── 3. Point Light Shadow (Omnidirectional 6-Face Cubemap) ──
		const bool wantsPointShadow = light.LightType == ELightType::Point &&
			HasFlag(light.Flags, ELightFlags::CastShadows);

		if (wantsPointShadow && validShadowSettings)
		{
			// DirectX LH 큐브맵 규격 6개 축 방향 및 업 벡터 (+X, -X, +Y, -Y, +Z, -Z)
			static const vec3 cubeDirections[6] = {
				vec3(1.f,  0.f,  0.f), vec3(-1.f,  0.f,  0.f),
				vec3(0.f,  1.f,  0.f), vec3(0.f, -1.f,  0.f),
				vec3(0.f,  0.f,  1.f), vec3(0.f,  0.f, -1.f)
			};
			static const vec3 cubeUps[6] = {
				vec3(0.f, 1.f,  0.f), vec3(0.f, 1.f, 0.f),
				vec3(0.f, 0.f, -1.f), vec3(0.f, 0.f, 1.f),
				vec3(0.f, 1.f,  0.f), vec3(0.f, 1.f, 0.f)
			};

			const f32 nearZ = 0.05f;
			const f32 farZ = glm::max(light.Range, 0.1f);
			const mat4 pointProj = glm::perspectiveLH_ZO(glm::radians(90.0f), 1.0f, nearZ, farZ);

			bool allFacesValid = true;
			for (uint32 f = 0; f < 6; ++f)
			{
				const mat4 pointView = glm::lookAtLH(position, position + cubeDirections[f], cubeUps[f]);
				const mat4 pointViewProj = pointProj * pointView;

				if (!IsFiniteLightMatrix(pointViewProj))
				{
					allFacesValid = false;
					break;
				}

				CameraBuffer camBuf{};
				camBuf.viewMatrix = pointView;
				camBuf.invViewMatrix = glm::inverse(pointView);
				camBuf.projMatrix = pointProj;
				camBuf.invProjMatrix = glm::inverse(pointProj);
				camBuf.viewProjMatrix = pointViewProj;
				camBuf.invViewProjMatrix = glm::inverse(pointViewProj);
				camBuf.cameraPosition = position;

				light.m_PointShadowCameraBuffers[f] = camBuf;
				light.m_PointShadowViewProjMatrices[f] = pointViewProj;
			}

			if (allFacesValid)
			{
				light.m_ShadowParams = vec4(light.ShadowBias, light.ShadowSlopeBias, light.ShadowNormalBias, 1.0f);
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