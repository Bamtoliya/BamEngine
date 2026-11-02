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

			// 3. 베이스 레이어(ASM 또는 단일 클립) 평가
			const Animation* baseClip = nullptr;
			float baseTime = animator.currentTime;
			bool isLoopingBase = animator.isLooping;

			if (animator.runtimeState.HasStateMachine())
			{
				animator.runtimeState.Update(dt * animator.playRate, *skeleton, animator.finalBoneMatrices);
				animator.currentTime = animator.runtimeState.GetStateTime();
				baseTime = animator.currentTime;
				baseClip = animator.runtimeState.GetCurrentClip();
				if (const auto* st = animator.runtimeState.GetStateMachine()->FindState(animator.runtimeState.GetCurrentStateName()))
				{
					isLoopingBase = st->isLooping;
				}

				// 트리거된 노티파이 이벤트 디스패치
				auto notifies = animator.runtimeState.PopTriggeredNotifies();
				for (const auto& ev : notifies)
				{
					for (const auto& cb : animator.notifyCallbacks) { if (cb) cb(entity, ev); }
				}
			}
			else
			{
				baseClip = animator.currentClip.Get();
				if (baseClip)
				{
					const f64 tps = baseClip->GetTicksPerSecond() > 0.0 ? baseClip->GetTicksPerSecond() : 25.0;
					const f32 durationSec = baseClip->GetDuration() > 0.0 ? static_cast<f32>(baseClip->GetDuration() / tps) : 0.0f;
					animator.currentTime += dt * animator.playRate;
					if (!animator.isLooping && durationSec > 0.0f && animator.currentTime >= durationSec)
					{
						animator.currentTime = durationSec;
						animator.isPlaying = false;
					}
					baseTime = animator.currentTime;
					AnimationEvaluator::EvaluateSkinningMatrices(*baseClip, *skeleton, baseTime, animator.finalBoneMatrices, animator.isLooping);
				}
			}

			// 4. 상위 오버레이 레이어(Layered Animation) 마스킹 합성
			if (baseClip)
			{
				for (auto& layer : animator.layers)
				{
					if (!layer.isPlaying || layer.weight <= 0.0f) continue;
					auto* layerClip = layer.clip.Get();
					if (!layerClip) continue;

					// 스켈레톤 본 개수에 맞게 마스크 가중치 캐시 갱신
					if (layer.mask.boneWeights.size() != skeleton->GetBones().size())
					{
						layer.mask.BuildWeights(*skeleton);
					}

					// 레이어 타임라인 진행 및 비루프 재생 종료 검출
					const f64 layerTps = layerClip->GetTicksPerSecond() > 0.0 ? layerClip->GetTicksPerSecond() : 25.0;
					const f32 layerDuration = layerClip->GetDuration() > 0.0 ? static_cast<f32>(layerClip->GetDuration() / layerTps) : 0.0f;
					layer.currentTime += dt * layer.playRate;
					if (!layer.isLooping && layerDuration > 0.0f && layer.currentTime >= layerDuration)
					{
						layer.currentTime = layerDuration;
						layer.isPlaying = false;
					}

					AnimationEvaluator::EvaluateMaskedSkinningMatrices(*baseClip, baseTime, *layerClip, layer.currentTime, layer.mask.boneWeights, layer.weight, *skeleton, animator.finalBoneMatrices, isLoopingBase, layer.isLooping);
				}
			}
		}
	}
}