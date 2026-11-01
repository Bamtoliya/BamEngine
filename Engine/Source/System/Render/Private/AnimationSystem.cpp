#include "AnimationSystem.h"
#include "Components/Render/AnimatorComponent.h"
#include "Components/Render/SkinnedMeshRendererComponent.h"
#include "Components/Core/FlagComponent.h"
#include "Components/Core/HierarchyComponent.h"
#include "AnimationEvaluator.h"
#include "Scene.h"

namespace
{
	// 엔티티 자신 또는 자식 계층을 순회하여 첫 번째 유효한 Skeleton을 탐색합니다.
	const Skeleton* FindSkeletonInHierarchy(const entt::registry& world, entt::entity root)
	{
		if (const auto* skinned = world.try_get<SkinnedMeshRendererComponent>(root))
		{
			if (const auto* skeleton = skinned->skeletonHandle.Get()) return skeleton;
		}

		const auto* hierarchy = world.try_get<HierarchyComponent>(root);
		if (!hierarchy) return nullptr;

		for (entt::entity child : hierarchy->children)
		{
			if (!world.valid(child)) continue;
			if (const auto* skeleton = FindSkeletonInHierarchy(world, child)) return skeleton;
		}
		return nullptr;
	}
}

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
			if (flag && (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, EEntityFlag::Paused) ||
				HasFlag(flag->effectiveFlags, EEntityFlag::Dead)))
			{
				continue;
			}

			auto& animator = view.get<AnimatorComponent>(entity);

			// 1. 인스펙터 또는 씬 직렬화로 설정된 .animator 컨트롤러 경로 자동 로드 동기화
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
					// 로드 실패 시 매 프레임 디스크 재시도 폭주를 막기 위해 시도 경로 기록
					animator.loadedControllerPath = animator.controllerPath;
				}
			}

			// 2. 자신 또는 자식 엔티티의 SkinnedMeshRendererComponent에서 스켈레톤 탐색
			const auto* skeleton = FindSkeletonInHierarchy(world, entity);
			if (!skeleton || !animator.isPlaying) continue;

			// 3. 상태 머신이 바인딩되어 있는 경우 ASM 런타임 갱신, 아니면 단일 클립 평가
			if (animator.runtimeState.HasStateMachine())
			{
				animator.runtimeState.Update(dt * animator.playRate, *skeleton, animator.finalBoneMatrices);
			}
			else
			{
				auto* clip = animator.currentClip.Get();
				if (!clip) continue;

				animator.currentTime += dt * animator.playRate;
				if (!animator.isLooping && clip->GetDuration() > 0.0 && clip->GetTicksPerSecond() > 0.0)
				{
					const f32 durationSec = static_cast<f32>(clip->GetDuration() / clip->GetTicksPerSecond());
					if (animator.currentTime >= durationSec)
					{
						animator.currentTime = durationSec;
						animator.isPlaying = false;
					}
				}
				AnimationEvaluator::EvaluateSkinningMatrices(*clip, *skeleton, animator.currentTime, animator.finalBoneMatrices);
			}
		}
	}
}