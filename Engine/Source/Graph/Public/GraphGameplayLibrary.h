#pragma once

#include "GraphAnnotations.h"
#include "System/Graph/Public/GraphSystem.h"
#include "Components/Physics/RigidBodyComponent.h"
#include "System/Audio/Public/AudioManager.h"
#include "System/Input/Public/InputManager.h"
#include "System/Physics/Public/PhysicsSystem.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include "Logger.h"
#include "Components/Render/SpriteAnimationComponent.h"


namespace Engine::GraphGameplay
{
	// ── 디버그 출력 노드 (Debug) ──
		// ── 디버그 출력 노드 (Debug) ──
	FUNCTION(GRAPH_CALLABLE("Debug.PrintFloat"), DISPLAY_NAME("Print Float"), CATEGORY("Debug"),
		GRAPH_PIN_DEFAULT("value", 0.0f))
		inline void PrintFloat(float value)
	{
		BAM_LOG(Info, "Engine", "BlueprintLog", "Float: {}", value);
	}

	FUNCTION(GRAPH_CALLABLE("Debug.PrintInt"), DISPLAY_NAME("Print Int"), CATEGORY("Debug"),
		GRAPH_PIN_DEFAULT("value", 0))
		inline void PrintInt(std::int64_t value)
	{
		BAM_LOG(Info, "Engine", "BlueprintLog", "Int: {}", value);
	}

	FUNCTION(GRAPH_CALLABLE("Debug.PrintVector"), DISPLAY_NAME("Print Vector"), CATEGORY("Debug"))
		inline void PrintVector(std::array<float, 3> value)
	{
		BAM_LOG(Info, "Engine", "BlueprintLog", "Vector: ({}, {}, {})", value[0], value[1], value[2]);
	}

	// ── 시간 노드 (Time) ──
	FUNCTION(GRAPH_PURE("Time.GetDeltaSeconds"), DISPLAY_NAME("Get Delta Seconds"), CATEGORY("Time"))
		inline float GetDeltaSeconds()
	{
		return GraphSystem::GetCurrentDeltaTime();
	}

	// ── 입력 노드 (Input) ──
	FUNCTION(GRAPH_PURE("Input.GetAxisHorizontal"), DISPLAY_NAME("Get Axis Horizontal (A/D/Left/Right)"), CATEGORY("Input"))
		inline float GetAxisHorizontal()
	{
		auto& input = InputManager::Get();
		float axis = 0.0f;
		if (input.IsKeyButtonPressed(EKeyCode::D) || input.IsKeyButtonPressed(EKeyCode::Right)) axis += 1.0f;
		if (input.IsKeyButtonPressed(EKeyCode::A) || input.IsKeyButtonPressed(EKeyCode::Left)) axis -= 1.0f;
		return axis;
	}

	FUNCTION(GRAPH_PURE("Input.GetAxisVertical"), DISPLAY_NAME("Get Axis Vertical (W/S/Up/Down)"), CATEGORY("Input"))
		inline float GetAxisVertical()
	{
		auto& input = InputManager::Get();
		float axis = 0.0f;
		if (input.IsKeyButtonPressed(EKeyCode::W) || input.IsKeyButtonPressed(EKeyCode::Up)) axis += 1.0f;
		if (input.IsKeyButtonPressed(EKeyCode::S) || input.IsKeyButtonPressed(EKeyCode::Down)) axis -= 1.0f;
		return axis;
	}

	FUNCTION(GRAPH_PURE("Input.IsSpacePressed"), DISPLAY_NAME("Is Space Pressed"), CATEGORY("Input"))
		inline bool IsSpacePressed()
	{
		return InputManager::Get().IsKeyButtonPressed(EKeyCode::Space);
	}

	FUNCTION(GRAPH_PURE("Input.IsKeyPressed"), DISPLAY_NAME("Is Key Pressed"), CATEGORY("Input"),
		GRAPH_PIN_DEFAULT("keyCode", 0))
		inline bool IsKeyPressed(std::int64_t keyCode)
	{
		if (keyCode <= 0 || keyCode >= static_cast<std::int64_t>(EKeyCode::Count)) return false;
		return InputManager::Get().IsKeyButtonPressed(static_cast<EKeyCode>(keyCode));
	}

	// ── 물리 제어 노드 (Physics) ──
	FUNCTION(GRAPH_CALLABLE("Physics.AddImpulse"), DISPLAY_NAME("Add Impulse"), CATEGORY("Physics"),
		GRAPH_PIN_DEFAULT("x", 0.0f), GRAPH_PIN_DEFAULT("y", 0.0f), GRAPH_PIN_DEFAULT("z", 0.0f))
		inline void AddImpulse(float x, float y, float z)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;

		auto& reg = scene->GetRegistry();
		auto* rb = reg.try_get<RigidBodyComponent>(entity);
		if (!rb || rb->BodyType != ERigidBodyType::Dynamic || rb->Mass <= 0.0f) return;

		rb->LinearVelocity += glm::vec3(x, y, z) / rb->Mass;
	}

	FUNCTION(GRAPH_CALLABLE("Physics.AddForce"), DISPLAY_NAME("Add Force"), CATEGORY("Physics"),
		GRAPH_PIN_DEFAULT("x", 0.0f), GRAPH_PIN_DEFAULT("y", 0.0f), GRAPH_PIN_DEFAULT("z", 0.0f))
		inline void AddForce(float x, float y, float z)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;

		auto& reg = scene->GetRegistry();
		auto* rb = reg.try_get<RigidBodyComponent>(entity);
		if (!rb || rb->BodyType != ERigidBodyType::Dynamic) return;

		rb->AccumulatedForce += glm::vec3(x, y, z);
	}

	FUNCTION(GRAPH_CALLABLE("Physics.SetLinearVelocity"), DISPLAY_NAME("Set Linear Velocity"), CATEGORY("Physics"),
		GRAPH_PIN_DEFAULT("x", 0.0f), GRAPH_PIN_DEFAULT("y", 0.0f), GRAPH_PIN_DEFAULT("z", 0.0f))
		inline void SetLinearVelocity(float x, float y, float z)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;

		auto& reg = scene->GetRegistry();
		auto* rb = reg.try_get<RigidBodyComponent>(entity);
		if (!rb) return;

		rb->LinearVelocity = glm::vec3(x, y, z);
	}

	FUNCTION(GRAPH_PURE("Physics.GetLinearVelocity"), DISPLAY_NAME("Get Linear Velocity"), CATEGORY("Physics"))
		inline std::array<float, 3> GetLinearVelocity()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return { 0.0f, 0.0f, 0.0f };

		auto& reg = scene->GetRegistry();
		const auto* rb = reg.try_get<RigidBodyComponent>(entity);
		if (!rb) return { 0.0f, 0.0f, 0.0f };

		return { rb->LinearVelocity.x, rb->LinearVelocity.y, rb->LinearVelocity.z };
	}

	// ── 오디오 노드 (Audio) ──
	FUNCTION(GRAPH_CALLABLE("Audio.StopAll"), DISPLAY_NAME("Stop All Audio"), CATEGORY("Audio"))
		inline void StopAllAudio()
	{
		AudioManager::Get().StopAllSounds();
	}

	FUNCTION(GRAPH_CALLABLE("Audio.SetMasterVolume"), DISPLAY_NAME("Set Master Volume"), CATEGORY("Audio"),
		GRAPH_PIN_DEFAULT("volume", 1.0f))
		inline void SetMasterVolume(float volume)
	{
		AudioManager::Get().SetMasterVolume(std::clamp(volume, 0.0f, 1.0f));
	}

	// ── 물리 레이캐스트 노드 (Line Trace) ──
	inline SceneRaycastHit s_LastRaycastHit{};
	FUNCTION(GRAPH_CALLABLE("Physics.Raycast"), DISPLAY_NAME("Raycast (Line Trace)"), CATEGORY("Physics"),
		GRAPH_PIN_DEFAULT("originX", 0.0f), GRAPH_PIN_DEFAULT("originY", 0.0f), GRAPH_PIN_DEFAULT("originZ", 0.0f),
		GRAPH_PIN_DEFAULT("dirX", 0.0f), GRAPH_PIN_DEFAULT("dirY", -1.0f), GRAPH_PIN_DEFAULT("dirZ", 0.0f),
		GRAPH_PIN_DEFAULT("maxDistance", 1000.0f))
		inline bool LineTrace(float originX, float originY, float originZ,
			float dirX, float dirY, float dirZ, float maxDistance)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return false;
		const glm::vec3 dir = glm::vec3(dirX, dirY, dirZ);
		const float dirLen = glm::length(dir);
		if (dirLen <= 0.0001f) return false;
		const Ray ray(vec3(originX, originY, originZ), dir / dirLen);
		const bool hit = PhysicsSystem::Raycast(*scene, ray, s_LastRaycastHit, maxDistance);
		return hit;
	}
	FUNCTION(GRAPH_PURE("Physics.GetLastHitLocation"), DISPLAY_NAME("Get Last Hit Location"), CATEGORY("Physics"))
		inline std::array<float, 3> GetLastHitLocation()
	{
		return { s_LastRaycastHit.Hit.point.x, s_LastRaycastHit.Hit.point.y, s_LastRaycastHit.Hit.point.z };
	}
	FUNCTION(GRAPH_PURE("Physics.GetLastHitNormal"), DISPLAY_NAME("Get Last Hit Normal"), CATEGORY("Physics"))
		inline std::array<float, 3> GetLastHitNormal()
	{
		return { s_LastRaycastHit.Hit.normal.x, s_LastRaycastHit.Hit.normal.y, s_LastRaycastHit.Hit.normal.z };
	}
	FUNCTION(GRAPH_PURE("Physics.GetLastHitDistance"), DISPLAY_NAME("Get Last Hit Distance"), CATEGORY("Physics"))
		inline float GetLastHitDistance()
	{
		return s_LastRaycastHit.Hit.distance;
	}

	// ── 스프라이트 애니메이션 노드 (Sprite Flipbook) ──
	FUNCTION(GRAPH_CALLABLE("Sprite.PlayAnimation"), DISPLAY_NAME("Play Sprite Animation"), CATEGORY("Sprite"))
		inline void PlaySpriteAnimation()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<SpriteAnimationComponent>(entity);
		if (anim) anim->Play();
	}
	FUNCTION(GRAPH_CALLABLE("Sprite.PauseAnimation"), DISPLAY_NAME("Pause Sprite Animation"), CATEGORY("Sprite"))
		inline void PauseSpriteAnimation()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<SpriteAnimationComponent>(entity);
		if (anim) anim->Pause();
	}
	FUNCTION(GRAPH_CALLABLE("Sprite.SetAnimationFPS"), DISPLAY_NAME("Set Sprite Animation FPS"), CATEGORY("Sprite"),
		GRAPH_PIN_DEFAULT("fps", 12.0f))
		inline void SetSpriteAnimationFPS(float fps)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<SpriteAnimationComponent>(entity);
		if (anim) anim->framesPerSecond = std::max(0.1f, fps);
	}
}