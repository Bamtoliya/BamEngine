#include "ActionTimelineSystem.h"
#include "ActionTimelineComponent.h"
#include "AnimatorComponent.h"
#include "SkinnedMeshRendererComponent.h"
#include "CameraComponent.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "FlagComponent.h"
#include "HierarchyComponent.h"
#include "ResourceManager.h"
#include "AudioManager.h"
#include "AudioClip.h"
#include "Scene.h"
#include "PlatformPath.h"
#include <cmath>
#include "AnimationSceneQuery.h"
#include "Skeleton.h"
#include "CompositeVFXComponent.h" 
#include "CompositeEffect.h"       
#include "NameComponent.h"
#include "AnimationEvaluator.h"
#include "Animation.h"
#include "Model.h"
#include "ModelInstanceComponent.h"
#include "Logger.h"
#include "ActionTimelineEvents.h"
#include "SystemManager.h"

namespace
{
	struct SocketWorldTransform
	{
		vec3 position = vec3(0.0f);
		quat rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
	};

	// 본 소켓 월드 위치 및 회전 계산 헬퍼
	SocketWorldTransform ComputeSocketWorldTransform(const entt::registry& world, entt::entity entity, const std::string& boneName, const vec3& offsetPos, const vec3& offsetRot)
	{
		SocketWorldTransform result;
		const auto* worldTrans = world.try_get<WorldTransformComponent>(entity);
		const mat4 entityWorldMat = worldTrans ? worldTrans->worldMatrix : mat4(1.0f);

		const mat4 rotMat = glm::mat4_cast(glm::quat(glm::radians(offsetRot)));
		const mat4 offsetMat = glm::translate(mat4(1.0f), offsetPos) * rotMat;

		if (boneName.empty())
		{
			const mat4 finalMat = entityWorldMat * offsetMat;
			result.position = vec3(finalMat[3]);
			result.rotation = glm::quat_cast(finalMat);
			return result;
		}

		// 본 행렬은 Animator 엔티티(모델 루트) 공간이다. 대상 엔티티의 자식에 모델을 둔 구성도 있으므로 자식 계층까지 찾는다.
		const entt::entity animatorEntity = FindAnimatorEntityInHierarchy(world, entity);
		const auto* animator = animatorEntity != entt::null ? world.try_get<AnimatorComponent>(animatorEntity) : nullptr;
		const auto* skeleton = animator ? FindSkeletonInHierarchy(world, animatorEntity) : nullptr;
		if (!animator || !skeleton || animator->finalBoneMatrices.empty())
		{
			const mat4 finalMat = entityWorldMat * offsetMat;
			result.position = vec3(finalMat[3]);
			result.rotation = glm::quat_cast(finalMat);
			return result;
		}

		const std::wstring boneNameW = StrToWStr(boneName);
		const auto& boneMap = skeleton->GetBoneMap();
		const auto& bones = skeleton->GetBones();
		auto it = boneMap.find(boneNameW);
		if (it != boneMap.end())
		{
			uint32 boneIdx = it->second;
			if (boneIdx < animator->finalBoneMatrices.size() && boneIdx < bones.size())
			{
				mat4 invOffset = glm::inverse(bones[boneIdx].OffsetMatrix);
				mat4 combined = animator->finalBoneMatrices[boneIdx] * invOffset;
				// 스킨드 렌더링과 같이 Animator 엔티티의 월드 행렬을 곱한다. 같은 엔티티 구성이면 entityWorldMat과 같다.
				const auto* animatorWorld = world.try_get<WorldTransformComponent>(animatorEntity);
				const mat4 animatorWorldMat = animatorWorld ? animatorWorld->worldMatrix : entityWorldMat;
				mat4 finalMat = animatorWorldMat * combined * offsetMat;
				result.position = vec3(finalMat[3]);
				result.rotation = glm::quat_cast(finalMat);
				return result;
			}
		}

		const mat4 finalMat = entityWorldMat * offsetMat;
		result.position = vec3(finalMat[3]);
		result.rotation = glm::quat_cast(finalMat);
		return result;
	}

	// 본 소켓 월드 좌표 계산 헬퍼 (기존 호환 유지)
	vec3 ComputeSocketWorldPos(const entt::registry& world, entt::entity entity, const std::string& boneName, const vec3& offset)
	{
		return ComputeSocketWorldTransform(world, entity, boneName, offset, vec3(0.0f)).position;
	}

	ResourceHandle<Animation> FindClipByName(const Model* model, const std::wstring& name)
	{
		if (!model) return {};
		for (const auto& clip : model->GetAnimations())
		{
			if (clip.IsValid() && clip->GetName() == name) return clip;
		}
		return {};
	}

	// 타임라인을 작성한 클립을 찾는다. Animator 엔티티(모델 루트)의 연결 모델을 먼저 보고, 없으면 타임라인에 저장된 작성 모델에서 찾는다.
	// 클립은 스켈레톤 기준이므로 같은 스켈레톤을 쓰는 다른 모델에도 적용할 수 있다.
	ResourceHandle<Animation> ResolveTimelineClip(const entt::registry& world, entt::entity animatorEntity, const ActionTimelineAsset& asset)
	{
		if (asset.GetClipName().empty()) return {};
		const std::wstring clipName = StrToWStr(asset.GetClipName());

		ResourceHandle<Animation> clip;
		if (const auto* instance = animatorEntity != entt::null ? world.try_get<ModelInstanceComponent>(animatorEntity) : nullptr)
			clip = FindClipByName(instance->modelHandle.Get(), clipName);
		if (!clip.IsValid() && !asset.GetModelPath().empty())
		{
			const ResourceHandle<Model> model(ResourceManager::Get().LoadFile(StrToWStr(asset.GetModelPath())));
			clip = FindClipByName(model.Get(), clipName);
		}
		if (!clip.IsValid())
			BAM_LOG(Warn, "Engine", "ActionTimeline", "Timeline clip '{}' was not found (model: '{}'). The animator keeps its current clip.", asset.GetClipName(), asset.GetModelPath());
		return clip;
	}

	// 노티파이 소켓 본이 엔티티 스켈레톤에 없으면 엔티티 원점에서 발동한다. 타임라인을 불러올 때 한 번 알린다.
	void WarnMissingSocketBones(const entt::registry& world, entt::entity entity, const ActionTimelineAsset& asset)
	{
		// 소켓 계산과 같은 스켈레톤(Animator 엔티티 계층)으로 판정한다. Animator가 없으면 모든 소켓이 원점으로 대체된다.
		const entt::entity animatorEntity = FindAnimatorEntityInHierarchy(world, entity);
		const auto* skeleton = animatorEntity != entt::null ? FindSkeletonInHierarchy(world, animatorEntity) : nullptr;
		for (const auto& track : asset.GetNotifyTracks())
		{
			for (const auto& notify : track.Notifies)
			{
				if (notify.SocketBoneName.empty()) continue;
				if (skeleton && skeleton->GetBoneMap().contains(StrToWStr(notify.SocketBoneName))) continue;
				BAM_LOG(Warn, "Engine", "ActionTimeline", "Socket bone '{}' of notify '{}' was not found on the entity skeleton. The entity origin is used instead.", notify.SocketBoneName, notify.Name);
			}
		}
	}

	// 타임라인이 Animator를 넘겨받는다. 같은 Animator를 이미 넘겨받았으면(재생 중 다시 Play) 처음 보관한 상태를 유지한다.
	void BeginAnimatorOverride(ActionTimelineComponent& timeline, entt::entity animatorEntity, const AnimatorComponent& animator)
	{
		timeline.animatorOverrideSerial = timeline.playSerial;
		if (timeline.animatorOverrideEntity == animatorEntity) return;
		timeline.animatorOverrideEntity = animatorEntity;
		timeline.savedAnimatorClip = animator.currentClip;
		timeline.savedAnimatorTime = animator.currentTime;
		timeline.savedAnimatorPlaying = animator.isPlaying;
	}

	// 넘겨받은 Animator를 보관한 상태로 되돌리고, 타임라인의 마지막 포즈에서 원래 애니메이션으로 섞어 넘어간다.
	// 원래 정지 상태였으면 AnimationSystem이 돌지 않으므로 섞지 않는다. Animator가 이미 제거되었으면 표시만 지운다.
	void EndAnimatorOverride(entt::registry& world, ActionTimelineComponent& timeline, float blendOutTime)
	{
		const entt::entity animatorEntity = timeline.animatorOverrideEntity;
		if (animatorEntity == entt::null) return;
		timeline.animatorOverrideEntity = entt::null;
		if (auto* animator = world.valid(animatorEntity) ? world.try_get<AnimatorComponent>(animatorEntity) : nullptr)
		{
			if (timeline.savedAnimatorPlaying) animator->BeginBlendFromPose(animator->localBonePose, blendOutTime);
			animator->currentClip = timeline.savedAnimatorClip;
			animator->currentTime = timeline.savedAnimatorTime;
			animator->isPlaying = timeline.savedAnimatorPlaying;
		}
		timeline.savedAnimatorClip = {};
	}
}

ActionTimelineSystem* ActionTimelineSystem::Create(void* arg)
{
	auto* instance = new ActionTimelineSystem();
	if (instance->Initialize(arg) != EResult::Success)
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void ActionTimelineSystem::OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
	(void)registry;
	if (!std::isfinite(dt) || dt <= 0.0f) return;

	// 게임플레이 이벤트는 게임 실행(Gameplay 페이즈 포함)에서만 발행한다. 에디터 실시간 프리뷰(Presentation만)에서는 보내지 않는다.
	const bool publishGameplayEvents = (SystemManager::Get().GetUpdatePhaseMask() & static_cast<uint8>(ESystemPhase::Gameplay)) != 0;

	for (Scene* scene : activeScenes)
	{
		if (!scene || !scene->IsActive() || scene->IsPaused()) continue;
		auto& world = scene->GetRegistry();

		// 타임라인 순회 중에는 모으기만 하고, 씬의 갱신이 끝난 뒤 같은 프레임에 발행한다(핸들러가 엔티티를 바꿔도 순회가 깨지지 않게).
		std::vector<ActionTimelineEvent> pendingEvents;

		// ── 0. 소켓 부착(AttachToSocket) 엔티티 위치 및 회전 실시간 동기화 ──
		{
			auto attachView = world.view<SocketAttachmentComponent, TransformComponent>();
			std::vector<entt::entity> deadAttachments;

			for (auto attachEntity : attachView)
			{
				auto& attach = attachView.get<SocketAttachmentComponent>(attachEntity);
				auto& trans = attachView.get<TransformComponent>(attachEntity);

				if (!world.valid(attach.targetEntity))
				{
					if (attach.autoDestroyOnTargetDead) deadAttachments.push_back(attachEntity);
					continue;
				}

				const auto* targetFlag = world.try_get<FlagComponent>(attach.targetEntity);
				if (targetFlag && HasFlag(targetFlag->effectiveFlags, EEntityFlag::Dead))
				{
					if (attach.autoDestroyOnTargetDead) deadAttachments.push_back(attachEntity);
					continue;
				}

				const auto xform = ComputeSocketWorldTransform(world, attach.targetEntity, attach.boneName, attach.offsetPosition, attach.offsetRotation);
				trans.SetLocalPosition(xform.position);
				trans.rotation = xform.rotation;
				trans.SetEulerRotation(glm::degrees(glm::eulerAngles(xform.rotation)));
				trans.SetLocalScale(attach.scale);
			}

			for (auto deadE : deadAttachments)
			{
				if (auto* ent = scene->FindEntity(deadE)) scene->DestroyEntity(*ent);
			}
		}

		// ── 0-1. 재생 완료된 1회성 액션 VFX 엔티티 자동 소멸 (AutoDestroy) ──
		{
			auto vfxView = world.view<CompositeVFXComponent, ActionVFXLifecycleComponent>();
			std::vector<entt::entity> finishedVFX;

			for (auto vfxEntity : vfxView)
			{
				const auto& vfxComp = vfxView.get<CompositeVFXComponent>(vfxEntity);
				const auto& lifecycle = vfxView.get<ActionVFXLifecycleComponent>(vfxEntity);

				if (lifecycle.autoDestroy && vfxComp.isFinished && !vfxComp.isLooping)
				{
					finishedVFX.push_back(vfxEntity);
				}
			}

			for (auto e : finishedVFX)
			{
				if (auto* ent = scene->FindEntity(e)) scene->DestroyEntity(*ent);
			}
		}

		auto view = world.view<ActionTimelineComponent>();

		for (auto entity : view)
		{
			const auto* flag = world.try_get<FlagComponent>(entity);
			if (flag && (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, EEntityFlag::Paused) || HasFlag(flag->effectiveFlags, EEntityFlag::Dead)))
			{
				continue;
			}

			auto& timeline = view.get<ActionTimelineComponent>(entity);

			// 1. 에셋 경로 변경 시 자동 로드 동기화
			if (timeline.timelinePath != timeline.loadedTimelinePath)
			{
				timeline.loadedTimelinePath = timeline.timelinePath;
				++timeline.playSerial; // 다른 타임라인으로 바뀌면 이전 지속형 VFX를 해제한다
				if (!timeline.timelinePath.empty())
				{
					auto handle = ResourceManager::Get().LoadFile(StrToWStr(timeline.timelinePath));
					timeline.timelineAsset = ResourceHandle<ActionTimelineAsset>(handle);
					if (const auto* loaded = timeline.timelineAsset.Get())
						WarnMissingSocketBones(world, entity, *loaded);
				}
				else
				{
					timeline.timelineAsset = {};
				}
			}

			auto* asset = timeline.timelineAsset.Get();

			// 세션이 끝났으면 Animator를 타임라인 이전 상태로 되돌린다. 일시정지(같은 세션, 끝나기 전)는 마지막 포즈를 유지한다.
			if (timeline.animatorOverrideEntity != entt::null)
			{
				const bool sessionChanged = timeline.animatorOverrideSerial != timeline.playSerial;
				const bool finished = asset && timeline.currentTime >= std::max(0.05f, asset->GetDuration());
				if (!asset || (!timeline.isPlaying && (sessionChanged || finished))) EndAnimatorOverride(world, timeline, asset ? asset->GetBlendOutTime() : 0.0f);
			}
			if (!asset || !timeline.isPlaying) continue;

			// 재생 세션이 바뀌면(Play·Stop·타임라인 교체) 작성 클립을 다시 찾는다.
			if (timeline.boundClipSerial != timeline.playSerial)
			{
				timeline.boundClip = ResolveTimelineClip(world, FindAnimatorEntityInHierarchy(world, entity), *asset);
				timeline.boundClipSerial = timeline.playSerial;
			}

			const float duration = std::max(0.05f, asset->GetDuration());
			// 세션 첫 틱만 시작 시점(0)을 포함한다. 이후에는 지난 틱에서 처리한 시점(prevTime)을 다시 발동하지 않는다.
			const bool isFirstTick = timeline.playElapsed <= 0.0f;
			timeline.prevTime = timeline.currentTime;
			timeline.currentTime += dt * timeline.playRate;
			timeline.playElapsed += dt * timeline.playRate;

			bool hasLooped = false;
			if (timeline.currentTime >= duration)
			{
				if (timeline.isLooping)
				{
					timeline.currentTime = std::fmod(timeline.currentTime, duration);
					hasLooped = true;
				}
				else
				{
					timeline.currentTime = duration;
					timeline.isPlaying = false;
				}
			}

			// 2. Animator 구동: 작성 클립이 있을 때만 넘겨받는다. 클립이 없는 타임라인(사운드·카메라 전용)은 Animator를 건드리지 않는다.
			// Animator는 자신 또는 자식 모델 루트에서 찾는다.
			const entt::entity animatorEntity = FindAnimatorEntityInHierarchy(world, entity);
			auto* animator = animatorEntity != entt::null ? world.try_get<AnimatorComponent>(animatorEntity) : nullptr;
			const auto* clip = timeline.boundClip.Get();
			const auto* skeleton = (animator && clip) ? FindSkeletonInHierarchy(world, animatorEntity) : nullptr;
			if (animator && clip && skeleton)
			{
				// 계층이 바뀌어 다른 Animator를 찾았으면 이전 Animator를 먼저 되돌린다.
				if (timeline.animatorOverrideEntity != animatorEntity) EndAnimatorOverride(world, timeline, asset->GetBlendOutTime());
				BeginAnimatorOverride(timeline, animatorEntity, *animator);
				animator->isPlaying = false; // 타임라인이 시간을 구동하므로 AnimationSystem의 자체 진행을 멈춘다
				animator->currentClip = timeline.boundClip;
				animator->currentTime = timeline.currentTime;
				// isPlaying이 false이면 AnimationSystem이 포즈를 계산하지 않으므로 여기서 계산한다. 소켓 위치도 이 행렬을 쓰므로 노티파이 검출보다 먼저 갱신한다.
				AnimationEvaluator::EvaluateLocalPose(*clip, *skeleton, timeline.currentTime, animator->localBonePose, timeline.isLooping);
				AnimationEvaluator::BuildSkinningMatrices(*skeleton, animator->localBonePose, animator->finalBoneMatrices);
			}
			else
			{
				EndAnimatorOverride(world, timeline, asset->GetBlendOutTime());
			}

			// 3. 노티파이 이벤트 구간 검출 및 트리거
			const auto& notifyTracks = asset->GetNotifyTracks();
			for (const auto& track : notifyTracks)
			{
				if (track.IsMuted) continue;
				for (const auto& notify : track.Notifies)
				{
					// 구간 (from, to]을 지났는지 본다. 경계 시점이 두 틱에 걸쳐 두 번 발동하지 않도록 아래쪽 끝은 첫 틱(또는 루프 후 0)만 포함한다.
					const float t = notify.TriggerTime;
					const auto crossed = [t](float from, float to, bool includeFrom) { return (includeFrom ? t >= from : t > from) && t <= to; };
					const bool triggered = hasLooped
						? (crossed(timeline.prevTime, duration, isFirstTick) || crossed(0.0f, timeline.currentTime, true))
						: crossed(timeline.prevTime, timeline.currentTime, isFirstTick);

					if (triggered)
					{
						// (1) 등록된 C++ / 스크립트 게임플레이 콜백 실행
						for (const auto& cb : timeline.notifyCallbacks) { if (cb) cb(entity, notify); }

						// (1-1) 이벤트 노티파이: 씬 갱신이 끝난 뒤 ActionTimelineEvents 구독자에게 발행한다.
						if (notify.Type == EAnimNotifyType::CustomEvent && publishGameplayEvents)
							pendingEvents.push_back({ scene, entity, notify.EventTag, notify.Name, notify.TriggerTime });

						// (2) 소리 노티파이 (Audio FX)
						if (notify.Type == EAnimNotifyType::Sound && !notify.SoundPath.empty())
						{
							auto audioHandle = ResourceManager::Get().LoadFile(StrToWStr(notify.SoundPath));
							if (auto* clip = dynamic_cast<AudioClip*>(ResourceManager::Get().GetResource(audioHandle)))
							{
								if (auto* snd = clip->GetFMODSound())
								{
									AudioManager::Get().PlayOneShot(snd, notify.SoundVolume);
								}
							}
						}

						// (3) 파티클 노티파이 (Particle FX 위치 계산)
						if (notify.Type == EAnimNotifyType::Particle)
						{
							vec3 socketWorldPos = ComputeSocketWorldPos(world, entity, notify.SocketBoneName, notify.OffsetPosition);
							// 추후 풀링된 파티클 이미터 또는 파티클 매니저가 있을 경우 socketWorldPos 위치에 버스트 생성
						}

						// (4) [신설] 복합 VFX 노티파이 (SpawnCompositeVFX)
						if (notify.Type == EAnimNotifyType::SpawnCompositeVFX && !notify.VFXPath.empty())
						{
							auto vfxHandle = ResourceManager::Get().LoadFile(StrToWStr(notify.VFXPath));
							if (vfxHandle.IsValid())
							{
								const auto xform = ComputeSocketWorldTransform(world, entity, notify.SocketBoneName, notify.OffsetPosition, notify.OffsetRotation);
								Entity& newEntity = scene->CreateEntity();
								const entt::entity vfxEntity = newEntity.GetEntityHandle();

								auto& nameComp = world.get_or_emplace<NameComponent>(vfxEntity);
								nameComp.name = StrToWStr(notify.Name.empty() ? "Spawned_CompositeVFX" : (notify.Name + "_VFX"));

								auto& trans = world.get_or_emplace<TransformComponent>(vfxEntity);
								trans.SetLocalPosition(xform.position);
								trans.rotation = xform.rotation;
								trans.SetEulerRotation(glm::degrees(glm::eulerAngles(xform.rotation)));
								trans.SetLocalScale(notify.VFXScale);

								world.get_or_emplace<WorldTransformComponent>(vfxEntity);
								auto& flag = world.get_or_emplace<FlagComponent>(vfxEntity);
								flag.flags = EEntityFlag::Active | EEntityFlag::Visible;

								auto& vfxComp = world.get_or_emplace<CompositeVFXComponent>(vfxEntity);
								vfxComp.effectHandle = ResourceHandle<CompositeEffect>(vfxHandle);
								vfxComp.isPlaying = true;
								vfxComp.isLooping = false;
								vfxComp.playOnce = true;
								vfxComp.holdUntilRelease = notify.Duration > 0.0f;
								vfxComp.playRate = notify.VFXPlayRate;
								vfxComp.Restart();

								if (notify.AttachToSocket)
								{
									auto& attach = world.get_or_emplace<SocketAttachmentComponent>(vfxEntity);
									attach.targetEntity = entity;
									attach.boneName = notify.SocketBoneName;
									attach.offsetPosition = notify.OffsetPosition;
									attach.offsetRotation = notify.OffsetRotation;
									attach.scale = notify.VFXScale;
								}

								if (notify.AutoDestroy)
								{
									world.get_or_emplace<ActionVFXLifecycleComponent>(vfxEntity);
								}

								if (vfxComp.holdUntilRelease)
								{
									auto& hold = world.get_or_emplace<ActionVFXHoldComponent>(vfxEntity);
									hold.ownerEntity = entity;
									hold.playSerial = timeline.playSerial;
									hold.releaseElapsed = timeline.playElapsed + notify.Duration;
								}
							}
						}
					}
				}
			}

			// 4. 카메라 샷 연출 (Camera Shots)
			const auto& cameraTracks = asset->GetCameraTracks();
			for (const auto& camShot : cameraTracks)
			{
				if (!camShot.IsActive) continue;
				const float shotStart = camShot.StartTime;
				const float shotEnd = camShot.StartTime + camShot.Duration;

				if (timeline.currentTime >= shotStart && timeline.currentTime <= shotEnd)
				{
					const float progress = (camShot.Duration > 0.0f) ? (timeline.currentTime - shotStart) / camShot.Duration : 1.0f;
					for (const auto& cb : timeline.shotCallbacks) { if (cb) cb(entity, camShot, progress); }

					// 씬의 메인 카메라 검색 및 FOV / Shake 보정
					auto camView = world.view<CameraComponent>();
					for (auto camEntity : camView)
					{
						auto& cam = camView.get<CameraComponent>(camEntity);
						if (cam.IsMainCamera)
						{
							// 목표 FOV로 부드럽게 보간
							cam.FOV = glm::mix(cam.FOV, camShot.TargetFOV, dt * 8.0f);
							cam.InvalidateCache();
						}
					}
				}
			}
		}

		// ── 5. 지속형 VFX 노티파이 해제 ──
		// 소유자가 사라지거나 비활성화될 때, 타임라인을 다시 재생·정지·교체할 때, 끝 시점을 지나거나 재생이 끝났을 때 해제한다.
		// 일시정지(Pause·Paused 플래그) 중에는 유지한다.
		{
			auto holdView = world.view<ActionVFXHoldComponent, CompositeVFXComponent>();
			std::vector<entt::entity> released;
			for (auto vfxEntity : holdView)
			{
				const auto& hold = holdView.get<ActionVFXHoldComponent>(vfxEntity);
				const auto* owner = world.valid(hold.ownerEntity) ? world.try_get<ActionTimelineComponent>(hold.ownerEntity) : nullptr;
				const auto* ownerFlag = owner ? world.try_get<FlagComponent>(hold.ownerEntity) : nullptr;
				const bool ownerGone = !owner || (ownerFlag && (!ownerFlag->IsActiveInHierarchy() || HasFlag(ownerFlag->effectiveFlags, EEntityFlag::Dead)));
				const auto* ownerAsset = owner ? owner->timelineAsset.Get() : nullptr;
				const bool ownerEnded = owner && !owner->isPlaying && ownerAsset && owner->currentTime >= std::max(0.05f, ownerAsset->GetDuration());
				if (ownerGone || ownerEnded || owner->playSerial != hold.playSerial || owner->playElapsed >= hold.releaseElapsed)
					released.push_back(vfxEntity);
			}
			for (auto vfxEntity : released)
			{
				world.get<CompositeVFXComponent>(vfxEntity).Release();
				world.remove<ActionVFXHoldComponent>(vfxEntity);
			}
		}

		// ── 6. 이벤트 노티파이 발행 ──
		// 앞선 핸들러가 소유 엔티티를 지웠을 수 있으므로 발행 직전에 확인한다.
		for (const auto& event : pendingEvents)
		{
			if (world.valid(event.entity)) ActionTimelineEvents::Publish(event);
		}
	}
}