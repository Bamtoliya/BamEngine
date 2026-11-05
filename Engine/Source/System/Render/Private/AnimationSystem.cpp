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
			if (!skeleton || !animator.isPlaying) continue;

			auto& localPose = animator.localBonePose;
			localPose.clear();

			// 3. 베이스 로컬 포즈 평가
			if (animator.runtimeState.HasStateMachine())
			{
				animator.runtimeState.UpdateLocalPose(dt * animator.playRate, *skeleton, localPose);
				animator.currentTime = animator.runtimeState.GetStateTime();

				auto notifies = animator.runtimeState.PopTriggeredNotifies();
				for (const auto& ev : notifies)
				{
					for (const auto& cb : animator.notifyCallbacks)
					{
						if (cb) cb(entity, ev);
					}
				}
			}
			else
			{
				const auto* baseClip = animator.currentClip.Get();
				if (!baseClip) continue;

				const f64 tps = baseClip->GetTicksPerSecond() > 0.0 ? baseClip->GetTicksPerSecond() : 25.0;
				const f32 durationSec = baseClip->GetDuration() > 0.0
					? static_cast<f32>(baseClip->GetDuration() / tps) : 0.0f;

				animator.currentTime += dt * animator.playRate;
				if (!animator.isLooping && durationSec > 0.0f && animator.currentTime >= durationSec)
				{
					animator.currentTime = durationSec;
					animator.isPlaying = false;
				}

				AnimationEvaluator::EvaluateLocalPose(*baseClip, *skeleton,
					animator.currentTime, localPose, animator.isLooping);
			}

			if (localPose.empty()) continue;

			// 4. 기존 포즈 위에 레이어를 배열 순서대로 누적 합성
			for (auto& layer : animator.layers)
			{
				if (!layer.isPlaying || layer.weight <= 0.0f) continue;

				const auto* layerClip = layer.clip.Get();
				if (!layerClip) continue;

				if (layer.mask.boneWeights.size() != skeleton->GetBones().size())
				{
					layer.mask.BuildWeights(*skeleton);
				}

				const f64 layerTps = layerClip->GetTicksPerSecond() > 0.0
					? layerClip->GetTicksPerSecond() : 25.0;
				const f32 layerDuration = layerClip->GetDuration() > 0.0
					? static_cast<f32>(layerClip->GetDuration() / layerTps) : 0.0f;

				layer.currentTime += dt * layer.playRate;
				if (!layer.isLooping && layerDuration > 0.0f && layer.currentTime >= layerDuration)
				{
					layer.currentTime = layerDuration;
					layer.isPlaying = false;
				}

				AnimationEvaluator::ApplyMaskedLocalPose(*layerClip, layer.currentTime,
					layer.mask.boneWeights, layer.weight, *skeleton, localPose, layer.isLooping);
			}

			// 5. 모든 합성이 끝난 뒤 최종 스키닝 행렬 생성
			AnimationEvaluator::BuildSkinningMatrices(*skeleton, localPose, animator.finalBoneMatrices);
		}
	}
}