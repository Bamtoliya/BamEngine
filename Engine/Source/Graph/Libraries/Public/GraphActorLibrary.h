#pragma once

#include "GraphAnnotations.h"
#include "GraphSystem.h"
#include "ComponentEnable.h"
#include "Scene.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "TransformSystem.h"
#include "FlagComponent.h"
#include "LightComponent.h"
#include "PrefabInstanceComponent.h"
#include "PlatformPath.h"
#include <archive/JsonArchive.h>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <glm/glm.hpp>

namespace Engine::GraphActor
{
	// ── Transform 조작 노드 ──
	FUNCTION(GRAPH_CALLABLE("Transform.AddWorldOffset"), DISPLAY_NAME("Add World Offset"), CATEGORY("Transformation"),
		GRAPH_PIN_DEFAULT("x", 0.0f), GRAPH_PIN_DEFAULT("y", 0.0f), GRAPH_PIN_DEFAULT("z", 0.0f))
		inline void AddWorldOffset(float x, float y, float z)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;

		auto& reg = scene->GetRegistry();
		auto* transform = reg.try_get<TransformComponent>(entity);
		if (!transform) return;

		transform->position += glm::vec3(x, y, z);
	}

	FUNCTION(GRAPH_CALLABLE("Transform.AddLocalOffset"), DISPLAY_NAME("Add Local Offset"), CATEGORY("Transformation"),
		GRAPH_PIN_DEFAULT("x", 0.0f), GRAPH_PIN_DEFAULT("y", 0.0f), GRAPH_PIN_DEFAULT("z", 0.0f))
		inline void AddLocalOffset(float x, float y, float z)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;

		auto& reg = scene->GetRegistry();
		auto* transform = reg.try_get<TransformComponent>(entity);
		if (!transform) return;

		(void)transform->TranslateLocal(glm::vec3(x, y, z));
	}

	FUNCTION(GRAPH_CALLABLE("Transform.AddLocalRotation"), DISPLAY_NAME("Add Local Rotation"), CATEGORY("Transformation"),
		GRAPH_PIN_DEFAULT("pitch", 0.0f), GRAPH_PIN_DEFAULT("yaw", 0.0f), GRAPH_PIN_DEFAULT("roll", 0.0f))
		inline void AddLocalRotation(float pitch, float yaw, float roll)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;

		auto& reg = scene->GetRegistry();
		auto* transform = reg.try_get<TransformComponent>(entity);
		if (!transform) return;

		transform->eulerRotation += glm::vec3(pitch, yaw, roll);
		(void)transform->SynchronizeRotation();
	}

	FUNCTION(GRAPH_PURE("Transform.GetLocation"), DISPLAY_NAME("Get Actor Location"), CATEGORY("Transformation"))
		inline std::array<float, 3> GetLocation()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return { 0.0f, 0.0f, 0.0f };

		auto& reg = scene->GetRegistry();
		const auto* transform = reg.try_get<TransformComponent>(entity);
		if (!transform) return { 0.0f, 0.0f, 0.0f };

		return { transform->position.x, transform->position.y, transform->position.z };
	}

	FUNCTION(GRAPH_CALLABLE("Transform.SetLocation"), DISPLAY_NAME("Set Actor Location"), CATEGORY("Transformation"),
		GRAPH_PIN_DEFAULT("x", 0.0f), GRAPH_PIN_DEFAULT("y", 0.0f), GRAPH_PIN_DEFAULT("z", 0.0f))
		inline void SetLocation(float x, float y, float z)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;

		auto& reg = scene->GetRegistry();
		auto* transform = reg.try_get<TransformComponent>(entity);
		if (!transform) return;

		transform->position = glm::vec3(x, y, z);
	}

	FUNCTION(GRAPH_PURE("Transform.GetForwardVector"), DISPLAY_NAME("Get Forward Vector"), CATEGORY("Transformation"))
		inline std::array<float, 3> GetForwardVector()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return { 0.0f, 0.0f, 1.0f };

		auto& reg = scene->GetRegistry();
		const auto* transform = reg.try_get<TransformComponent>(entity);
		if (!transform) return { 0.0f, 0.0f, 1.0f };

		const glm::vec3 forward = transform->GetLocalForward();
		return { forward.x, forward.y, forward.z };
	}

	// ── 액터 상태 노드 ──
	FUNCTION(GRAPH_CALLABLE("Actor.SetActive"), DISPLAY_NAME("Set Actor Active"), CATEGORY("Actor"),
		GRAPH_PIN_DEFAULT("active", true))
		inline void SetActorActive(bool active)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;

		auto& reg = scene->GetRegistry();
		auto* flag = reg.try_get<FlagComponent>(entity);
		if (flag) flag->SetActive(active);
	}

	FUNCTION(GRAPH_PURE("Actor.IsActive"), DISPLAY_NAME("Is Actor Active"), CATEGORY("Actor"))
		inline bool IsActorActive()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return false;

		auto& reg = scene->GetRegistry();
		const auto* flag = reg.try_get<FlagComponent>(entity);
		return flag ? flag->IsActive() : true;
	}

	FUNCTION(GRAPH_CALLABLE("Actor.SetVisible"), DISPLAY_NAME("Set Actor Visible"), CATEGORY("Actor"),
		GRAPH_PIN_DEFAULT("visible", true))
		inline void SetActorVisible(bool visible)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;

		auto& reg = scene->GetRegistry();
		auto* flag = reg.try_get<FlagComponent>(entity);
		if (flag) flag->SetVisible(visible);
	}

	FUNCTION(GRAPH_PURE("Actor.IsVisible"), DISPLAY_NAME("Is Actor Visible"), CATEGORY("Actor"))
		inline bool IsActorVisible()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return false;

		auto& reg = scene->GetRegistry();
		const auto* flag = reg.try_get<FlagComponent>(entity);
		return flag ? flag->IsVisible() : true;
	}

	// ── 조명 제어 노드 ──
	FUNCTION(GRAPH_CALLABLE("Light.SetColor"), DISPLAY_NAME("Set Light Color"), CATEGORY("Lighting"),
		GRAPH_PIN_DEFAULT("r", 1.0f), GRAPH_PIN_DEFAULT("g", 1.0f), GRAPH_PIN_DEFAULT("b", 1.0f))
		inline void SetLightColor(float r, float g, float b)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;

		auto& reg = scene->GetRegistry();
		auto* light = TryGetEnabledComponent<LightComponent>(reg, entity);
		if (!light) return;

		light->Color = glm::vec3(r, g, b);
	}

	FUNCTION(GRAPH_CALLABLE("Light.SetIntensity"), DISPLAY_NAME("Set Light Intensity"), CATEGORY("Lighting"),
		GRAPH_PIN_DEFAULT("intensity", 1.0f))
		inline void SetLightIntensity(float intensity)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;

		auto& reg = scene->GetRegistry();
		auto* light = TryGetEnabledComponent<LightComponent>(reg, entity);
		if (!light) return;

		light->Intensity = intensity;
	}

	FUNCTION(GRAPH_CALLABLE("Light.SetEnabled"), DISPLAY_NAME("Set Light Enabled"), CATEGORY("Lighting"),
		GRAPH_PIN_DEFAULT("enabled", true))
		inline void SetLightEnabled(bool enabled)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;

		auto& reg = scene->GetRegistry();
		auto* light = reg.try_get<LightComponent>(entity);
		if (!light || light->IsEnabled == enabled) return;

		light->IsEnabled = enabled;
		light->InvalidateCache();
	}

	// ── 엔티티 생명주기 노드 ──
	FUNCTION(GRAPH_CALLABLE("Entity.DestroySelf"), DISPLAY_NAME("Destroy Actor"), CATEGORY("Entity"))
		inline void DestroySelf()
	{
		GraphSystem::RequestCurrentEntityDestroy();
	}

	FUNCTION(GRAPH_CALLABLE("Entity.SpawnPrefab"), DISPLAY_NAME("Spawn Prefab"), CATEGORY("Entity"),
		GRAPH_PIN_DEFAULT("prefabPath", ""),
		GRAPH_PIN_DEFAULT("x", 0.0f), GRAPH_PIN_DEFAULT("y", 0.0f), GRAPH_PIN_DEFAULT("z", 0.0f))
		inline std::int64_t SpawnPrefab(std::string prefabPath, float x, float y, float z)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene || prefabPath.empty()) return -1;

		const std::wstring wPath = Engine::Unicode::Utf8ToWide(prefabPath);
		const auto physical = PlatformPath::ResolveUri(wPath);
		archive::JsonArchive ar;
		auto result = ar.LoadFromFile(physical);
		if (!result) return -1;

		Entity* rootEntity = nullptr;
		result = scene->InstantiatePrefabArchive(ar, rootEntity, nullptr);
		if (!result || !rootEntity) return -1;

		auto& reg = scene->GetRegistry();
		const entt::entity spawned = rootEntity->GetEntityHandle();
		auto& prefabComp = reg.emplace_or_replace<PrefabInstanceComponent>(spawned);
		const auto uri = PlatformPath::ToVirtualUri(physical);
		prefabComp.prefabAssetPath = PlatformPath::IsProjectUri(uri) ? uri : wPath;

		if (auto* transform = reg.try_get<TransformComponent>(spawned))
		{
			(void)transform->SetLocalPosition(glm::vec3(x, y, z));
			if (auto* worldTransform = reg.try_get<WorldTransformComponent>(spawned))
				TransformSystem::UpdateRootWorld(*transform, *worldTransform);
		}
		return static_cast<std::int64_t>(static_cast<std::uint32_t>(spawned));
	}

	FUNCTION(GRAPH_CALLABLE("Entity.DestroyEntity"), DISPLAY_NAME("Destroy Entity"), CATEGORY("Entity"),
		GRAPH_PIN_DEFAULT("entityId", -1))
		inline bool DestroyEntity(std::int64_t entityId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene || entityId < 0 ||
			entityId > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()))
			return false;

		const entt::entity target = static_cast<entt::entity>(static_cast<std::uint32_t>(entityId));
		if (target == GraphSystem::GetCurrentEntity())
		{
			GraphSystem::RequestCurrentEntityDestroy();
			return true;
		}

		Entity* wrapper = scene->FindEntity(target);
		if (!wrapper) return false;
		return scene->DestroyEntity(*wrapper);
	}
}
