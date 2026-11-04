#include "ActionTimelineSystem.h"
#include "Components/Render/ActionTimelineComponent.h"
#include "Components/Render/AnimatorComponent.h"
#include "Components/Render/SkinnedMeshRendererComponent.h"
#include "Components/Render/CameraComponent.h"
#include "Components/Core/TransformComponent.h"
#include "Components/Core/WorldTransformComponent.h"
#include "Components/Core/FlagComponent.h"
#include "Components/Core/HierarchyComponent.h"
#include "ResourceManager.h"
#include "AudioManager.h"
#include "AudioClip.h"
#include "Scene.h"
#include "PlatformPath.h"
#include <cmath>

namespace Engine
{
	namespace
	{
		// 스켈레톤 탐색 헬퍼
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

		// 본 소켓 월드 좌표 계산 헬퍼
		vec3 ComputeSocketWorldPos(const entt::registry& world, entt::entity entity, const std::string& boneName, const vec3& offset)
		{
			const auto* worldTrans = world.try_get<WorldTransformComponent>(entity);
			const mat4 entityWorldMat = worldTrans ? worldTrans->worldMatrix : mat4(1.0f);
			if (boneName.empty()) return vec3(entityWorldMat * vec4(offset, 1.0f));

			const auto* animator = world.try_get<AnimatorComponent>(entity);
			const auto* skeleton = FindSkeletonInHierarchy(world, entity);
			if (!animator || !skeleton || animator->finalBoneMatrices.empty()) return vec3(entityWorldMat * vec4(offset, 1.0f));

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
					vec4 localPos = combined * vec4(offset, 1.0f);
					return vec3(entityWorldMat * localPos);
				}
			}
			return vec3(entityWorldMat * vec4(offset, 1.0f));
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

		for (Scene* scene : activeScenes)
		{
			if (!scene || !scene->IsActive() || scene->IsPaused()) continue;
			auto& world = scene->GetRegistry();
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
					if (!timeline.timelinePath.empty())
					{
						auto handle = ResourceManager::Get().LoadFile(StrToWStr(timeline.timelinePath));
						timeline.timelineAsset = ResourceHandle<ActionTimelineAsset>(handle);
					}
					else
					{
						timeline.timelineAsset = {};
					}
				}

				auto* asset = timeline.timelineAsset.Get();
				if (!asset || !timeline.isPlaying) continue;

				const float duration = std::max(0.05f, asset->GetDuration());
				timeline.prevTime = timeline.currentTime;
				timeline.currentTime += dt * timeline.playRate;

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

				// 2. AnimatorComponent 동기화
				if (auto* animator = world.try_get<AnimatorComponent>(entity))
				{
					animator->currentTime = timeline.currentTime;
					animator->isPlaying = false; // 타임라인이 시간을 주도적으로 구동하므로 Animator의 자체 dt 가산을 억제
				}

				// 3. 노티파이 이벤트 구간 검출 및 트리거
				const auto& notifyTracks = asset->GetNotifyTracks();
				for (const auto& track : notifyTracks)
				{
					if (track.IsMuted) continue;
					for (const auto& notify : track.Notifies)
					{
						const float t = notify.TriggerTime;
						bool triggered = false;
						if (hasLooped)
						{
							triggered = (t >= timeline.prevTime && t <= duration) || (t >= 0.0f && t <= timeline.currentTime);
						}
						else
						{
							triggered = (t >= timeline.prevTime && t <= timeline.currentTime);
						}

						if (triggered)
						{
							// (1) 등록된 C++ / 스크립트 게임플레이 콜백 실행
							for (const auto& cb : timeline.notifyCallbacks) { if (cb) cb(entity, notify); }

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
		}
	}
}