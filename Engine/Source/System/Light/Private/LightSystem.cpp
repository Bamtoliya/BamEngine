#include "LightSystem.h"
#include "Components/Render/LightComponent.h"
#include "Components/Core/WorldTransformComponent.h"
#include "Components/Core/FlagComponent.h"
#include "Renderer.h"
#include "RenderPass.h"

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
			if (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, EEntityFlag::Dead)) continue;
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
			if (std::abs(glm::dot(direction, up)) > 0.999f) up = vec3(1.f, 0.f, 0.f);

			// 메인 뷰포트 카메라 찾기
			const CameraBuffer* mainCam = nullptr;
			for (const auto& vp : Renderer::Get().GetActiveViewportCameras())
			{
				if (vp.hasCamera && vp.renderPass && vp.renderPass->GetPassType() == ERenderPassType::Geometry)
				{
					mainCam = &vp.cameraBuffer;
					break;
				}
			}

			// 캐스케이드 분할 거리 계산 (기본 빨강 범위를 25m로 확장하고 CascadeSplitLambda로 비율 조절)
			const f32 nearZ = 0.1f;
			const f32 maxDist = glm::clamp(light.ShadowMaxDistance, 20.0f, 500.0f);
			const f32 lambda = glm::clamp(light.CascadeSplitLambda, 0.1f, 1.0f);
			const f32 exponent = glm::mix(1.0f, 1.6f, lambda); // lambda=0.5일 때 약 25m, 60m 분할

			f32 cascadeSplits[MAX_SHADOW_CASCADES + 1];
			cascadeSplits[0] = nearZ;
			for (uint32 i = 1; i <= MAX_SHADOW_CASCADES; ++i)
			{
				const f32 ratio = static_cast<f32>(i) / static_cast<f32>(MAX_SHADOW_CASCADES);
				cascadeSplits[i] = nearZ + (maxDist - nearZ) * std::pow(ratio, exponent);
			}

			light.m_CascadeSplits = vec4(cascadeSplits[1], cascadeSplits[2], cascadeSplits[3], 0.0f);
			bool allCascadesValid = true;

			for (uint32 c = 0; c < MAX_SHADOW_CASCADES; ++c)
			{
				const f32 splitNear = cascadeSplits[c];
				const f32 splitFar = cascadeSplits[c + 1];

				vec3 frustumCorners[8];
				if (mainCam && mainCam->projMatrix[1][1] != 0.0f && mainCam->projMatrix[0][0] != 0.0f)
				{
					const f32 tanHalfFovY = 1.0f / mainCam->projMatrix[1][1];
					const f32 aspect = mainCam->projMatrix[1][1] / mainCam->projMatrix[0][0];

					const f32 xNear = splitNear * tanHalfFovY * aspect;
					const f32 yNear = splitNear * tanHalfFovY;
					const f32 xFar = splitFar * tanHalfFovY * aspect;
					const f32 yFar = splitFar * tanHalfFovY;

					const vec3 cornersVS[8] = {
						vec3(-xNear,  yNear, splitNear), vec3(xNear,  yNear, splitNear),
						vec3(xNear, -yNear, splitNear), vec3(-xNear, -yNear, splitNear),
						vec3(-xFar,   yFar, splitFar),  vec3(xFar,   yFar, splitFar),
						vec3(xFar,  -yFar, splitFar),  vec3(-xFar,  -yFar, splitFar)
					};

					for (int i = 0; i < 8; ++i)
					{
						const vec4 worldCorner = mainCam->invViewMatrix * vec4(cornersVS[i], 1.0f);
						frustumCorners[i] = vec3(worldCorner) / worldCorner.w;
					}
				}
				else
				{
					// 카메라 정보가 없는 경우 광원 위치 기준 폴백
					const f32 r = light.ShadowRange * (float(c + 1) / float(MAX_SHADOW_CASCADES));
					for (int i = 0; i < 8; ++i) frustumCorners[i] = position + vec3((i & 1 ? r : -r), (i & 2 ? r : -r), (i & 4 ? r : -r));
				}

				// 외접 구 바운딩 계산 (카메라 회전 시 섀도우 지글거림 방지)
				vec3 center(0.0f);
				for (int i = 0; i < 8; ++i) center += frustumCorners[i];
				center /= 8.0f;

				f32 radius = 0.0f;
				for (int i = 0; i < 8; ++i) radius = glm::max(radius, glm::length(frustumCorners[i] - center));
				radius = std::ceil(radius * 16.0f) / 16.0f;

				// 광원 뷰 행렬
				const vec3 lightPos = center - direction * radius;
				mat4 shadowView = glm::lookAtLH(lightPos, center, up);

				// 텍셀 스냅핑 (카메라 이동 시 가장자리 떨림 제거)
				const f32 shadowMapRes = 2048.0f;
				const f32 worldUnitsPerTexel = (radius * 2.0f) / shadowMapRes;
				vec4 centerLS = shadowView * vec4(center, 1.0f);
				centerLS.x = std::floor(centerLS.x / worldUnitsPerTexel) * worldUnitsPerTexel;
				centerLS.y = std::floor(centerLS.y / worldUnitsPerTexel) * worldUnitsPerTexel;
				center = vec3(glm::inverse(shadowView) * centerLS);
				shadowView = glm::lookAtLH(center - direction * radius, center, up);

				// 직교 투영 행렬 (Near: 0, Far: radius * 2)
				const mat4 shadowProj = glm::orthoLH_ZO(-radius, radius, -radius, radius, 0.0f, radius * 2.0f);
				const mat4 shadowViewProj = shadowProj * shadowView;

				if (!IsFiniteLightMatrix(shadowViewProj))
				{
					allCascadesValid = false;
					break;
				}

				CameraBuffer camBuf{};
				camBuf.viewMatrix = shadowView;
				camBuf.invViewMatrix = glm::inverse(shadowView);
				camBuf.projMatrix = shadowProj;
				camBuf.invProjMatrix = glm::inverse(shadowProj);
				camBuf.viewProjMatrix = shadowViewProj;
				camBuf.invViewProjMatrix = glm::inverse(shadowViewProj);
				camBuf.cameraPosition = center - direction * radius;

				light.m_ShadowCameraBuffers[c] = camBuf;
				light.m_ShadowViewProjMatrices[c] = shadowViewProj;
			}

			if (allCascadesValid)
			{
				light.m_ShadowParams = vec4(light.ShadowBias, light.ShadowSlopeBias, light.ShadowNormalBias, 0.f);
				light.m_IsShadowCacheValid = true;
				effectiveFlags |= ELightFlags::CastShadows;
			}
		} // <── 1. 여기서 Directional Light 블록을 완전히 닫아줍니다!

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

				light.m_ShadowCameraBuffers[0] = camBuf;
				light.m_ShadowViewProjMatrices[0] = spotViewProj;
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