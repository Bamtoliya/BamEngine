#include "ActionTimelineSystem.h"
#include "ComponentEnable.h"
#include "ActionTimelineComponent.h"
#include "AnimatorComponent.h"
#include "SkinnedMeshRendererComponent.h"
#include "CameraComponent.h"
#include "TransformComponent.h"
#include "TransformSystem.h"
#include "WorldTransformComponent.h"
#include "FlagComponent.h"
#include "HierarchyComponent.h"
#include "HitboxComponent.h"
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
#include "ParticleEffect.h"
#include "NameComponent.h"
#include "AnimationEvaluator.h"
#include "Animation.h"
#include "Model.h"
#include "ModelInstanceComponent.h"
#include "Logger.h"
#include "ActionTimelineEvents.h"
#include "ActionTimelinePlayback.h"
#include "SystemManager.h"
#include <chrono>
#include <filesystem>
#include <tuple>
#include <unordered_map>

namespace
{
	enum class TimelineResourceLoadStage { ScenePreparation, Playback };
	using TimelineClock = std::chrono::steady_clock;

	void LogSlowTimelineWork(TimelineClock::time_point start, std::string_view operation, const std::string& path)
	{
		const double elapsedMs = std::chrono::duration<double, std::milli>(TimelineClock::now() - start).count();
		if (elapsedMs >= 2.0)
			BAM_LOG(Info, "Engine", "ActionTimeline", "{} took {:.2f} ms: {}", operation, elapsedMs, path);
	}

	Handle LoadTimelineResource(const std::string& path, TimelineResourceLoadStage stage)
	{
		const auto start = TimelineClock::now();
		const auto handle = ResourceManager::Get().LoadFile(path);
		LogSlowTimelineWork(start, stage == TimelineResourceLoadStage::ScenePreparation ? "Scene preparation load" : "Playback load", path);
		return handle;
	}

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
		const auto* animator = animatorEntity != entt::null ? TryGetEnabledComponent<AnimatorComponent>(world, animatorEntity) : nullptr;
		const auto* skeleton = animator ? FindEnabledSkeletonInHierarchy(world, animatorEntity) : nullptr;
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

	ResourceHandle<Animation> FindTargetModelClip(const entt::registry& world, entt::entity animatorEntity, const std::wstring& clipName)
	{
		const auto* instance = animatorEntity != entt::null ? world.try_get<ModelInstanceComponent>(animatorEntity) : nullptr;
		return instance ? FindClipByName(instance->modelHandle.Get(), clipName) : ResourceHandle<Animation>{};
	}

	// 타임라인을 작성한 클립을 찾는다. Animator 엔티티(모델 루트)의 연결 모델을 먼저 보고, 없으면 타임라인에 저장된 작성 모델에서 찾는다.
	// 클립은 스켈레톤 기준이므로 같은 스켈레톤을 쓰는 다른 모델에도 적용할 수 있다.
	ResourceHandle<Animation> ResolveTimelineClip(const entt::registry& world, entt::entity animatorEntity, const ActionTimelineAsset& asset)
	{
		if (asset.GetClipName().empty()) return {};
		const std::wstring clipName = StrToWStr(asset.GetClipName());

		auto clip = FindTargetModelClip(world, animatorEntity, clipName);
		if (!clip.IsValid() && !asset.GetModelPath().empty())
		{
			const ResourceHandle<Model> authoringModel(LoadTimelineResource(asset.GetModelPath(), TimelineResourceLoadStage::Playback));
			const auto authoringSkeletonHandle = authoringModel.IsValid() ? authoringModel->GetSkeleton() : ResourceHandle<Skeleton>();
			const auto* targetSkeleton = animatorEntity != entt::null ? FindEnabledSkeletonInHierarchy(world, animatorEntity) : nullptr;
			const auto* authoringSkeleton = authoringSkeletonHandle.Get();
			if (targetSkeleton && authoringSkeleton &&
				targetSkeleton->GetSkinBindingSignature() != authoringSkeleton->GetSkinBindingSignature())
			{
				BAM_LOG(Warn, "Engine", "ActionTimeline",
					"Timeline '{}' resolves clip '{}' from an authoring model with a different skeleton binding signature.",
					WStrToStr(asset.GetPath()), asset.GetClipName());
			}
			clip = FindClipByName(authoringModel.Get(), clipName);
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
		const auto* skeleton = animatorEntity != entt::null ? FindEnabledSkeletonInHierarchy(world, animatorEntity) : nullptr;
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

	void WarnOutOfRangeNotifies(const ActionTimelineAsset& asset)
	{
		const float duration = std::max(0.05f, asset.GetDuration());
		for (const auto& track : asset.GetNotifyTracks())
		{
			for (const auto& notify : track.Notifies)
			{
				if (!std::isfinite(notify.TriggerTime) || notify.TriggerTime < 0.0f || notify.TriggerTime > duration ||
					!std::isfinite(notify.Duration) || notify.Duration < 0.0f ||
					(notify.Duration > 0.0f && notify.TriggerTime + notify.Duration > duration))
				{
					BAM_LOG(Warn, "Engine", "ActionTimeline",
						"Notify '{}' is outside timeline duration ({:.3f}s).", notify.Name, duration);
				}
			}
		}
	}

	struct ActionTimelineDestroyHookTag {};

	// 소유 타임라인이 유지 중이던 지속형 VFX를 즉시 해제한다.
	void ReleaseHeldVFXForOwner(entt::registry& world, entt::entity ownerEntity)
	{
		std::vector<entt::entity> released;
		auto holdView = world.view<ActionVFXHoldComponent, CompositeVFXComponent>();
		for (auto vfxEntity : holdView)
		{
			if (holdView.get<ActionVFXHoldComponent>(vfxEntity).ownerEntity == ownerEntity)
				released.push_back(vfxEntity);
		}
		for (auto vfxEntity : released)
		{
			if (!world.valid(vfxEntity) || !world.all_of<ActionVFXHoldComponent, CompositeVFXComponent>(vfxEntity))
				continue;
			world.get<CompositeVFXComponent>(vfxEntity).Release();
			world.remove<ActionVFXHoldComponent>(vfxEntity);
		}
	}

	// 넘겨받은 Animator를 보관한 상태로 되돌리고, 타임라인의 마지막 포즈에서 원래 애니메이션으로 섞어 넘어간다.
	// 이미 다른 소유자에게 넘어갔거나 Animator가 파괴된 경우에는 건드리지 않는다.
	void EndAnimatorOverride(entt::registry& world, entt::entity ownerEntity, ActionTimelineComponent& timeline, float blendOutTime)
	{
		const entt::entity animatorEntity = timeline.animatorOverrideEntity;
		if (animatorEntity == entt::null) return;
		timeline.animatorOverrideEntity = entt::null;

		auto* animator = world.valid(animatorEntity) ? world.try_get<AnimatorComponent>(animatorEntity) : nullptr;
		if (!animator || animator->timelineOverrideOwner != ownerEntity ||
			animator->timelineOverrideSerial != timeline.animatorOverrideSerial) return;

		animator->timelineOverrideOwner = entt::null;
		animator->timelineOverrideSerial = 0;
		if (animator->timelineSavedPlaying)
			animator->BeginBlendFromPose(animator->localBonePose, blendOutTime);
		animator->currentClip = animator->timelineSavedClip;
		animator->currentTime = animator->timelineSavedTime;
		animator->isPlaying = animator->timelineSavedPlaying;
		animator->timelineSavedClip = {};
		animator->timelineSavedTime = 0.0f;
		animator->timelineSavedPlaying = false;
	}

	void OnActionTimelineComponentDestroy(entt::registry& world, entt::entity entity)
	{
		auto& timeline = world.get<ActionTimelineComponent>(entity);
		const auto* asset = timeline.timelineAsset.Get();
		const float blendOutTime = asset ? asset->GetBlendOutTime() : ActionTimelineAsset::DefaultBlendOutTime;
		EndAnimatorOverride(world, entity, timeline, blendOutTime);
		ReleaseHeldVFXForOwner(world, entity);
		if (auto* hitbox = world.try_get<HitboxComponent>(entity))
		{
			hitbox->SetActive(false);
		}
	}

	void EnsureDestroyHook(entt::registry& world)
	{
		if (world.ctx().contains<ActionTimelineDestroyHookTag>()) return;
		world.ctx().emplace<ActionTimelineDestroyHookTag>();
		world.on_destroy<ActionTimelineComponent>().connect<&OnActionTimelineComponentDestroy>();
	}

	// 타임라인이 Animator를 넘겨받는다. 처음 넘겨받을 때만 원래 상태를 Animator에 저장하고,
	// 다른 타임라인에서 인계받는 경우에는 기존 원래 상태를 덮어쓰지 않고 소유권만 넘긴다.
	void BeginAnimatorOverride(entt::registry& world, entt::entity ownerEntity, ActionTimelineComponent& timeline,
		entt::entity animatorEntity, AnimatorComponent& animator)
	{
		EnsureDestroyHook(world);

		if (animator.timelineOverrideOwner != entt::null && animator.timelineOverrideOwner != ownerEntity)
		{
			if (world.valid(animator.timelineOverrideOwner))
			{
				if (auto* prevTimeline = world.try_get<ActionTimelineComponent>(animator.timelineOverrideOwner))
				{
					if (prevTimeline->animatorOverrideEntity == animatorEntity)
					{
						prevTimeline->animatorOverrideEntity = entt::null;
						prevTimeline->isPlaying = false;
						prevTimeline->notifyStartPending = true;
						++prevTimeline->playSerial;
						ReleaseHeldVFXForOwner(world, animator.timelineOverrideOwner);
					}
				}
			}
		}
		else if (animator.timelineOverrideOwner == entt::null)
		{
			animator.timelineSavedClip = animator.currentClip;
			animator.timelineSavedTime = animator.currentTime;
			animator.timelineSavedPlaying = animator.isPlaying;
		}

		animator.timelineOverrideOwner = ownerEntity;
		animator.timelineOverrideSerial = timeline.playSerial;
		timeline.animatorOverrideEntity = animatorEntity;
		timeline.animatorOverrideSerial = timeline.playSerial;
	}

	ResourceHandle<CompositeEffect> GetOrCreateNotifyParticleEffect(const AnimNotifyData& notify)
	{
		if (!ResourceManager::IsCreated() || notify.BurstCount == 0) return {};

		const auto quantize = [](float value, float scale) -> long long
		{
			return std::isfinite(value) ? std::llround(static_cast<double>(value) * static_cast<double>(scale)) : 0LL;
		};
		const uint32 clampedBurst = std::max<uint32>(notify.BurstCount, 1u);
		const float clampedSize = std::max(0.01f, notify.ParticleSize);
		const float clampedSpeed = std::max(0.0f, notify.ParticleSpeed);
		const float clampedLifetime = std::max(0.05f, notify.ParticleLifetime);

		const std::wstring keySuffix = std::to_wstring(clampedBurst) + L"_" +
			std::to_wstring(quantize(clampedSize, 1000.0f)) + L"_" +
			std::to_wstring(quantize(clampedSpeed, 1000.0f)) + L"_" +
			std::to_wstring(quantize(clampedLifetime, 1000.0f)) + L"_" +
			std::to_wstring(quantize(notify.ParticleColor.r, 255.0f)) + L"_" +
			std::to_wstring(quantize(notify.ParticleColor.g, 255.0f)) + L"_" +
			std::to_wstring(quantize(notify.ParticleColor.b, 255.0f)) + L"_" +
			std::to_wstring(quantize(notify.ParticleColor.a, 255.0f));

		const std::wstring particleKey = L"__ActionTimelineParticleEffect_" + keySuffix;
		const std::wstring compositeKey = L"__ActionTimelineParticleComposite_" + keySuffix;

		auto& rm = ResourceManager::Get();
		if (auto existing = rm.GetResourceHandle<CompositeEffect>(compositeKey); existing.IsValid())
		{
			return existing;
		}

		auto particleHandle = rm.GetResourceHandle<ParticleEffect>(particleKey);
		if (!particleHandle.IsValid())
		{
			ParticleEmitterData emitter;
			emitter.name = "NotifyBurst";
			emitter.triggerMode = EParticleEmitterTrigger::Continuous;
			emitter.maxParticles = clampedBurst;
			emitter.emissionRate = 0.0f;
			emitter.burstCount = clampedBurst;
			emitter.burstInterval = 0.001f;
			emitter.lifetimeMax = clampedLifetime;
			emitter.lifetimeMin = std::max(0.05f, clampedLifetime * 0.8f);
			emitter.speedMax = clampedSpeed;
			emitter.speedMin = std::max(0.0f, clampedSpeed * 0.7f);
			emitter.spreadAngleDegrees = 180.0f;
			emitter.sizeStart = vec2(clampedSize);
			emitter.sizeEnd = vec2(std::max(0.005f, clampedSize * 0.25f));
			emitter.colorStart = notify.ParticleColor;
			emitter.colorEnd = vec4(notify.ParticleColor.r, notify.ParticleColor.g, notify.ParticleColor.b, 0.0f);

			ParticleEffectCreateDesc particleDesc;
			particleDesc.emitters.push_back(emitter);
			particleHandle = rm.AddResource(particleKey, ParticleEffect::Create(&particleDesc));
		}
		if (!particleHandle.IsValid()) return {};

		CompositeEffectCreateDesc effectDesc;
		effectDesc.totalDuration = 0.06f;
		auto effectHandle = rm.AddResource(compositeKey, CompositeEffect::Create(&effectDesc));
		if (!effectHandle.IsValid()) return {};

		VFXParticleTrack track;
		track.trackName = "NotifyBurst";
		track.startTime = 0.0f;
		track.duration = 0.06f;
		track.effectHandle = particleHandle;
		effectHandle->GetParticleTracks().push_back(track);
		return effectHandle;
	}

	entt::entity SpawnTimelineVFXEntity(Scene& scene, entt::registry& world, const std::wstring& entityName,
		const SocketWorldTransform& xform, const vec3& scale, const ResourceHandle<CompositeEffect>& effectHandle,
		float playRate, bool holdUntilRelease, bool autoDestroy)
	{
		Entity& newEntity = scene.CreateEntity();
		const entt::entity vfxEntity = newEntity.GetEntityHandle();

		world.get_or_emplace<NameComponent>(vfxEntity).name = entityName;

		auto& trans = world.get_or_emplace<TransformComponent>(vfxEntity);
		std::ignore = trans.SetLocalPosition(xform.position);
		trans.rotation = xform.rotation;
		std::ignore = trans.SetEulerRotation(glm::degrees(glm::eulerAngles(xform.rotation)));
		std::ignore = trans.SetLocalScale(scale);

		auto& worldTrans = world.get_or_emplace<WorldTransformComponent>(vfxEntity);
		TransformSystem::UpdateRootWorld(trans, worldTrans);

		auto& flag = world.get_or_emplace<FlagComponent>(vfxEntity);
		flag.flags = EEntityFlag::Active | EEntityFlag::Visible;
		flag.effectiveFlags = flag.flags;

		auto& vfxComp = world.get_or_emplace<CompositeVFXComponent>(vfxEntity);
		vfxComp.effectHandle = effectHandle;
		vfxComp.isPlaying = true;
		vfxComp.isLooping = false;
		vfxComp.playOnce = true;
		vfxComp.holdUntilRelease = holdUntilRelease;
		vfxComp.playRate = playRate;
		vfxComp.Restart();

		if (autoDestroy)
		{
			world.get_or_emplace<ActionVFXLifecycleComponent>(vfxEntity);
		}
		return vfxEntity;
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

void ActionTimelineSystem::PrepareSceneResources(const Scene& scene)
{
	if (!ResourceManager::IsCreated()) return;

	const auto& world = scene.GetRegistry();
	std::unordered_map<std::string, Handle> preparedPaths;
	const auto prepare = [&preparedPaths](const std::string& path) -> Handle
	{
		if (path.empty()) return {};
		const auto [entry, inserted] = preparedPaths.try_emplace(path);
		if (inserted)
		{
			try
			{
				entry->second = LoadTimelineResource(path, TimelineResourceLoadStage::ScenePreparation);
			}
			catch (const std::filesystem::filesystem_error& error)
			{
				BAM_LOG(Warn, "Engine", "ActionTimeline", "Scene preparation could not load '{}': {}", path, error.what());
			}
		}
		return entry->second;
	};

	for (auto [entity, timeline] : world.view<const ActionTimelineComponent>().each())
	{
		if (!IsComponentEnabled<ActionTimelineComponent>(world, entity) || timeline.timelinePath.empty()) continue;
		const auto handle = prepare(timeline.timelinePath);
		const auto* asset = ResourceHandle<ActionTimelineAsset>(handle).Get();
		if (!asset) continue;

		if (!asset->GetClipName().empty() && !asset->GetModelPath().empty())
		{
			const auto animator = FindAnimatorEntityInHierarchy(world, entity);
			if (!FindTargetModelClip(world, animator, StrToWStr(asset->GetClipName())).IsValid())
				prepare(asset->GetModelPath());
		}

		for (const auto& track : asset->GetNotifyTracks())
		{
			if (track.IsMuted) continue;
			for (const auto& notify : track.Notifies)
			{
				if (notify.Type == EAnimNotifyType::Sound)
					prepare(notify.SoundPath);
				else if (notify.Type == EAnimNotifyType::SpawnCompositeVFX)
					prepare(notify.VFXPath);
			}
		}
	}
}

void ActionTimelineSystem::OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
	(void)registry;
	if (!std::isfinite(dt) || dt <= 0.0f) return;

	// 편집 모드 뷰포트 Realtime(Presentation)에서는 실제 씬의 타임라인 진행·VFX 생성·카메라 FOV 변경을 막는다.
	if (SystemManager::IsCreated() &&
		(SystemManager::Get().GetUpdatePhaseMask() & static_cast<uint8>(ESystemPhase::Gameplay)) == 0)
		return;

	for (Scene* scene : activeScenes)
	{
		if (!scene || !scene->IsActive() || scene->IsPaused()) continue;
		auto& world = scene->GetRegistry();
		EnsureDestroyHook(world);

		// 타임라인 순회 중에는 모으기만 하고, 씬의 갱신이 끝난 뒤 같은 프레임에 발행한다(핸들러가 엔티티를 바꿔도 순회가 깨지지 않게).
		std::vector<ActionTimelineEvent> pendingEvents;

		// ── 0. 소켓 부착(AttachToSocket) 엔티티 위치 및 회전 실시간 동기화 ──
		{
			auto attachView = world.view<SocketAttachmentComponent, TransformComponent>();
			std::vector<entt::entity> deadAttachments;

			for (auto attachEntity : attachView)
			{
				if (!IsComponentEnabled<SocketAttachmentComponent>(world, attachEntity)) continue;
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
				if (!IsComponentEnabled<ActionVFXLifecycleComponent, CompositeVFXComponent>(world, vfxEntity)) continue;
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
			auto& timeline = view.get<ActionTimelineComponent>(entity);
			const auto* flag = world.try_get<FlagComponent>(entity);
			if (!IsComponentEnabled<ActionTimelineComponent>(world, entity) ||
				(flag && (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, EEntityFlag::Dead))))
			{
				const auto* existingAsset = timeline.timelineAsset.Get();
				const float blendOutTime = existingAsset ? existingAsset->GetBlendOutTime() : ActionTimelineAsset::DefaultBlendOutTime;
				EndAnimatorOverride(world, entity, timeline, blendOutTime);
				ReleaseHeldVFXForOwner(world, entity);
				continue;
			}
			if (flag && HasFlag(flag->effectiveFlags, EEntityFlag::Paused))
			{
				continue;
			}

			// 1. 에셋 경로 변경 시 자동 로드 동기화
			if (timeline.timelinePath != timeline.loadedTimelinePath)
			{
				timeline.loadedTimelinePath = timeline.timelinePath;
				++timeline.playSerial; // 다른 타임라인으로 바뀌면 이전 지속형 VFX를 해제한다
				timeline.currentTime = 0.0f;
				timeline.prevTime = 0.0f;
				timeline.playElapsed = 0.0f;
				timeline.notifyStartPending = true;
				if (!timeline.timelinePath.empty())
				{
					auto handle = LoadTimelineResource(timeline.timelinePath, TimelineResourceLoadStage::Playback);
					timeline.timelineAsset = ResourceHandle<ActionTimelineAsset>(handle);
					if (const auto* loaded = timeline.timelineAsset.Get())
					{
						WarnMissingSocketBones(world, entity, *loaded);
						WarnOutOfRangeNotifies(*loaded);
					}
				}
				else
				{
					timeline.timelineAsset = {};
				}
			}

			auto* asset = timeline.timelineAsset.Get();
			const entt::entity animatorEntity = asset && (timeline.isPlaying || timeline.animatorOverrideEntity != entt::null)
				? FindAnimatorEntityInHierarchy(world, entity) : entt::entity(entt::null);

			// 세션이 끝났으면 Animator를 타임라인 이전 상태로 되돌린다. 일시정지(같은 세션, 끝나기 전)는 마지막 포즈를 유지한다.
			if (timeline.animatorOverrideEntity != entt::null)
			{
				const bool sessionChanged = timeline.animatorOverrideSerial != timeline.playSerial;
				const bool finished = asset && timeline.currentTime >= std::max(0.05f, asset->GetDuration());
				if (!asset || timeline.animatorOverrideEntity != animatorEntity ||
					(!timeline.isPlaying && (sessionChanged || finished)))
					EndAnimatorOverride(world, entity, timeline, asset ? asset->GetBlendOutTime() : ActionTimelineAsset::DefaultBlendOutTime);
			}
			if (!asset || !timeline.isPlaying) continue;

			// 재생 세션이나 활성 Animator 대상이 바뀌면 작성 클립을 다시 찾는다.
			if (timeline.boundClipSerial != timeline.playSerial || timeline.boundClipAnimator != animatorEntity)
			{
				timeline.boundClip = ResolveTimelineClip(world, animatorEntity, *asset);
				timeline.boundClipSerial = timeline.playSerial;
				timeline.boundClipAnimator = animatorEntity;
			}

			const float duration = std::max(0.05f, asset->GetDuration());
			timeline.prevTime = timeline.currentTime;
			const auto playbackStep = AdvanceActionTimeline(timeline.currentTime, dt * timeline.playRate, duration,
				timeline.isLooping, timeline.notifyStartPending);
			timeline.currentTime = playbackStep.CurrentTime;
			timeline.playElapsed += dt * timeline.playRate;
			timeline.notifyStartPending = false;
			if (playbackStep.Finished) timeline.isPlaying = false;

			// 2. Animator 구동: 작성 클립이 있을 때만 넘겨받는다. 클립이 없는 타임라인(사운드·카메라 전용)은 Animator를 건드리지 않는다.
			// Animator는 자신 또는 자식 모델 루트에서 찾는다.
			auto* animator = animatorEntity != entt::null ? TryGetEnabledComponent<AnimatorComponent>(world, animatorEntity) : nullptr;
			const auto* clip = timeline.boundClip.Get();
			const auto* skeleton = (animator && clip) ? FindEnabledSkeletonInHierarchy(world, animatorEntity) : nullptr;
			if (animator && clip && skeleton)
			{
				BeginAnimatorOverride(world, entity, timeline, animatorEntity, *animator);
				animator->isPlaying = false; // 타임라인이 시간을 구동하므로 AnimationSystem의 자체 진행을 멈춘다
				animator->currentClip = timeline.boundClip;
				animator->currentTime = timeline.currentTime;
				// isPlaying이 false이면 AnimationSystem이 포즈를 계산하지 않으므로 여기서 계산한다. 소켓 위치도 이 행렬을 쓰므로 노티파이 검출보다 먼저 갱신한다.
				AnimationEvaluator::EvaluateLocalPose(*clip, *skeleton, timeline.currentTime, animator->localBonePose, timeline.isLooping);
				AnimationEvaluator::BuildSkinningMatrices(*skeleton, animator->localBonePose, animator->finalBoneMatrices);
			}
			else
			{
				EndAnimatorOverride(world, entity, timeline, asset->GetBlendOutTime());
			}

			// 3. 노티파이 이벤트 구간 검출 및 트리거
			const auto notifyPlaySerial = timeline.playSerial;
			const auto& notifyTracks = asset->GetNotifyTracks();
			for (const auto& track : notifyTracks)
			{
				if (track.IsMuted) continue;
				for (const auto& notify : track.Notifies)
				{
					const std::size_t crossingCount = CountActionTimelineCrossings(playbackStep, notify.TriggerTime);
					for (std::size_t crossing = 0; crossing < crossingCount; ++crossing)
					{
						// (1) 등록된 C++ / 스크립트 게임플레이 콜백 실행
						for (const auto& cb : timeline.notifyCallbacks) { if (cb) cb(entity, notify); }

						// (1-1) 이벤트 노티파이: 씬 갱신이 끝난 뒤 ActionTimelineEvents 구독자에게 발행한다.
						if (notify.Type == EAnimNotifyType::CustomEvent)
							pendingEvents.push_back({ scene, entity, notify.EventTag, notify.Name, notify.TriggerTime, notifyPlaySerial });

						// (1-2) 히트박스 노티파이 창 (HitboxOn / HitboxOff)
						const bool isHitboxOn = (notify.EventTag == "HitboxOn" || notify.EventTag == "Hitbox.Enable" ||
							notify.Name == "HitboxOn" || notify.Name == "Hitbox.Enable");
						const bool isHitboxOff = (notify.EventTag == "HitboxOff" || notify.EventTag == "Hitbox.Disable" ||
							notify.Name == "HitboxOff" || notify.Name == "Hitbox.Disable");
						if (isHitboxOn)
						{
							if (auto* hitbox = world.try_get<HitboxComponent>(entity))
							{
								hitbox->SetActive(true);
							}
							if (const auto* hier = world.try_get<HierarchyComponent>(entity))
							{
								for (entt::entity child : hier->children)
								{
									if (world.valid(child))
									{
										if (auto* childHitbox = world.try_get<HitboxComponent>(child))
											childHitbox->SetActive(true);
									}
								}
							}
						}
						else if (isHitboxOff)
						{
							if (auto* hitbox = world.try_get<HitboxComponent>(entity))
							{
								hitbox->SetActive(false);
							}
							if (const auto* hier = world.try_get<HierarchyComponent>(entity))
							{
								for (entt::entity child : hier->children)
								{
									if (world.valid(child))
									{
										if (auto* childHitbox = world.try_get<HitboxComponent>(child))
											childHitbox->SetActive(false);
									}
								}
							}
						}

						// (2) 소리 노티파이 (Audio FX)
						if (notify.Type == EAnimNotifyType::Sound && !notify.SoundPath.empty())
						{
							auto audioHandle = LoadTimelineResource(notify.SoundPath, TimelineResourceLoadStage::Playback);
							if (auto* clip = dynamic_cast<AudioClip*>(ResourceManager::Get().GetResource(audioHandle)))
							{
								if (auto* snd = clip->GetFMODSound())
								{
									const auto start = TimelineClock::now();
									AudioManager::Get().PlayOneShot(snd, notify.SoundVolume);
									LogSlowTimelineWork(start, "FMOD playSound", notify.SoundPath);
								}
							}
						}

						// (3) 파티클 노티파이 (Particle FX 버스트 스폰)
						if (notify.Type == EAnimNotifyType::Particle)
						{
							const vec3 socketWorldPos = ComputeSocketWorldPos(world, entity, notify.SocketBoneName, notify.OffsetPosition);
							if (auto effectHandle = GetOrCreateNotifyParticleEffect(notify); effectHandle.IsValid())
							{
								const std::wstring name = StrToWStr(notify.Name.empty() ? "Spawned_ParticleNotify" : (notify.Name + "_Particle"));
								SpawnTimelineVFXEntity(*scene, world, name,
									SocketWorldTransform{ socketWorldPos, quat(1.0f, 0.0f, 0.0f, 0.0f) },
									vec3(1.0f), effectHandle, 1.0f, false, true);
							}
						}

						// (4) [신설] 복합 VFX 노티파이 (SpawnCompositeVFX)
						if (notify.Type == EAnimNotifyType::SpawnCompositeVFX && !notify.VFXPath.empty())
						{
							auto vfxHandle = LoadTimelineResource(notify.VFXPath, TimelineResourceLoadStage::Playback);
							if (vfxHandle.IsValid())
							{
								const auto xform = ComputeSocketWorldTransform(world, entity, notify.SocketBoneName, notify.OffsetPosition, notify.OffsetRotation);
								const std::wstring name = StrToWStr(notify.Name.empty() ? "Spawned_CompositeVFX" : (notify.Name + "_VFX"));
								const bool holdUntilRelease = notify.Duration > 0.0f;
								const entt::entity vfxEntity = SpawnTimelineVFXEntity(*scene, world, name, xform, notify.VFXScale,
									ResourceHandle<CompositeEffect>(vfxHandle), notify.VFXPlayRate, holdUntilRelease, notify.AutoDestroy);

								if (notify.AttachToSocket)
								{
									auto& attach = world.get_or_emplace<SocketAttachmentComponent>(vfxEntity);
									attach.targetEntity = entity;
									attach.boneName = notify.SocketBoneName;
									attach.offsetPosition = notify.OffsetPosition;
									attach.offsetRotation = notify.OffsetRotation;
									attach.scale = notify.VFXScale;
								}

								if (holdUntilRelease)
								{
									auto& hold = world.get_or_emplace<ActionVFXHoldComponent>(vfxEntity);
									hold.ownerEntity = entity;
									hold.playSerial = timeline.playSerial;
									hold.releaseElapsed = timeline.playElapsed + notify.Duration;
								}
							}
						}
					}

					// 지속형 히트박스 노티파이 창 (TriggerTime + Duration 끝나는 시점 검출)
					if (notify.Duration > 0.0f)
					{
						const bool isHitboxWindowStart = (notify.EventTag == "HitboxOn" || notify.EventTag == "Hitbox.Enable" ||
							notify.Name == "HitboxOn" || notify.Name == "Hitbox.Enable");
						if (isHitboxWindowStart)
						{
							const std::size_t endCrossings = CountActionTimelineCrossings(playbackStep, notify.TriggerTime + notify.Duration);
							if (endCrossings > 0)
							{
								if (auto* hitbox = world.try_get<HitboxComponent>(entity))
								{
									hitbox->SetActive(false);
								}
								if (const auto* hier = world.try_get<HierarchyComponent>(entity))
								{
									for (entt::entity child : hier->children)
									{
										if (world.valid(child))
										{
											if (auto* childHitbox = world.try_get<HitboxComponent>(child))
												childHitbox->SetActive(false);
										}
									}
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
						if (!IsComponentEnabled<CameraComponent>(world, camEntity)) continue;
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
				if (!IsComponentEnabled<CompositeVFXComponent>(world, vfxEntity)) continue;
				const auto& hold = holdView.get<ActionVFXHoldComponent>(vfxEntity);
				const auto* owner = world.valid(hold.ownerEntity) ? TryGetEnabledComponent<ActionTimelineComponent>(world, hold.ownerEntity) : nullptr;
				const auto* ownerFlag = owner ? world.try_get<FlagComponent>(hold.ownerEntity) : nullptr;
				const bool ownerGone = !owner || (ownerFlag && (!ownerFlag->IsActiveInHierarchy() || HasFlag(ownerFlag->effectiveFlags, EEntityFlag::Dead)));
				const auto* ownerAsset = owner ? owner->timelineAsset.Get() : nullptr;
				const bool ownerEnded = owner && !owner->isPlaying && ownerAsset && owner->currentTime >= std::max(0.05f, ownerAsset->GetDuration());
				if (!IsComponentEnabled<ActionVFXHoldComponent>(world, vfxEntity))
				{
					holdView.get<CompositeVFXComponent>(vfxEntity).Release();
					continue;
				}
				if (ownerGone || ownerEnded || owner->playSerial != hold.playSerial || owner->playElapsed >= hold.releaseElapsed)
					released.push_back(vfxEntity);
				else
				{
					auto& vfx = holdView.get<CompositeVFXComponent>(vfxEntity);
					vfx.timelineTimeScale = std::max(0.0f, owner->playRate);
					vfx.SetTimelinePaused(!owner->isPlaying || vfx.timelineTimeScale == 0.0f ||
						(ownerFlag && HasFlag(ownerFlag->effectiveFlags, EEntityFlag::Paused)));
				}
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
			const auto* timeline = TryGetEnabledComponent<ActionTimelineComponent>(world, event.entity);
			if (timeline && timeline->playSerial == event.playSerial)
				ActionTimelineEvents::Publish(event);
		}
	}
}
