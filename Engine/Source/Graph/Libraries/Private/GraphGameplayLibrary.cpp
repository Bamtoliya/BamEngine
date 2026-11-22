#include "GraphGameplayLibrary.h"
#include "AnimationIK.h"
#include "AnimationSceneQuery.h"
#include "ComponentEnable.h"
#include "Scene.h"
#include "SaveGameManager.h"
#include "NavAgentComponent.h"
#include "AIPerceptionComponent.h"
#include "AudioManager.h"
#include "AudioMixer.h"
#include "SceneManager.h"
#include <limits>

namespace Engine
{
	namespace
	{
		entt::entity ResolveTargetEntity(const entt::registry& reg, uint64 entityId)
		{
			if (entityId == 0)
			{
				return GraphSystem::GetCurrentEntity();
			}
			if (entityId > static_cast<uint64>(std::numeric_limits<uint32>::max()))
			{
				return entt::null;
			}
			const auto target = static_cast<entt::entity>(static_cast<uint32>(entityId));
			return reg.valid(target) ? target : entt::null;
		}

		AnimationIKComponent* FindTargetIKComponent(entt::registry& reg, entt::entity entity)
		{
			if (entity == entt::null || !reg.valid(entity)) return nullptr;
			auto* ik = reg.try_get<AnimationIKComponent>(entity);
			if (!ik)
			{
				const entt::entity animEntity = FindAnimatorEntityInHierarchy(reg, entity);
				if (animEntity != entt::null)
					ik = reg.try_get<AnimationIKComponent>(animEntity);
			}
			return ik;
		}
	}

	void GraphGameplayLibrary::SetAnimationIKEnabled(uint64 entityId, bool enabled)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return;
		SetAnimationIKEnabled(*scene, entityId, enabled);
	}

	void GraphGameplayLibrary::SetAnimationIKEnabled(Scene& scene, uint64 entityId, bool enabled)
	{
		auto& reg = scene.GetRegistry();
		const entt::entity target = ResolveTargetEntity(reg, entityId);
		if (auto* ik = FindTargetIKComponent(reg, target))
		{
			ik->enabled = enabled;
		}
	}

	void GraphGameplayLibrary::SetTwoBoneIKTarget(uint64 entityId, int32 chainIndex, vec3 targetPos, float weight)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return;
		SetTwoBoneIKTarget(*scene, entityId, chainIndex, targetPos, weight);
	}

	void GraphGameplayLibrary::SetTwoBoneIKTarget(Scene& scene, uint64 entityId, int32 chainIndex, vec3 targetPos, float weight)
	{
		auto& reg = scene.GetRegistry();
		const entt::entity target = ResolveTargetEntity(reg, entityId);
		if (auto* ik = FindTargetIKComponent(reg, target))
		{
			if (chainIndex >= 0 && static_cast<size_t>(chainIndex) < ik->twoBoneChains.size())
			{
				ik->twoBoneChains[chainIndex].targetPosition = targetPos;
				ik->twoBoneChains[chainIndex].weight = weight;
				ik->twoBoneChains[chainIndex].targetInWorldSpace = true;
			}
		}
	}

	void GraphGameplayLibrary::SetLookAtIKTarget(uint64 entityId, int32 targetIndex, vec3 targetPos, float weight)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return;
		SetLookAtIKTarget(*scene, entityId, targetIndex, targetPos, weight);
	}

	void GraphGameplayLibrary::SetLookAtIKTarget(Scene& scene, uint64 entityId, int32 targetIndex, vec3 targetPos, float weight)
	{
		auto& reg = scene.GetRegistry();
		const entt::entity target = ResolveTargetEntity(reg, entityId);
		if (auto* ik = FindTargetIKComponent(reg, target))
		{
			if (targetIndex >= 0 && static_cast<size_t>(targetIndex) < ik->lookAtTargets.size())
			{
				ik->lookAtTargets[targetIndex].targetPosition = targetPos;
				ik->lookAtTargets[targetIndex].weight = weight;
				ik->lookAtTargets[targetIndex].targetInWorldSpace = true;
			}
		}
	}

	void GraphGameplayLibrary::SetFootGroundIKEnabled(uint64 entityId, bool enabled, float footHeightOffset)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return;
		SetFootGroundIKEnabled(*scene, entityId, enabled, footHeightOffset);
	}

	void GraphGameplayLibrary::SetFootGroundIKEnabled(Scene& scene, uint64 entityId, bool enabled, float footHeightOffset)
	{
		auto& reg = scene.GetRegistry();
		const entt::entity target = ResolveTargetEntity(reg, entityId);
		if (auto* ik = FindTargetIKComponent(reg, target))
		{
			ik->enableFootGroundIK = enabled;
			if (footHeightOffset >= 0.0f)
			{
				ik->footHeightOffset = footHeightOffset;
			}
		}
	}

	// ── SaveGame ──
	bool GraphGameplayLibrary::WriteCheckpointSlot(const std::string& slotName, const std::string& checkpointId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		const entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return false;
		return WriteCheckpointSlot(*scene, entity, slotName, checkpointId);
	}

	bool GraphGameplayLibrary::WriteCheckpointSlot(Scene& scene, entt::entity entity, const std::string& slotName, const std::string& checkpointId)
	{
		if (slotName.empty()) return false;
		SaveGameData data;
		std::string sceneUri;
		const std::wstring& wsName = scene.GetName();
		sceneUri.assign(wsName.begin(), wsName.end());
		if (!SaveGameManager::CaptureCheckpointFromEntity(scene, entity, sceneUri, checkpointId, data))
		{
			return false;
		}
		return SaveGameManager::Get().WriteSlot(slotName, data);
	}

	bool GraphGameplayLibrary::LoadCheckpointSlot(const std::string& slotName)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		const entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return false;
		return LoadCheckpointSlot(*scene, entity, slotName);
	}

	bool GraphGameplayLibrary::LoadCheckpointSlot(Scene& scene, entt::entity entity, const std::string& slotName)
	{
		if (slotName.empty()) return false;
		SaveGameData data;
		if (!SaveGameManager::Get().ReadSlot(slotName, data))
		{
			return false;
		}
		return SaveGameManager::ApplyCheckpointToEntity(scene, entity, data);
	}

	bool GraphGameplayLibrary::HasSlot(const std::string& slotName)
	{
		if (slotName.empty()) return false;
		return SaveGameManager::Get().HasSlot(slotName);
	}

	bool GraphGameplayLibrary::DeleteSlot(const std::string& slotName)
	{
		if (slotName.empty()) return false;
		return SaveGameManager::Get().DeleteSlot(slotName);
	}

	// ── Navigation & AI ──
	void GraphGameplayLibrary::SetNavDestination(int64 entityId, float x, float y, float z)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return;
		auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null) return;
		SetNavDestination(*scene, target, vec3(x, y, z));
	}

	void GraphGameplayLibrary::SetNavDestination(Scene& scene, entt::entity entity, const vec3& dest)
	{
		auto& reg = scene.GetRegistry();
		if (entity == entt::null || !reg.valid(entity)) return;
		auto* agent = TryGetEnabledComponent<NavAgentComponent>(reg, entity);
		if (!agent) return;
		agent->SetDestination(dest);
	}

	void GraphGameplayLibrary::StopNav(int64 entityId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return;
		auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null) return;
		StopNav(*scene, target);
	}

	void GraphGameplayLibrary::StopNav(Scene& scene, entt::entity entity)
	{
		auto& reg = scene.GetRegistry();
		if (entity == entt::null || !reg.valid(entity)) return;
		auto* agent = TryGetEnabledComponent<NavAgentComponent>(reg, entity);
		if (!agent) return;
		agent->Stop();
	}

	bool GraphGameplayLibrary::HasReachedNavDestination(int64 entityId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return false;
		const auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null) return false;
		return HasReachedNavDestination(*scene, target);
	}

	bool GraphGameplayLibrary::HasReachedNavDestination(const Scene& scene, entt::entity entity)
	{
		const auto& reg = scene.GetRegistry();
		if (entity == entt::null || !reg.valid(entity)) return false;
		const auto* agent = TryGetEnabledComponent<NavAgentComponent>(reg, entity);
		if (!agent) return false;
		return agent->reachedDestination;
	}

	int64 GraphGameplayLibrary::GetAIDetectedTarget(int64 entityId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return -1;
		const auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null) return -1;
		return GetAIDetectedTarget(*scene, target);
	}

	int64 GraphGameplayLibrary::GetAIDetectedTarget(const Scene& scene, entt::entity entity)
	{
		const auto& reg = scene.GetRegistry();
		if (entity == entt::null || !reg.valid(entity)) return -1;
		const auto* perception = TryGetEnabledComponent<AIPerceptionComponent>(reg, entity);
		if (!perception || perception->detectedTarget == entt::null || !reg.valid(perception->detectedTarget))
		{
			return -1;
		}
		return static_cast<int64>(static_cast<uint32>(perception->detectedTarget));
	}

	bool GraphGameplayLibrary::HasAILineOfSight(int64 entityId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return false;
		const auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null) return false;
		return HasAILineOfSight(*scene, target);
	}

	bool GraphGameplayLibrary::HasAILineOfSight(const Scene& scene, entt::entity entity)
	{
		const auto& reg = scene.GetRegistry();
		if (entity == entt::null || !reg.valid(entity)) return false;
		const auto* perception = TryGetEnabledComponent<AIPerceptionComponent>(reg, entity);
		if (!perception) return false;
		return perception->hasLineOfSight;
	}

	// ── AudioMixer ──
	void GraphGameplayLibrary::SetBusVolume(const std::string& busName, float volume)
	{
		if (!AudioManager::IsCreated()) return;
		const auto busOpt = AudioMixer::ParseBusName(busName);
		if (!busOpt.has_value()) return;
		AudioManager::Get().GetMixer().SetBusVolume(busOpt.value(), volume);
	}

	float GraphGameplayLibrary::GetBusVolume(const std::string& busName)
	{
		if (!AudioManager::IsCreated()) return 0.0f;
		const auto busOpt = AudioMixer::ParseBusName(busName);
		if (!busOpt.has_value()) return 0.0f;
		return AudioManager::Get().GetMixer().GetBusVolume(busOpt.value());
	}

	void GraphGameplayLibrary::SetBusMuted(const std::string& busName, bool muted)
	{
		if (!AudioManager::IsCreated()) return;
		const auto busOpt = AudioMixer::ParseBusName(busName);
		if (!busOpt.has_value()) return;
		AudioManager::Get().GetMixer().SetBusMuted(busOpt.value(), muted);
	}

	void GraphGameplayLibrary::FadeBusVolume(const std::string& busName, float targetVolume, float duration)
	{
		if (!AudioManager::IsCreated()) return;
		const auto busOpt = AudioMixer::ParseBusName(busName);
		if (!busOpt.has_value()) return;
		AudioManager::Get().GetMixer().FadeBusVolume(busOpt.value(), targetVolume, duration);
	}

	// ── SceneStreaming ──
	void GraphGameplayLibrary::MarkDontDestroyOnLoad(int64 entityId, const std::string& group)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene || !SceneManager::IsCreated()) return;
		auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null) return;
		MarkDontDestroyOnLoad(*scene, target, group);
	}

	void GraphGameplayLibrary::MarkDontDestroyOnLoad(Scene& scene, entt::entity entity, const std::string& group)
	{
		if (!SceneManager::IsCreated() || entity == entt::null) return;
		(void)SceneManager::Get().MarkDontDestroyOnLoad(scene, entity, group);
	}

	bool GraphGameplayLibrary::UnloadAdditiveScene(const std::string& subSceneName)
	{
		if (!SceneManager::IsCreated() || subSceneName.empty()) return false;
		const EResult result = SceneManager::Get().UnloadAdditiveScene(subSceneName);
		return (result == EResult::Success);
	}
}
