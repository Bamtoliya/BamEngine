#include "SkyAtmosphereSystem.h"
#include "Components/Render/SkyAtmosphereComponent.h"
#include "Components/Render/LightComponent.h"
#include "CoreComponents.h"
#include "Scene.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "CloudTextureGenerator.h"
#include "Material.h"
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include "PlatformPath.h"

void SkyAtmosphereSystem::InitializeResources()
{
	auto& rm = Engine::ResourceManager::Get();
	auto* rhi = Engine::Renderer::Get().GetRHI();
	if (!rhi) return;

	const std::wstring shapeKey = L"Resources/Texture/CloudBaseShape3D.bamtex";
	const std::wstring weatherKey = L"Resources/Texture/CloudWeatherMap2D.bamtex";
	// 1. 디스크에 이미 캐싱된 파일이 있는지 확인 및 로드
	auto shapeHandle = rm.GetResourceHandle<Engine::Texture>(shapeKey);
	if (!shapeHandle.IsValid())
	{
		auto resolved = Engine::PlatformPath::ResolveProjectRelative(shapeKey);
		if (std::filesystem::exists(resolved))
			shapeHandle = Engine::ResourceHandle<Engine::Texture>(rm.LoadFile(shapeKey));
	}
	auto weatherHandle = rm.GetResourceHandle<Engine::Texture>(weatherKey);
	if (!weatherHandle.IsValid())
	{
		auto resolved = Engine::PlatformPath::ResolveProjectRelative(weatherKey);
		if (std::filesystem::exists(resolved))
			weatherHandle = Engine::ResourceHandle<Engine::Texture>(rm.LoadFile(weatherKey));
	}
	// 2. 리소스가 없으면 새로 생성 후 디스크에 .bamtex 파일로 저장
	if (!shapeHandle.IsValid())
	{
		Engine::Texture* shapeTex = Engine::CloudTextureGenerator::CreateBaseShapeTexture(rhi, shapeKey, 64);
		if (shapeTex)
		{
			shapeHandle = rm.AddResource<Engine::Texture>(shapeKey, shapeTex);
			rm.SaveToBinaryFile(shapeTex, shapeKey);
		}
	}
	if (!weatherHandle.IsValid())
	{
		Engine::Texture* weatherTex = Engine::CloudTextureGenerator::CreateWeatherMapTexture(rhi, weatherKey, 256);
		if (weatherTex)
		{
			weatherHandle = rm.AddResource<Engine::Texture>(weatherKey, weatherTex);
			rm.SaveToBinaryFile(weatherTex, weatherKey);
		}
	}

	// 3. SkyMaterial 바인딩
	auto skyMatHandle = rm.GetResourceHandle<Engine::Material>(L"Resources/Material/SkyMaterial.bammat.json");
	if (!skyMatHandle.IsValid())
		skyMatHandle = rm.GetResourceHandle<Engine::Material>(L"Resources/Material/SkyMaterial");

	if (auto* skyMat = skyMatHandle.Get())
	{
		skyMat->SetTextureBinding("CloudBaseShape3D", 0, shapeHandle);
		skyMat->SetTextureBinding("CloudWeatherMap2D", 1, weatherHandle);
	}
}

void SkyAtmosphereSystem::OnUpdate(entt::registry& globalRegistry, const std::vector<Engine::Scene*>& activeScenes, f32 dt)
{
	for (Engine::Scene* scene : activeScenes)
	{
		if (!scene || scene->IsPaused()) continue;
		auto& registry = scene->GetRegistry();

		auto skyView = registry.view<Engine::SkyAtmosphereComponent>();
		for (auto [e, sky] : skyView.each())
		{
			if (!sky.IsEnabled) continue;

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
					if (light.IsEnabled && light.LightType == Engine::ELightType::Directional)
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