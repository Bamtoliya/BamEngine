#include "AnimationSystem.h"
#include "ComponentEnable.h"
#include "AnimatorComponent.h"
#include "SkinnedMeshRendererComponent.h"
#include "FlagComponent.h"
#include "HierarchyComponent.h"
#include "AnimationEvaluator.h"
#include "Scene.h"
#include "AnimationSceneQuery.h"
#include "Skeleton.h"
#include "AnimatorPoseUpdate.h"
#ifdef BAM_EDITOR
#include "Logger.h"
#include "ReflectionRegistry.h"
#include "ResourceManager.h"
#include <filesystem>
#include <string_view>
#include <unordered_set>
#endif

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

#ifdef BAM_EDITOR
void AnimationSystem::PrepareSceneControllerResources(const Scene& scene)
{
	if (!ReflectionRegistry::IsCreated() || !ResourceManager::IsCreated()) return;

	const auto& world = scene.GetRegistry();
	std::unordered_set<std::string_view> preparedPaths;
	for (auto [entity, animator] : world.view<const AnimatorComponent>().each())
	{
		if (!IsComponentEnabled(world, entity, animator) || animator.controllerPath.empty()) continue;
		if (!preparedPaths.emplace(animator.controllerPath).second) continue;

		// 역직렬화가 클립을 리소스 매니저에 올린다. 엔티티 바인딩은 첫 게임 틱에서 수행한다.
		try
		{
			AnimatorStateMachine controller;
			controller.LoadFromFile(animator.controllerPath);
		}
		catch (const std::filesystem::filesystem_error& error)
		{
			BAM_LOG(Warn, "Engine", "AnimatorTiming", "Scene preparation failed for '{}': {}", animator.controllerPath, error.what());
		}
	}
}
#endif

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
			if (!IsComponentEnabled<AnimatorComponent>(world, entity)) continue;
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
					animator.ClearController();
				}
				else if (!animator.LoadController(animator.controllerPath))
				{
					// 기존 정책: 같은 실패 경로를 매 프레임 다시 로드하지 않는다.
					animator.loadedControllerPath = animator.controllerPath;
				}
			}

			// 2. 스켈레톤과 재생 상태 확인
			const auto* skeleton = FindEnabledSkeletonInHierarchy(world, entity);
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
