#pragma once

#include "GraphAnnotations.h"
#include "GraphSystem.h"
#include "RigidBodyComponent.h"
#include "AudioManager.h"
#include "InputManager.h"
#include "PhysicsSystem.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include "Logger.h"
#include "SpriteAnimationComponent.h"
#include "CharacterControllerComponent.h"


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
	FUNCTION(GRAPH_CALLABLE("Sprite.StopAnimation"), DISPLAY_NAME("Stop Sprite Animation"), CATEGORY("Sprite"))
		inline void StopSpriteAnimation()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<SpriteAnimationComponent>(entity);
		if (anim) anim->Stop();
	}
	FUNCTION(GRAPH_CALLABLE("Sprite.SetFrame"), DISPLAY_NAME("Set Sprite Frame"), CATEGORY("Sprite"),
		GRAPH_PIN_DEFAULT("frameIndex", 0))
		inline void SetSpriteAnimationFrame(int frameIndex)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<SpriteAnimationComponent>(entity);
		if (anim) anim->SetFrame(frameIndex);
	}
	FUNCTION(GRAPH_CALLABLE("Sprite.SetPlayRate"), DISPLAY_NAME("Set Sprite Play Rate"), CATEGORY("Sprite"),
		GRAPH_PIN_DEFAULT("rate", 1.0f))
		inline void SetSpritePlayRate(float rate)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<SpriteAnimationComponent>(entity);
		if (anim) anim->playRate = rate;
	}
	FUNCTION(GRAPH_PURE("Sprite.IsPlaying"), DISPLAY_NAME("Is Sprite Animation Playing"), CATEGORY("Sprite"))
		inline bool IsSpriteAnimationPlaying()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return false;
		auto* anim = scene->GetRegistry().try_get<SpriteAnimationComponent>(entity);
		return anim ? anim->isPlaying : false;
	}
	FUNCTION(GRAPH_PURE("Sprite.GetCurrentFrame"), DISPLAY_NAME("Get Current Sprite Frame"), CATEGORY("Sprite"))
		inline int GetCurrentSpriteFrame()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return 0;
		auto* anim = scene->GetRegistry().try_get<SpriteAnimationComponent>(entity);
		return anim ? anim->currentFrameIndex : 0;
	}

	// ── 3D 애니메이터 노드 (3D Skeletal Animation) ──
	FUNCTION(GRAPH_CALLABLE("Animator.Play"), DISPLAY_NAME("Play 3D Animation"), CATEGORY("Animation"))
		inline void Play3DAnimation()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		if (anim) anim->Play();
	}
	FUNCTION(GRAPH_CALLABLE("Animator.Pause"), DISPLAY_NAME("Pause 3D Animation"), CATEGORY("Animation"))
		inline void Pause3DAnimation()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		if (anim) anim->Pause();
	}
	FUNCTION(GRAPH_CALLABLE("Animator.Stop"), DISPLAY_NAME("Stop 3D Animation"), CATEGORY("Animation"))
		inline void Stop3DAnimation()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		if (anim) anim->Stop();
	}
	FUNCTION(GRAPH_CALLABLE("Animator.SetPlayRate"), DISPLAY_NAME("Set 3D Animation Play Rate"), CATEGORY("Animation"),
		GRAPH_PIN_DEFAULT("rate", 1.0f))
		inline void Set3DAnimationPlayRate(float rate)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		if (anim) anim->playRate = rate;
	}
	FUNCTION(GRAPH_CALLABLE("Animator.SetTime"), DISPLAY_NAME("Set 3D Animation Time"), CATEGORY("Animation"),
		GRAPH_PIN_DEFAULT("time", 0.0f))
		inline void Set3DAnimationTime(float time)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		if (anim) anim->SetTime(time);
	}
	FUNCTION(GRAPH_PURE("Animator.IsPlaying"), DISPLAY_NAME("Is 3D Animation Playing"), CATEGORY("Animation"))
		inline bool Is3DAnimationPlaying()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return false;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		return anim ? anim->isPlaying : false;
	}
	FUNCTION(GRAPH_PURE("Animator.GetCurrentTime"), DISPLAY_NAME("Get 3D Animation Time"), CATEGORY("Animation"))
		inline float Get3DAnimationCurrentTime()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return 0.0f;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		return anim ? anim->currentTime : 0.0f;
	}

	// ── 3D Animator 파라미터 제어 노드 ──
	FUNCTION(GRAPH_CALLABLE("Animator.SetFloat"), DISPLAY_NAME("NODE_ANIMATOR_SET_FLOAT"), CATEGORY("Animation"),
		GRAPH_PIN_DEFAULT("value", 0.0f))
		inline void SetAnimatorFloat(std::string name, float value)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		if (anim) anim->SetFloat(name, value);
	}

	FUNCTION(GRAPH_CALLABLE("Animator.SetInt"), DISPLAY_NAME("NODE_ANIMATOR_SET_INT"), CATEGORY("Animation"),
		GRAPH_PIN_DEFAULT("value", 0))
		inline void SetAnimatorInt(std::string name, std::int64_t value)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		if (anim) anim->SetInt(name, static_cast<int32>(value));
	}

	FUNCTION(GRAPH_CALLABLE("Animator.SetBool"), DISPLAY_NAME("NODE_ANIMATOR_SET_BOOL"), CATEGORY("Animation"),
		GRAPH_PIN_DEFAULT("value", false))
		inline void SetAnimatorBool(std::string name, bool value)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		if (anim) anim->SetBool(name, value);
	}

	FUNCTION(GRAPH_CALLABLE("Animator.SetTrigger"), DISPLAY_NAME("NODE_ANIMATOR_SET_TRIGGER"), CATEGORY("Animation"))
		inline void SetAnimatorTrigger(std::string name)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		if (anim) anim->SetTrigger(name);
	}

	FUNCTION(GRAPH_PURE("Animator.GetFloat"), DISPLAY_NAME("NODE_ANIMATOR_GET_FLOAT"), CATEGORY("Animation"))
		inline float GetAnimatorFloat(std::string name)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return 0.0f;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		return anim ? anim->runtimeState.GetFloat(name) : 0.0f;
	}

	FUNCTION(GRAPH_PURE("Animator.GetBool"), DISPLAY_NAME("NODE_ANIMATOR_GET_BOOL"), CATEGORY("Animation"))
		inline bool GetAnimatorBool(std::string name)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return false;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		return anim ? anim->runtimeState.GetBool(name) : false;
	}
	// ── 3D Animator 레이어 제어 노드 ──
	FUNCTION(GRAPH_CALLABLE("Animator.SetLayerWeight"), DISPLAY_NAME("NODE_ANIMATOR_SET_LAYER_WEIGHT"), CATEGORY("Animation"),
		GRAPH_PIN_DEFAULT("weight", 1.0f))
		inline void SetAnimatorLayerWeight(std::string layerName, float weight)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		if (anim) anim->SetLayerWeight(layerName, weight);
	}
	FUNCTION(GRAPH_PURE("Animator.GetLayerWeight"), DISPLAY_NAME("NODE_ANIMATOR_GET_LAYER_WEIGHT"), CATEGORY("Animation"))
		inline float GetAnimatorLayerWeight(std::string layerName)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return 0.0f;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		return anim ? anim->GetLayerWeight(layerName) : 0.0f;
	}
	FUNCTION(GRAPH_CALLABLE("Animator.StopLayer"), DISPLAY_NAME("NODE_ANIMATOR_STOP_LAYER"), CATEGORY("Animation"))
		inline void StopAnimatorLayer(std::string layerName)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return;
		auto* anim = scene->GetRegistry().try_get<AnimatorComponent>(entity);
		if (anim) anim->StopLayer(layerName);
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
		auto* kcc = reg.try_get<CharacterControllerComponent>(entity);
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
		auto* kcc = reg.try_get<CharacterControllerComponent>(entity);
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
		const auto* kcc = reg.try_get<CharacterControllerComponent>(entity);
		return kcc ? kcc->IsGrounded : false;
	}
	FUNCTION(GRAPH_PURE("CharacterController.GetVelocity"), DISPLAY_NAME("Get Character Velocity"), CATEGORY("CharacterController"))
		inline std::array<float, 3> CharacterGetVelocity()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return { 0.0f, 0.0f, 0.0f };
		auto& reg = scene->GetRegistry();
		const auto* kcc = reg.try_get<CharacterControllerComponent>(entity);
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
		auto* kcc = reg.try_get<CharacterControllerComponent>(entity);
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
		const auto* kcc = reg.try_get<CharacterControllerComponent>(entity);
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
		const auto* kcc = reg.try_get<CharacterControllerComponent>(entity);
		return kcc ? kcc->GroundAngle : 0.0f;
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
		auto* arm = reg.try_get<SpringArmComponent>(entity);
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
		auto* arm = reg.try_get<SpringArmComponent>(entity);
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
		auto* arm = reg.try_get<SpringArmComponent>(entity);
		if (arm) arm->TargetArmLength = length;
	}

	FUNCTION(GRAPH_PURE("SpringArm.GetArmLength"), DISPLAY_NAME("Get Arm Length"), CATEGORY("Camera"))
		inline float SpringArmGetArmLength()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return 4.0f;

		auto& reg = scene->GetRegistry();
		const auto* arm = reg.try_get<SpringArmComponent>(entity);
		return arm ? arm->CurrentArmLength : 4.0f;
	}
}