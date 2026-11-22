#include "SkyAtmosphereSystem.h"
#include "ComponentEnable.h"
#include "SkyAtmosphereComponent.h"
#include "LightComponent.h"
#include "WorldComponents.h"
#include "Scene.h"
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

void SkyAtmosphereSystem::OnUpdate(entt::registry& globalRegistry, const std::vector<Engine::Scene*>& activeScenes, f32 dt)
{
	for (Engine::Scene* scene : activeScenes)
	{
		if (!scene || scene->IsPaused()) continue;
		auto& registry = scene->GetRegistry();

		auto skyView = registry.view<Engine::SkyAtmosphereComponent>();
		for (auto [e, sky] : skyView.each())
		{
			if (!Engine::IsComponentEnabled(registry, e, sky)) continue;

			// 1. 시간 시뮬레이션
			if (sky.EnableTimeProgression && sky.DayCycleDuration > 0.1f)
			{
				sky.TimeOfDay = std::fmod(sky.TimeOfDay + (24.0f / sky.DayCycleDuration) * dt, 24.0f);
				if (sky.TimeOfDay < 0.0f) sky.TimeOfDay += 24.0f;
			}

			// 2. 태양 각도 계산
			f32 hourAngle = (sky.TimeOfDay - 12.0f) * (glm::pi<float>() / 12.0f);
			f32 latRad = glm::radians(sky.Latitude);
			vec3 toSun = glm::normalize(vec3(std::sin(hourAngle), std::cos(hourAngle) * std::cos(latRad), -std::cos(hourAngle) * std::sin(latRad)));

			// 3. Directional Light Transform 회전 동기화
			if (sky.AutoSyncDirectionalLight)
			{
				auto lightView = registry.view<Engine::LightComponent, Engine::TransformComponent>();
				for (auto [le, light, tr] : lightView.each())
				{
					if (Engine::IsComponentEnabled(registry, le, light) && light.LightType == Engine::ELightType::Directional)
					{
						vec3 forward = -toSun;
						f32 pitch = glm::degrees(std::asin(-forward.y));
						f32 yaw = glm::degrees(std::atan2(forward.x, forward.z));
						tr.eulerRotation = vec3(pitch, yaw, 0.0f);
						tr.rotation = glm::quat(glm::radians(tr.eulerRotation));
						break;
					}
				}
			}
		}
	}
}