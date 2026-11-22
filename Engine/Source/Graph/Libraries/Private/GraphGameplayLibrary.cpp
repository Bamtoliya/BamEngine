#include "GraphGameplayLibrary.h"
#include "AnimationIK.h"
#include "AnimationSceneQuery.h"
#include "ComponentEnable.h"
#include "Scene.h"
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
}
