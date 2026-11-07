#include "AnimationSystem.h"
#include "Components/Render/AnimatorComponent.h"
#include "Components/Render/SkinnedMeshRendererComponent.h"
#include "Components/Core/FlagComponent.h"
#include "Components/Core/HierarchyComponent.h"
#include "AnimationEvaluator.h"
#include "Scene.h"
#include "AnimationSceneQuery.h"
#include "Skeleton.h"
#include "AnimatorPoseUpdate.h"

AnimationSystem* AnimationSystem::Create(void* arg)
{
	auto* instance = new AnimationSystem();
	if (instance->Initialize(arg) != EResult::Success)
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void AnimationSystem::OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
	(void)registry;
	if (!std::isfinite(dt) || dt <= 0.0f) return;

	for (Scene* scene : activeScenes)
	{
		if (!scene || !scene->IsActive() || scene->IsPaused()) continue;

		auto& world = scene->GetRegistry();
		auto view = world.view<AnimatorComponent>();

		for (auto entity : view)
		{
			const auto* flag = world.try_get<FlagComponent>(entity);
			if (flag && (!flag->IsActiveInHierarchy() ||
				HasFlag(flag->effectiveFlags, EEntityFlag::Paused) ||
				HasFlag(flag->effectiveFlags, EEntityFlag::Dead)))
			{
				continue;
			}

			auto& animator = view.get<AnimatorComponent>(entity);

			// 1. 컨트롤러 경로 변경 동기화
			if (animator.controllerPath != animator.loadedControllerPath)
			{
				if (animator.controllerPath.empty())
				{
					animator.stateMachineAsset.reset();
					animator.runtimeState.BindStateMachine(nullptr);
					animator.loadedControllerPath.clear();
				}
				else if (!animator.LoadController(animator.controllerPath))
				{
					// 기존 정책: 같은 실패 경로를 매 프레임 다시 로드하지 않는다.
					animator.loadedControllerPath = animator.controllerPath;
				}
			}

			// 2. 스켈레톤과 재생 상태 확인
			const auto* skeleton = FindSkeletonInHierarchy(world, entity);
			if (!skeleton) continue;

			if (!animator.isPlaying)
			{
				if (animator.finalBoneMatrices.empty())
				{
					Engine::AnimationEvaluator::EvaluateBindPoseSkinningMatrices(*skeleton, animator.finalBoneMatrices);
				}
				continue;
			}

			const auto source = Engine::UpdateAnimatorBasePose(animator, *skeleton, dt);
			if (source == Engine::EAnimatorBasePoseSource::None)
			{
				if (animator.finalBoneMatrices.empty())
				{
					Engine::AnimationEvaluator::EvaluateBindPoseSkinningMatrices(*skeleton, animator.finalBoneMatrices);
				}
				continue;
			}

			if (source == Engine::EAnimatorBasePoseSource::StateMachine)
			{
				const auto notifies = animator.runtimeState.PopTriggeredNotifies();
				for (const auto& event : notifies)
				{
					for (const auto& callback : animator.notifyCallbacks)
					{
						if (callback) callback(entity, event);
					}
				}
			}

			Engine::UpdateAnimatorSkinning(animator, *skeleton, dt);
		}
	}
}