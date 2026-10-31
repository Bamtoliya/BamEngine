#include "AnimationSystem.h"
#include "Components/Render/AnimatorComponent.h"
#include "Components/Render/SkinnedMeshRendererComponent.h"
#include "Components/Core/FlagComponent.h"
#include "AnimationEvaluator.h"
#include "Scene.h"

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
		auto view = world.view<AnimatorComponent, SkinnedMeshRendererComponent>();

		for (auto entity : view)
		{
			const auto* flag = world.try_get<FlagComponent>(entity);
			if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) ||
				HasFlag(flag->flags, EEntityFlag::Dead)))
			{
				continue;
			}

			auto& animator = view.get<AnimatorComponent>(entity);
			const auto& skinnedRenderer = view.get<SkinnedMeshRendererComponent>(entity);

			auto* skeleton = skinnedRenderer.skeletonHandle.Get();
			if (!skeleton || !animator.isPlaying) continue;

			// 상태 머신이 바인딩되어 있는 경우 ASM 런타임 갱신
			if (animator.runtimeState.HasStateMachine())
			{
				animator.runtimeState.Update(dt * animator.playRate, *skeleton, animator.finalBoneMatrices);
			}
			else
			{
				auto* clip = animator.currentClip.Get();
				if (!clip) continue;

				animator.currentTime += dt * animator.playRate;
				AnimationEvaluator::EvaluateSkinningMatrices(*clip, *skeleton, animator.currentTime, animator.finalBoneMatrices);
			}
		}
	}
}