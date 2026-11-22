#pragma once

#include "GraphAnnotations.h"
#include "GraphSystem.h"
#include "RigidBodyComponent.h"
#include "ComponentEnable.h"
#include "AudioManager.h"
#include "AudioClip.h"
#include "ResourceManager.h"
#include "CompositeVFXComponent.h"
#include "CompositeEffect.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "TransformSystem.h"
#include "NameComponent.h"
#include "HealthComponent.h"
#include "InputManager.h"
#include "PhysicsSystem.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include "Logger.h"
#include "SpriteAnimationComponent.h"
#include "CharacterControllerComponent.h"
#include "AnimatorComponent.h"
#include "AnimationSceneQuery.h"
#include "ThirdPersonControllerComponent.h"
#include "SpringArmComponent.h"
#include "ActionTimelineComponent.h"
#include "FlagComponent.h"
#include "AnimationIK.h"
#include "SaveGameManager.h"
#include "NavAgentComponent.h"
#include "AIPerceptionComponent.h"
#include "NavigationSystem.h"
#include "AudioMixer.h"
#include "SceneManager.h"
#include "ScenePersistenceComponent.h"

namespace Engine
{
	class ENGINE_API GraphGameplayLibrary
	{
	public:
		static void SetAnimationIKEnabled(uint64 entityId, bool enabled);
		static void SetTwoBoneIKTarget(uint64 entityId, int32 chainIndex, vec3 targetPos, float weight = 1.0f);
		static void SetLookAtIKTarget(uint64 entityId, int32 targetIndex, vec3 targetPos, float weight = 1.0f);
		static void SetFootGroundIKEnabled(uint64 entityId, bool enabled, float footHeightOffset = 0.05f);

		// Overloads taking an explicit Scene reference (for testing and direct script invocations)
		static void SetAnimationIKEnabled(Scene& scene, uint64 entityId, bool enabled);
		static void SetTwoBoneIKTarget(Scene& scene, uint64 entityId, int32 chainIndex, vec3 targetPos, float weight = 1.0f);
		static void SetLookAtIKTarget(Scene& scene, uint64 entityId, int32 targetIndex, vec3 targetPos, float weight = 1.0f);
		static void SetFootGroundIKEnabled(Scene& scene, uint64 entityId, bool enabled, float footHeightOffset = 0.05f);

		// SaveGame
		static bool WriteCheckpointSlot(const std::string& slotName, const std::string& checkpointId);
		static bool WriteCheckpointSlot(Scene& scene, entt::entity entity, const std::string& slotName, const std::string& checkpointId);
		static bool LoadCheckpointSlot(const std::string& slotName);
		static bool LoadCheckpointSlot(Scene& scene, entt::entity entity, const std::string& slotName);
		static bool HasSlot(const std::string& slotName);
		static bool DeleteSlot(const std::string& slotName);

		// Navigation & AI
		static void SetNavDestination(int64 entityId, float x, float y, float z);
		static void SetNavDestination(Scene& scene, entt::entity entity, const vec3& dest);
		static void StopNav(int64 entityId);
		static void StopNav(Scene& scene, entt::entity entity);
		static bool HasReachedNavDestination(int64 entityId);
		static bool HasReachedNavDestination(const Scene& scene, entt::entity entity);
		static int64 GetAIDetectedTarget(int64 entityId);
		static int64 GetAIDetectedTarget(const Scene& scene, entt::entity entity);
		static bool HasAILineOfSight(int64 entityId);
		static bool HasAILineOfSight(const Scene& scene, entt::entity entity);

		// AudioMixer
		static void SetBusVolume(const std::string& busName, float volume);
		static float GetBusVolume(const std::string& busName);
		static void SetBusMuted(const std::string& busName, bool muted);
		static void FadeBusVolume(const std::string& busName, float targetVolume, float duration);

		// SceneStreaming
		static void MarkDontDestroyOnLoad(int64 entityId, const std::string& group);
		static void MarkDontDestroyOnLoad(Scene& scene, entt::entity entity, const std::string& group);
		static bool UnloadAdditiveScene(const std::string& subSceneName);
	};
}

namespace Engine::GraphGameplay
{
	FUNCTION(GRAPH_CALLABLE("ActionTimeline.Play"), DISPLAY_NAME("NODE_PLAY_ACTION_TIMELINE"), CATEGORY("Animation"))
	inline bool PlayActionTimeline()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		const auto entity = GraphSystem::GetCurrentEntity();
		if (!scene || !scene->IsActive() || scene->IsPaused() || entity == entt::null) return false;

		auto& world = scene->GetRegistry();
		auto* timeline = TryGetEnabledComponent<ActionTimelineComponent>(world, entity);
		if (!timeline || timeline->timelinePath.empty() || timeline->isPlaying) return false;
		const auto* flag = world.try_get<FlagComponent>(entity);
		if (flag && (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, EEntityFlag::Dead) ||
			HasFlag(flag->effectiveFlags, EEntityFlag::Paused))) return false;

		timeline->Play();
		return true;
	}

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

	FUNCTION(GRAPH_PURE("Input.WasActionPressed"), DISPLAY_NAME("NODE_INPUT_WAS_ACTION_PRESSED"), CATEGORY("Input"),
		GRAPH_PIN_NAME("mapId", "PIN_INPUT_MAP_ID"), GRAPH_PIN_DEFAULT("mapId", "Gameplay"),
		GRAPH_PIN_NAME("actionId", "PIN_INPUT_ACTION_ID"), GRAPH_PIN_DEFAULT("actionId", ""))
		inline bool WasActionPressed(std::string mapId, std::string actionId)
	{
		return InputManager::Get().WasActionPressed(mapId, actionId);
	}

	FUNCTION(GRAPH_PURE("Input.WasActionReleased"), DISPLAY_NAME("NODE_INPUT_WAS_ACTION_RELEASED"), CATEGORY("Input"),
		GRAPH_PIN_NAME("mapId", "PIN_INPUT_MAP_ID"), GRAPH_PIN_DEFAULT("mapId", "Gameplay"),
		GRAPH_PIN_NAME("actionId", "PIN_INPUT_ACTION_ID"), GRAPH_PIN_DEFAULT("actionId", ""))
		inline bool WasActionReleased(std::string mapId, std::string actionId)
	{
		return InputManager::Get().WasActionReleased(mapId, actionId);
	}

	FUNCTION(GRAPH_PURE("Input.IsActionHeld"), DISPLAY_NAME("NODE_INPUT_IS_ACTION_HELD"), CATEGORY("Input"),
		GRAPH_PIN_NAME("mapId", "PIN_INPUT_MAP_ID"), GRAPH_PIN_DEFAULT("mapId", "Gameplay"),
		GRAPH_PIN_NAME("actionId", "PIN_INPUT_ACTION_ID"), GRAPH_PIN_DEFAULT("actionId", ""))
		inline bool IsActionHeld(std::string mapId, std::string actionId)
	{
		return InputManager::Get().IsActionHeld(mapId, actionId);
	}

	FUNCTION(GRAPH_PURE("Input.GetActionAxis"), DISPLAY_NAME("NODE_INPUT_GET_ACTION_AXIS"), CATEGORY("Input"),
		GRAPH_PIN_NAME("mapId", "PIN_INPUT_MAP_ID"), GRAPH_PIN_DEFAULT("mapId", "Gameplay"),
		GRAPH_PIN_NAME("positiveActionId", "PIN_INPUT_POSITIVE_ACTION_ID"),
		GRAPH_PIN_DEFAULT("positiveActionId", ""),
		GRAPH_PIN_NAME("negativeActionId", "PIN_INPUT_NEGATIVE_ACTION_ID"),
		GRAPH_PIN_DEFAULT("negativeActionId", ""))
		inline float GetActionAxis(std::string mapId, std::string positiveActionId, std::string negativeActionId)
	{
		const auto& input = InputManager::Get();
		const bool positiveHeld = input.IsActionHeld(mapId, positiveActionId);
		const bool negativeHeld = input.IsActionHeld(mapId, negativeActionId);
		return static_cast<float>(positiveHeld) - static_cast<float>(negativeHeld);
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
		auto* rb = TryGetEnabledComponent<RigidBodyComponent>(reg, entity);
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
		auto* rb = TryGetEnabledComponent<RigidBodyComponent>(reg, entity);
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
		auto* rb = TryGetEnabledComponent<RigidBodyComponent>(reg, entity);
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
		const auto* rb = TryGetEnabledComponent<RigidBodyComponent>(reg, entity);
		if (!rb) return { 0.0f, 0.0f, 0.0f };

		return { rb->LinearVelocity.x, rb->LinearVelocity.y, rb->LinearVelocity.z };
	}

	// ── 오디오 노드 (Audio) ──
	FUNCTION(GRAPH_CALLABLE("Audio.PlayOneShot"), DISPLAY_NAME("Play One-Shot Audio"), CATEGORY("Audio"),
		GRAPH_PIN_DEFAULT("clipPath", ""), GRAPH_PIN_DEFAULT("volume", 1.0f))
		inline bool PlayOneShotAudio(std::string clipPath, float volume)
	{
		if (clipPath.empty() || !AudioManager::IsCreated() || !ResourceManager::IsCreated()) return false;

		const auto handle = ResourceManager::Get().LoadFile(clipPath);
		if (!handle.IsValid()) return false;

		auto* clip = dynamic_cast<AudioClip*>(ResourceManager::Get().GetResource(handle));
		if (!clip) return false;

		auto* sound = clip->GetFMODSound();
		if (!sound) return false;

		return AudioManager::Get().PlayOneShot(sound, std::clamp(volume, 0.0f, 1.0f)) == EResult::Success;
	}

	FUNCTION(GRAPH_CALLABLE("Audio.StopAll"), DISPLAY_NAME("Stop All Audio"), CATEGORY("Audio"))
		inline void StopAllAudio()
	{
		if (!AudioManager::IsCreated()) return;
		AudioManager::Get().StopAllSounds();
	}

	FUNCTION(GRAPH_CALLABLE("Audio.SetMasterVolume"), DISPLAY_NAME("Set Master Volume"), CATEGORY("Audio"),
		GRAPH_PIN_DEFAULT("volume", 1.0f))
		inline void SetMasterVolume(float volume)
	{
		if (!AudioManager::IsCreated()) return;
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
		auto* anim = TryGetEnabledComponent<SpriteAnimationComponent>(scene->GetRegistry(), entity);
		if (anim) anim->Play();
	}
	FUNCTION(GRAPH_CALLABLE("Sprite.PauseAnimation"), DISPLAY_NAME("Pause Sprite Animation"), CATEGORY("Sprite"))
		inline void PauseSpriteAnimation()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = TryGetEnabledComponent<SpriteAnimationComponent>(scene->GetRegistry(), entity);
		if (anim) anim->Pause();
	}
	FUNCTION(GRAPH_CALLABLE("Sprite.StopAnimation"), DISPLAY_NAME("Stop Sprite Animation"), CATEGORY("Sprite"))
		inline void StopSpriteAnimation()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = TryGetEnabledComponent<SpriteAnimationComponent>(scene->GetRegistry(), entity);
		if (anim) anim->Stop();
	}
	FUNCTION(GRAPH_CALLABLE("Sprite.SetFrame"), DISPLAY_NAME("Set Sprite Frame"), CATEGORY("Sprite"),
		GRAPH_PIN_DEFAULT("frameIndex", 0))
		inline void SetSpriteAnimationFrame(int frameIndex)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = TryGetEnabledComponent<SpriteAnimationComponent>(scene->GetRegistry(), entity);
		if (anim) anim->SetFrame(frameIndex);
	}
	FUNCTION(GRAPH_CALLABLE("Sprite.SetPlayRate"), DISPLAY_NAME("Set Sprite Play Rate"), CATEGORY("Sprite"),
		GRAPH_PIN_DEFAULT("rate", 1.0f))
		inline void SetSpritePlayRate(float rate)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = TryGetEnabledComponent<SpriteAnimationComponent>(scene->GetRegistry(), entity);
		if (anim) anim->playRate = rate;
	}
	FUNCTION(GRAPH_PURE("Sprite.IsPlaying"), DISPLAY_NAME("Is Sprite Animation Playing"), CATEGORY("Sprite"))
		inline bool IsSpriteAnimationPlaying()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return false;
		auto* anim = TryGetEnabledComponent<SpriteAnimationComponent>(scene->GetRegistry(), entity);
		return anim ? anim->isPlaying : false;
	}
	FUNCTION(GRAPH_PURE("Sprite.GetCurrentFrame"), DISPLAY_NAME("Get Current Sprite Frame"), CATEGORY("Sprite"))
		inline int GetCurrentSpriteFrame()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return 0;
		auto* anim = TryGetEnabledComponent<SpriteAnimationComponent>(scene->GetRegistry(), entity);
		return anim ? anim->currentFrameIndex : 0;
	}

	// ── 3D 애니메이터 노드 (3D Skeletal Animation) ──
	namespace Detail
	{
		// 그래프가 붙은 엔티티부터 자식 계층까지 Animator를 찾는다.
		// 캐릭터 루트(이동·그래프)의 자식으로 모델을 둔 구성에서도 모델 루트의 Animator를 제어할 수 있다.
		inline AnimatorComponent* FindGraphAnimator()
		{
			Scene* scene = GraphSystem::GetCurrentScene();
			const entt::entity entity = GraphSystem::GetCurrentEntity();
			if (!scene || entity == entt::null) return nullptr;
			auto& world = scene->GetRegistry();
			const entt::entity animatorEntity = FindAnimatorEntityInHierarchy(world, entity);
			return animatorEntity != entt::null ? &world.get<AnimatorComponent>(animatorEntity) : nullptr;
		}
	}

	FUNCTION(GRAPH_CALLABLE("Animator.Play"), DISPLAY_NAME("Play 3D Animation"), CATEGORY("Animation"))
		inline void Play3DAnimation()
	{
		if (auto* anim = Detail::FindGraphAnimator()) anim->Play();
	}
	FUNCTION(GRAPH_CALLABLE("Animator.Pause"), DISPLAY_NAME("Pause 3D Animation"), CATEGORY("Animation"))
		inline void Pause3DAnimation()
	{
		if (auto* anim = Detail::FindGraphAnimator()) anim->Pause();
	}
	FUNCTION(GRAPH_CALLABLE("Animator.Stop"), DISPLAY_NAME("Stop 3D Animation"), CATEGORY("Animation"))
		inline void Stop3DAnimation()
	{
		if (auto* anim = Detail::FindGraphAnimator()) anim->Stop();
	}
	FUNCTION(GRAPH_CALLABLE("Animator.SetPlayRate"), DISPLAY_NAME("Set 3D Animation Play Rate"), CATEGORY("Animation"),
		GRAPH_PIN_DEFAULT("rate", 1.0f))
		inline void Set3DAnimationPlayRate(float rate)
	{
		if (auto* anim = Detail::FindGraphAnimator()) anim->playRate = rate;
	}
	FUNCTION(GRAPH_CALLABLE("Animator.SetTime"), DISPLAY_NAME("Set 3D Animation Time"), CATEGORY("Animation"),
		GRAPH_PIN_DEFAULT("time", 0.0f))
		inline void Set3DAnimationTime(float time)
	{
		if (auto* anim = Detail::FindGraphAnimator()) anim->SetTime(time);
	}
	FUNCTION(GRAPH_PURE("Animator.IsPlaying"), DISPLAY_NAME("Is 3D Animation Playing"), CATEGORY("Animation"))
		inline bool Is3DAnimationPlaying()
	{
		const auto* anim = Detail::FindGraphAnimator();
		return anim ? anim->isPlaying : false;
	}
	FUNCTION(GRAPH_PURE("Animator.GetCurrentTime"), DISPLAY_NAME("Get 3D Animation Time"), CATEGORY("Animation"))
		inline float Get3DAnimationCurrentTime()
	{
		const auto* anim = Detail::FindGraphAnimator();
		return anim ? anim->currentTime : 0.0f;
	}

	// ── 3D Animator 파라미터 제어 노드 ──
	FUNCTION(GRAPH_CALLABLE("Animator.SetFloat"), DISPLAY_NAME("NODE_ANIMATOR_SET_FLOAT"), CATEGORY("Animation"),
		GRAPH_PIN_DEFAULT("value", 0.0f))
		inline void SetAnimatorFloat(std::string name, float value)
	{
		if (auto* anim = Detail::FindGraphAnimator()) anim->SetFloat(name, value);
	}
	FUNCTION(GRAPH_CALLABLE("Animator.SetInt"), DISPLAY_NAME("NODE_ANIMATOR_SET_INT"), CATEGORY("Animation"),
		GRAPH_PIN_DEFAULT("value", 0))
		inline void SetAnimatorInt(std::string name, std::int64_t value)
	{
		if (auto* anim = Detail::FindGraphAnimator()) anim->SetInt(name, static_cast<int32>(value));
	}
	FUNCTION(GRAPH_CALLABLE("Animator.SetBool"), DISPLAY_NAME("NODE_ANIMATOR_SET_BOOL"), CATEGORY("Animation"),
		GRAPH_PIN_DEFAULT("value", false))
		inline void SetAnimatorBool(std::string name, bool value)
	{
		if (auto* anim = Detail::FindGraphAnimator()) anim->SetBool(name, value);
	}
	FUNCTION(GRAPH_CALLABLE("Animator.SetTrigger"), DISPLAY_NAME("NODE_ANIMATOR_SET_TRIGGER"), CATEGORY("Animation"))
		inline void SetAnimatorTrigger(std::string name)
	{
		if (auto* anim = Detail::FindGraphAnimator()) anim->SetTrigger(name);
	}
	FUNCTION(GRAPH_PURE("Animator.GetFloat"), DISPLAY_NAME("NODE_ANIMATOR_GET_FLOAT"), CATEGORY("Animation"))
		inline float GetAnimatorFloat(std::string name)
	{
		const auto* anim = Detail::FindGraphAnimator();
		return anim ? anim->runtimeState.GetFloat(name) : 0.0f;
	}
	FUNCTION(GRAPH_PURE("Animator.GetBool"), DISPLAY_NAME("NODE_ANIMATOR_GET_BOOL"), CATEGORY("Animation"))
		inline bool GetAnimatorBool(std::string name)
	{
		const auto* anim = Detail::FindGraphAnimator();
		return anim ? anim->runtimeState.GetBool(name) : false;
	}

	// ── 3D Animator 레이어 제어 노드 ──
	FUNCTION(GRAPH_CALLABLE("Animator.SetLayerWeight"), DISPLAY_NAME("NODE_ANIMATOR_SET_LAYER_WEIGHT"), CATEGORY("Animation"),
		GRAPH_PIN_DEFAULT("weight", 1.0f))
		inline void SetAnimatorLayerWeight(std::string layerName, float weight)
	{
		if (auto* anim = Detail::FindGraphAnimator()) anim->SetLayerWeight(layerName, weight);
	}
	FUNCTION(GRAPH_PURE("Animator.GetLayerWeight"), DISPLAY_NAME("NODE_ANIMATOR_GET_LAYER_WEIGHT"), CATEGORY("Animation"))
		inline float GetAnimatorLayerWeight(std::string layerName)
	{
		const auto* anim = Detail::FindGraphAnimator();
		return anim ? anim->GetLayerWeight(layerName) : 0.0f;
	}
	FUNCTION(GRAPH_CALLABLE("Animator.StopLayer"), DISPLAY_NAME("NODE_ANIMATOR_STOP_LAYER"), CATEGORY("Animation"))
		inline void StopAnimatorLayer(std::string layerName)
	{
		if (auto* anim = Detail::FindGraphAnimator()) anim->StopLayer(layerName);
	}

	// ── 캐릭터 컨트롤러 노드 (CharacterController) ──
	FUNCTION(GRAPH_CALLABLE("CharacterController.Move"), DISPLAY_NAME("Character Move"), CATEGORY("CharacterController"),
		GRAPH_PIN_DEFAULT("x", 0.0f), GRAPH_PIN_DEFAULT("y", 0.0f), GRAPH_PIN_DEFAULT("z", 0.0f), GRAPH_PIN_DEFAULT("run", false))
		inline void CharacterMove(float x, float y, float z, bool run)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto& reg = scene->GetRegistry();
		auto* kcc = TryGetEnabledComponent<CharacterControllerComponent>(reg, entity);
		if (!kcc) return;
		kcc->Move(glm::vec3(x, y, z), run);
	}
	FUNCTION(GRAPH_CALLABLE("CharacterController.Jump"), DISPLAY_NAME("Character Jump"), CATEGORY("CharacterController"))
		inline void CharacterJump()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto& reg = scene->GetRegistry();
		auto* kcc = TryGetEnabledComponent<CharacterControllerComponent>(reg, entity);
		if (!kcc) return;
		kcc->Jump();
	}
	FUNCTION(GRAPH_PURE("CharacterController.IsGrounded"), DISPLAY_NAME("Is Grounded"), CATEGORY("CharacterController"))
		inline bool CharacterIsGrounded()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return false;
		auto& reg = scene->GetRegistry();
		const auto* kcc = TryGetEnabledComponent<CharacterControllerComponent>(reg, entity);
		return kcc ? kcc->IsGrounded : false;
	}
	FUNCTION(GRAPH_PURE("CharacterController.GetVelocity"), DISPLAY_NAME("Get Character Velocity"), CATEGORY("CharacterController"))
		inline std::array<float, 3> CharacterGetVelocity()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return { 0.0f, 0.0f, 0.0f };
		auto& reg = scene->GetRegistry();
		const auto* kcc = TryGetEnabledComponent<CharacterControllerComponent>(reg, entity);
		if (!kcc) return { 0.0f, 0.0f, 0.0f };
		return { kcc->Velocity.x, kcc->Velocity.y, kcc->Velocity.z };
	}
	FUNCTION(GRAPH_CALLABLE("CharacterController.SetVelocity"), DISPLAY_NAME("Set Character Velocity"), CATEGORY("CharacterController"),
		GRAPH_PIN_DEFAULT("x", 0.0f), GRAPH_PIN_DEFAULT("y", 0.0f), GRAPH_PIN_DEFAULT("z", 0.0f))
		inline void CharacterSetVelocity(float x, float y, float z)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto& reg = scene->GetRegistry();
		auto* kcc = TryGetEnabledComponent<CharacterControllerComponent>(reg, entity);
		if (!kcc) return;
		kcc->SetVelocity(glm::vec3(x, y, z));
	}
	FUNCTION(GRAPH_PURE("CharacterController.GetGroundNormal"), DISPLAY_NAME("Get Ground Normal"), CATEGORY("CharacterController"))
		inline std::array<float, 3> CharacterGetGroundNormal()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return { 0.0f, 1.0f, 0.0f };
		auto& reg = scene->GetRegistry();
		const auto* kcc = TryGetEnabledComponent<CharacterControllerComponent>(reg, entity);
		if (!kcc) return { 0.0f, 1.0f, 0.0f };
		return { kcc->GroundNormal.x, kcc->GroundNormal.y, kcc->GroundNormal.z };
	}
	FUNCTION(GRAPH_PURE("CharacterController.GetGroundAngle"), DISPLAY_NAME("Get Ground Angle"), CATEGORY("CharacterController"))
		inline float CharacterGetGroundAngle()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return 0.0f;
		auto& reg = scene->GetRegistry();
		const auto* kcc = TryGetEnabledComponent<CharacterControllerComponent>(reg, entity);
		return kcc ? kcc->GroundAngle : 0.0f;
	}

	inline ThirdPersonControllerComponent* FindGraphMoveController()
	{
		auto* scene = GraphSystem::GetCurrentScene();
		const auto entity = GraphSystem::GetCurrentEntity();
		if (!scene) return nullptr;
		auto& world = scene->GetRegistry();
		if (!world.valid(entity) || !TryGetEnabledComponent<CharacterControllerComponent>(world, entity)) return nullptr;

		auto* ctrl = TryGetEnabledComponent<ThirdPersonControllerComponent>(world, entity);
		return ctrl && ctrl->InputEnabled && ctrl->UseGraphMoveInput ? ctrl : nullptr;
	}

	FUNCTION(GRAPH_CALLABLE("ThirdPersonController.SetMoveInput"), DISPLAY_NAME("NODE_THIRD_PERSON_SET_MOVE_INPUT"),
		CATEGORY("CATEGORY_PHYSICS"), GRAPH_PIN_DEFAULT("x", 0.0f), GRAPH_PIN_DEFAULT("z", 0.0f),
		GRAPH_PIN_DEFAULT("run", false))
		inline void SetThirdPersonMoveInput(float x, float z, bool run)
	{
		if (auto* ctrl = FindGraphMoveController())
		{
			ctrl->GraphMoveAxis = vec2(x, z);
			ctrl->GraphSprint = run;
		}
	}

	FUNCTION(GRAPH_CALLABLE("ThirdPersonController.RequestJump"), DISPLAY_NAME("NODE_THIRD_PERSON_REQUEST_JUMP"),
		CATEGORY("CATEGORY_PHYSICS"))
		inline void RequestThirdPersonJump()
	{
		if (auto* ctrl = FindGraphMoveController())
			ctrl->GraphJumpRequested = true;
	}

	// ── 스프링 암 노드 (SpringArm) ──
	FUNCTION(GRAPH_CALLABLE("SpringArm.AddPitch"), DISPLAY_NAME("SpringArm Add Pitch"), CATEGORY("Camera"),
		GRAPH_PIN_DEFAULT("delta", 0.0f))
		inline void SpringArmAddPitch(float delta)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;

		auto& reg = scene->GetRegistry();
		auto* arm = TryGetEnabledComponent<SpringArmComponent>(reg, entity);
		if (arm) arm->AddPitch(delta);
	}

	FUNCTION(GRAPH_CALLABLE("SpringArm.AddYaw"), DISPLAY_NAME("SpringArm Add Yaw"), CATEGORY("Camera"),
		GRAPH_PIN_DEFAULT("delta", 0.0f))
		inline void SpringArmAddYaw(float delta)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;

		auto& reg = scene->GetRegistry();
		auto* arm = TryGetEnabledComponent<SpringArmComponent>(reg, entity);
		if (arm) arm->AddYaw(delta);
	}

	FUNCTION(GRAPH_CALLABLE("SpringArm.SetArmLength"), DISPLAY_NAME("Set Arm Length"), CATEGORY("Camera"),
		GRAPH_PIN_DEFAULT("length", 4.0f))
		inline void SpringArmSetArmLength(float length)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;

		auto& reg = scene->GetRegistry();
		auto* arm = TryGetEnabledComponent<SpringArmComponent>(reg, entity);
		if (arm) arm->TargetArmLength = length;
	}

	FUNCTION(GRAPH_PURE("SpringArm.GetArmLength"), DISPLAY_NAME("Get Arm Length"), CATEGORY("Camera"))
		inline float SpringArmGetArmLength()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return 4.0f;

		auto& reg = scene->GetRegistry();
		const auto* arm = TryGetEnabledComponent<SpringArmComponent>(reg, entity);
		return arm ? arm->CurrentArmLength : 4.0f;
	}

	// ── VFX 노드 (CompositeVFX) ──
	FUNCTION(GRAPH_CALLABLE("VFX.SpawnCompositeVFX"), DISPLAY_NAME("Spawn Composite VFX"), CATEGORY("VFX"),
		GRAPH_PIN_DEFAULT("vfxPath", ""),
		GRAPH_PIN_DEFAULT("x", 0.0f), GRAPH_PIN_DEFAULT("y", 0.0f), GRAPH_PIN_DEFAULT("z", 0.0f),
		GRAPH_PIN_DEFAULT("playRate", 1.0f))
		inline std::int64_t SpawnCompositeVFX(std::string vfxPath, float x, float y, float z, float playRate)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene || vfxPath.empty() || !ResourceManager::IsCreated()) return -1;

		const auto handle = ResourceManager::Get().LoadFile(vfxPath);
		if (!handle.IsValid()) return -1;

		ResourceHandle<CompositeEffect> effectHandle(handle);
		if (!effectHandle.Get()) return -1;

		Entity& newEntity = scene->CreateEntity();
		const entt::entity vfxEntity = newEntity.GetEntityHandle();
		auto& world = scene->GetRegistry();

		world.get_or_emplace<NameComponent>(vfxEntity).name = L"Spawned_CompositeVFX";

		auto& trans = world.get_or_emplace<TransformComponent>(vfxEntity);
		(void)trans.SetLocalPosition(glm::vec3(x, y, z));

		auto& worldTrans = world.get_or_emplace<WorldTransformComponent>(vfxEntity);
		TransformSystem::UpdateRootWorld(trans, worldTrans);

		auto& flag = world.get_or_emplace<FlagComponent>(vfxEntity);
		flag.flags = EEntityFlag::Active | EEntityFlag::Visible;
		flag.effectiveFlags = flag.flags;

		auto& vfxComp = world.get_or_emplace<CompositeVFXComponent>(vfxEntity);
		vfxComp.effectHandle = effectHandle;
		vfxComp.isPlaying = true;
		vfxComp.isLooping = false;
		vfxComp.playOnce = true;
		vfxComp.playRate = playRate > 0.0f ? playRate : 1.0f;
		vfxComp.Restart();

		world.get_or_emplace<ActionVFXLifecycleComponent>(vfxEntity).autoDestroy = true;
		return static_cast<std::int64_t>(static_cast<std::uint32_t>(vfxEntity));
	}

	// ── 게임플레이 체력 노드 (Gameplay) ──
	namespace Detail
	{
		inline entt::entity ResolveGameplayTargetEntity(const entt::registry& reg, std::int64_t targetEntityId)
		{
			if (targetEntityId < 0)
			{
				const entt::entity current = GraphSystem::GetCurrentEntity();
				return (current != entt::null && reg.valid(current)) ? current : entt::null;
			}
			if (targetEntityId > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()))
				return entt::null;
			const entt::entity target = static_cast<entt::entity>(static_cast<std::uint32_t>(targetEntityId));
			return reg.valid(target) ? target : entt::null;
		}
	}

	FUNCTION(GRAPH_CALLABLE("Gameplay.ApplyDamage"), DISPLAY_NAME("Apply Damage"), CATEGORY("Gameplay"),
		GRAPH_PIN_DEFAULT("amount", 0.0f), GRAPH_PIN_DEFAULT("targetEntityId", -1))
		inline float ApplyDamage(float amount, std::int64_t targetEntityId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return 0.0f;

		auto& reg = scene->GetRegistry();
		const entt::entity target = Detail::ResolveGameplayTargetEntity(reg, targetEntityId);
		if (target == entt::null) return 0.0f;

		auto* health = TryGetEnabledComponent<HealthComponent>(reg, target);
		if (!health) return 0.0f;
		return health->ApplyDamage(amount);
	}

	FUNCTION(GRAPH_CALLABLE("Gameplay.Heal"), DISPLAY_NAME("Heal"), CATEGORY("Gameplay"),
		GRAPH_PIN_DEFAULT("amount", 0.0f), GRAPH_PIN_DEFAULT("targetEntityId", -1))
		inline float Heal(float amount, std::int64_t targetEntityId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return 0.0f;

		auto& reg = scene->GetRegistry();
		const entt::entity target = Detail::ResolveGameplayTargetEntity(reg, targetEntityId);
		if (target == entt::null) return 0.0f;

		auto* health = TryGetEnabledComponent<HealthComponent>(reg, target);
		if (!health) return 0.0f;
		return health->Heal(amount);
	}

	FUNCTION(GRAPH_PURE("Gameplay.GetHealth"), DISPLAY_NAME("Get Health"), CATEGORY("Gameplay"),
		GRAPH_PIN_DEFAULT("targetEntityId", -1))
		inline float GetHealth(std::int64_t targetEntityId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return 0.0f;

		const auto& reg = scene->GetRegistry();
		const entt::entity target = Detail::ResolveGameplayTargetEntity(reg, targetEntityId);
		if (target == entt::null) return 0.0f;

		const auto* health = TryGetEnabledComponent<HealthComponent>(reg, target);
		return health ? health->currentHealth : 0.0f;
	}

	// ── Animation IK 제어 노드 ──
	FUNCTION(GRAPH_CALLABLE("AnimationIK.SetEnabled"), DISPLAY_NAME("Set Animation IK Enabled"), CATEGORY("Animation/IK"),
		GRAPH_PIN_DEFAULT("entityId", 0), GRAPH_PIN_DEFAULT("enabled", true))
	inline void SetAnimationIKEnabled(std::int64_t entityId, bool enabled)
	{
		if (entityId >= 0) GraphGameplayLibrary::SetAnimationIKEnabled(static_cast<uint64>(entityId), enabled);
	}

	FUNCTION(GRAPH_CALLABLE("AnimationIK.SetTwoBoneIKTarget"), DISPLAY_NAME("Set Two-Bone IK Target"), CATEGORY("Animation/IK"),
		GRAPH_PIN_DEFAULT("entityId", 0), GRAPH_PIN_DEFAULT("chainIndex", 0),
		GRAPH_PIN_DEFAULT("targetX", 0.0f), GRAPH_PIN_DEFAULT("targetY", 0.0f), GRAPH_PIN_DEFAULT("targetZ", 0.0f),
		GRAPH_PIN_DEFAULT("weight", 1.0f))
	inline void SetTwoBoneIKTarget(std::int64_t entityId, std::int64_t chainIndex, float targetX, float targetY, float targetZ, float weight = 1.0f)
	{
		if (entityId >= 0) GraphGameplayLibrary::SetTwoBoneIKTarget(static_cast<uint64>(entityId), static_cast<int32>(chainIndex), vec3(targetX, targetY, targetZ), weight);
	}

	FUNCTION(GRAPH_CALLABLE("AnimationIK.SetLookAtIKTarget"), DISPLAY_NAME("Set Look-At IK Target"), CATEGORY("Animation/IK"),
		GRAPH_PIN_DEFAULT("entityId", 0), GRAPH_PIN_DEFAULT("targetIndex", 0),
		GRAPH_PIN_DEFAULT("targetX", 0.0f), GRAPH_PIN_DEFAULT("targetY", 0.0f), GRAPH_PIN_DEFAULT("targetZ", 0.0f),
		GRAPH_PIN_DEFAULT("weight", 1.0f))
	inline void SetLookAtIKTarget(std::int64_t entityId, std::int64_t targetIndex, float targetX, float targetY, float targetZ, float weight = 1.0f)
	{
		if (entityId >= 0) GraphGameplayLibrary::SetLookAtIKTarget(static_cast<uint64>(entityId), static_cast<int32>(targetIndex), vec3(targetX, targetY, targetZ), weight);
	}

	FUNCTION(GRAPH_CALLABLE("AnimationIK.SetFootGroundIKEnabled"), DISPLAY_NAME("Set Foot Ground IK Enabled"), CATEGORY("Animation/IK"),
		GRAPH_PIN_DEFAULT("entityId", 0), GRAPH_PIN_DEFAULT("enabled", true), GRAPH_PIN_DEFAULT("footHeightOffset", 0.05f))
	inline void SetFootGroundIKEnabled(std::int64_t entityId, bool enabled, float footHeightOffset = 0.05f)
	{
		if (entityId >= 0) GraphGameplayLibrary::SetFootGroundIKEnabled(static_cast<uint64>(entityId), enabled, footHeightOffset);
	}

	// ── 세이브 게임 노드 (SaveGame) ──
	FUNCTION(GRAPH_CALLABLE("Save.WriteCheckpointSlot"), DISPLAY_NAME("Write Checkpoint Slot"), CATEGORY("SaveGame"),
		GRAPH_PIN_DEFAULT("slotName", ""), GRAPH_PIN_DEFAULT("checkpointId", ""))
	inline bool WriteCheckpointSlot(std::string slotName, std::string checkpointId)
	{
		return GraphGameplayLibrary::WriteCheckpointSlot(slotName, checkpointId);
	}

	FUNCTION(GRAPH_CALLABLE("Save.LoadCheckpointSlot"), DISPLAY_NAME("Load Checkpoint Slot"), CATEGORY("SaveGame"),
		GRAPH_PIN_DEFAULT("slotName", ""))
	inline bool LoadCheckpointSlot(std::string slotName)
	{
		return GraphGameplayLibrary::LoadCheckpointSlot(slotName);
	}

	FUNCTION(GRAPH_PURE("Save.HasSlot"), DISPLAY_NAME("Has Slot"), CATEGORY("SaveGame"),
		GRAPH_PIN_DEFAULT("slotName", ""))
	inline bool HasSlot(std::string slotName)
	{
		return GraphGameplayLibrary::HasSlot(slotName);
	}

	FUNCTION(GRAPH_CALLABLE("Save.DeleteSlot"), DISPLAY_NAME("Delete Slot"), CATEGORY("SaveGame"),
		GRAPH_PIN_DEFAULT("slotName", ""))
	inline bool DeleteSlot(std::string slotName)
	{
		return GraphGameplayLibrary::DeleteSlot(slotName);
	}

	// ── 내비게이션 & AI 노드 (Navigation / AI) ──
	FUNCTION(GRAPH_CALLABLE("Nav.SetDestination"), DISPLAY_NAME("Set Destination"), CATEGORY("Navigation"),
		GRAPH_PIN_DEFAULT("entityId", -1), GRAPH_PIN_DEFAULT("x", 0.0f), GRAPH_PIN_DEFAULT("y", 0.0f), GRAPH_PIN_DEFAULT("z", 0.0f))
	inline void SetDestination(std::int64_t entityId, float x, float y, float z)
	{
		GraphGameplayLibrary::SetNavDestination(entityId, x, y, z);
	}

	FUNCTION(GRAPH_CALLABLE("Nav.Stop"), DISPLAY_NAME("Stop Navigation"), CATEGORY("Navigation"),
		GRAPH_PIN_DEFAULT("entityId", -1))
	inline void StopNavigation(std::int64_t entityId)
	{
		GraphGameplayLibrary::StopNav(entityId);
	}

	FUNCTION(GRAPH_PURE("Nav.HasReachedDestination"), DISPLAY_NAME("Has Reached Destination"), CATEGORY("Navigation"),
		GRAPH_PIN_DEFAULT("entityId", -1))
	inline bool HasReachedDestination(std::int64_t entityId)
	{
		return GraphGameplayLibrary::HasReachedNavDestination(entityId);
	}

	FUNCTION(GRAPH_PURE("AI.GetDetectedTarget"), DISPLAY_NAME("Get Detected Target"), CATEGORY("AI"),
		GRAPH_PIN_DEFAULT("entityId", -1))
	inline std::int64_t GetDetectedTarget(std::int64_t entityId)
	{
		return GraphGameplayLibrary::GetAIDetectedTarget(entityId);
	}

	FUNCTION(GRAPH_PURE("AI.HasLineOfSight"), DISPLAY_NAME("Has Line Of Sight"), CATEGORY("AI"),
		GRAPH_PIN_DEFAULT("entityId", -1))
	inline bool HasLineOfSight(std::int64_t entityId)
	{
		return GraphGameplayLibrary::HasAILineOfSight(entityId);
	}

	// ── 오디오 믹서 노드 (Audio) ──
	FUNCTION(GRAPH_CALLABLE("Audio.SetBusVolume"), DISPLAY_NAME("Set Bus Volume"), CATEGORY("Audio"),
		GRAPH_PIN_DEFAULT("busName", "Master"), GRAPH_PIN_DEFAULT("volume", 1.0f))
	inline void SetBusVolume(std::string busName, float volume)
	{
		GraphGameplayLibrary::SetBusVolume(busName, volume);
	}

	FUNCTION(GRAPH_PURE("Audio.GetBusVolume"), DISPLAY_NAME("Get Bus Volume"), CATEGORY("Audio"),
		GRAPH_PIN_DEFAULT("busName", "Master"))
	inline float GetBusVolume(std::string busName)
	{
		return GraphGameplayLibrary::GetBusVolume(busName);
	}

	FUNCTION(GRAPH_CALLABLE("Audio.SetBusMuted"), DISPLAY_NAME("Set Bus Muted"), CATEGORY("Audio"),
		GRAPH_PIN_DEFAULT("busName", "Master"), GRAPH_PIN_DEFAULT("muted", false))
	inline void SetBusMuted(std::string busName, bool muted)
	{
		GraphGameplayLibrary::SetBusMuted(busName, muted);
	}

	FUNCTION(GRAPH_CALLABLE("Audio.FadeBusVolume"), DISPLAY_NAME("Fade Bus Volume"), CATEGORY("Audio"),
		GRAPH_PIN_DEFAULT("busName", "Master"), GRAPH_PIN_DEFAULT("targetVolume", 1.0f), GRAPH_PIN_DEFAULT("duration", 1.0f))
	inline void FadeBusVolume(std::string busName, float targetVolume, float duration)
	{
		GraphGameplayLibrary::FadeBusVolume(busName, targetVolume, duration);
	}

	// ── 씬 스트리밍 및 영속성 노드 (Scene) ──
	FUNCTION(GRAPH_CALLABLE("Scene.MarkDontDestroyOnLoad"), DISPLAY_NAME("Mark Don't Destroy On Load"), CATEGORY("Scene"),
		GRAPH_PIN_DEFAULT("entityId", -1), GRAPH_PIN_DEFAULT("group", "Global"))
	inline void MarkDontDestroyOnLoad(std::int64_t entityId, std::string group)
	{
		GraphGameplayLibrary::MarkDontDestroyOnLoad(entityId, group);
	}

	FUNCTION(GRAPH_CALLABLE("Scene.UnloadAdditive"), DISPLAY_NAME("Unload Additive Scene"), CATEGORY("Scene"),
		GRAPH_PIN_DEFAULT("subSceneName", ""))
	inline bool UnloadAdditive(std::string subSceneName)
	{
		return GraphGameplayLibrary::UnloadAdditiveScene(subSceneName);
	}
}
