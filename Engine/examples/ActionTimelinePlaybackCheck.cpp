#include "ActionTimelineAsset.h"
#include "ActionTimelineComponent.h"
#include "ActionTimelinePlayback.h"
#include "ActionTimelineSystem.h"
#include "Animation.h"
#include "AnimationSceneQuery.h"
#include "AnimatorComponent.h"
#include "ComponentEnable.h"
#include "HierarchyComponent.h"
#include "CompositeVFXComponent.h"
#include "CompositeVFXSystem.h"
#include "FlagComponent.h"
#include "Model.h"
#include "ModelInstanceComponent.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "Skeleton.h"
#include "SkinnedMeshRendererComponent.h"
#include "TransformComponent.h"
#include "TransformSystem.h"
#include "WorldTransformComponent.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <tuple>

namespace
{
	bool Check(bool condition, const char* message)
	{
		if (!condition) std::cerr << message << '\n';
		return condition;
	}

	std::size_t CountCrossings(const Engine::ActionTimelinePlaybackStep& step, float time)
	{
		return Engine::CountActionTimelineCrossings(step, time);
	}

	bool CheckParticleTimeline()
	{
		if (!Engine::ResourceManager::Create())
			return Check(false, "Resource manager creation failed.");
		bool passed = true;
		{
			Engine::ParticleEffectCreateDesc particleDesc;
			Engine::ParticleEmitterData emitter;
			emitter.emissionRate = 40.0f;
			emitter.lifetimeMin = emitter.lifetimeMax = 1.0f;
			emitter.speedMin = emitter.speedMax = 0.0f;
			emitter.gravity = Engine::vec3(0.0f);
			particleDesc.emitters.push_back(emitter);
			auto particle = Engine::ResourceManager::Get().AddResource(L"TimelineCheckParticle", Engine::ParticleEffect::Create(&particleDesc));

			Engine::CompositeEffectCreateDesc effectDesc;
			effectDesc.totalDuration = 2.0f;
			auto effect = Engine::ResourceManager::Get().AddResource(L"TimelineCheckEffect", Engine::CompositeEffect::Create(&effectDesc));
			if (!particle.IsValid() || !effect.IsValid())
			{
				passed = Check(false, "Effect creation failed.");
			}
			else
			{
				Engine::VFXParticleTrack track;
				track.startTime = 0.1f;
				track.duration = 0.1f;
				track.effectHandle = particle;
				effect->GetParticleTracks().push_back(track);

				auto sceneDeleter = [](Engine::Scene* scene) { if (scene) scene->Release(); };
				std::unique_ptr<Engine::Scene, decltype(sceneDeleter)> scene(Engine::Scene::Create(), sceneDeleter);
				if (!scene)
				{
					passed = Check(false, "Scene creation failed.");
				}
				else
				{
					auto& world = scene->GetRegistry();
					const auto entity = world.create();
					world.emplace<Engine::WorldTransformComponent>(entity);
					world.emplace<Engine::FlagComponent>(entity);
					auto& vfx = world.emplace<Engine::CompositeVFXComponent>(entity);
					vfx.effectHandle = effect;
					vfx.previewSeed = 42;
					Engine::CompositeVFXSystem::SimulateSceneVFX(scene.get(), 0.05f);
					Engine::CompositeVFXSystem::SimulateSceneVFX(scene.get(), 0.2f);
					const auto& particles = [&]() -> const std::vector<Engine::Particle>& {
						return vfx.particleRuntimes[0].emitterInstances[0].particles;
					};
					const auto count = particles().size();
					passed &= Check(count > 0, "Crossing the entire track in one tick must still emit particles.");
					if (count > 0)
					{
						const float age = particles()[0].age;
						vfx.SetTimelinePaused(true);
						Engine::CompositeVFXSystem::SimulateSceneVFX(scene.get(), 0.2f);
						passed &= Check(particles().size() == count && particles()[0].age == age, "A paused VFX must not age particles.");
						vfx.SetTimelinePaused(false);
						vfx.Release();
						Engine::CompositeVFXSystem::SimulateSceneVFX(scene.get(), 0.2f);
						passed &= Check(particles().size() == count && particles()[0].age > age,
							"Drain must age existing particles without spawning new ones.");
						Engine::CompositeVFXSystem::SimulateSceneVFX(scene.get(), 1.0f);
						passed &= Check(vfx.isFinished && particles().empty(), "Drain must finish after particles expire.");
					}

					effect->SetTotalDuration(0.3f);
					vfx.Restart();
					Engine::CompositeVFXSystem::SimulateSceneVFX(scene.get(), 0.4f);
					passed &= Check(vfx.isDraining && !vfx.particleRuntimes[0].emitterInstances[0].particles.empty(),
						"Crossing effect end must simulate the active track before draining.");
				}
			}
		}
		Engine::ResourceManager::Destroy();
		return passed;
	}

	struct ReleaseBase
	{
		void operator()(Engine::Base* object) const { if (object) object->Release(); }
	};

	struct ResourceManagerScope
	{
		bool Created = Engine::ResourceManager::Create();
		~ResourceManagerScope()
		{
			if (Created) Engine::ResourceManager::Destroy();
		}
	};

	using ScenePtr = std::unique_ptr<Engine::Scene, ReleaseBase>;
	using TimelineSystemPtr = std::unique_ptr<Engine::ActionTimelineSystem, ReleaseBase>;

	bool CheckParticleNotifySpawnAndAutoDestroy()
	{
		ResourceManagerScope resources;
		if (!resources.Created) return Check(false, "Resource manager creation failed.");

		bool passed = true;
		{
			Engine::AnimNotifyData notify;
			notify.Name = "Burst";
			notify.TriggerTime = 0.05f;
			notify.Type = Engine::EAnimNotifyType::Particle;
			notify.OffsetPosition = Engine::vec3(1.0f, 2.0f, 3.0f);
			notify.ParticleLifetime = 0.2f;
			notify.BurstCount = 12;

			Engine::TimelineNotifyTrack track;
			track.TrackName = "FX Track";
			track.Notifies.push_back(notify);

			Engine::ActionTimelineCreateDesc timelineDesc;
			timelineDesc.Duration = 0.5f;
			timelineDesc.NotifyTracks.push_back(track);
			auto timelineAsset = Engine::ResourceManager::Get().AddResource(
				L"TimelineParticleNotifyAsset", Engine::ActionTimelineAsset::Create(&timelineDesc));

			ScenePtr scene(Engine::Scene::Create());
			TimelineSystemPtr system(Engine::ActionTimelineSystem::Create());
			if (!timelineAsset.IsValid() || !scene || !system)
				return Check(false, "Particle notify test setup failed.");

			scene->SetActive(true);
			auto& world = scene->GetRegistry();
			const auto owner = scene->CreateEntity().GetEntityHandle();
			auto& trans = world.get_or_emplace<Engine::TransformComponent>(owner);
			auto& worldTrans = world.get_or_emplace<Engine::WorldTransformComponent>(owner);
			std::ignore = trans.SetLocalPosition(Engine::vec3(0.0f));
			Engine::TransformSystem::UpdateRootWorld(trans, worldTrans);
			world.get_or_emplace<Engine::FlagComponent>(owner);

			auto& timeline = world.emplace<Engine::ActionTimelineComponent>(owner);
			timeline.timelineAsset = timelineAsset;
			timeline.Play();

			system->OnUpdate(world, { scene.get() }, 0.1f);

			Engine::ResourceHandle<Engine::CompositeEffect> firstEffectHandle;
			std::size_t spawnedCount = 0;
			Engine::vec3 spawnedPos(0.0f);
			for (auto vfxEntity : world.view<Engine::CompositeVFXComponent, Engine::ActionVFXLifecycleComponent>())
			{
				++spawnedCount;
				firstEffectHandle = world.get<Engine::CompositeVFXComponent>(vfxEntity).effectHandle;
				spawnedPos = world.get<Engine::TransformComponent>(vfxEntity).position;
			}
			passed &= Check(spawnedCount == 1 && firstEffectHandle.IsValid() &&
				glm::distance(spawnedPos, Engine::vec3(1.0f, 2.0f, 3.0f)) <= 1.0e-4f,
				"Particle notify must spawn one CompositeVFX entity at the socket world position.");

			Engine::CompositeVFXSystem::SimulateSceneVFX(scene.get(), 0.016f);
			std::size_t emittedParticles = 0;
			for (auto vfxEntity : world.view<Engine::CompositeVFXComponent, Engine::ActionVFXLifecycleComponent>())
			{
				const auto& vfxComp = world.get<Engine::CompositeVFXComponent>(vfxEntity);
				if (!vfxComp.particleRuntimes.empty() && !vfxComp.particleRuntimes[0].emitterInstances.empty())
					emittedParticles = vfxComp.particleRuntimes[0].emitterInstances[0].particles.size();
			}
			passed &= Check(emittedParticles == 12,
				"Particle notify must emit BurstCount particles on the spawned emitter.");

			Engine::CompositeVFXSystem::SimulateSceneVFX(scene.get(), 0.35f);
			bool finishedAfterLifetime = false;
			for (auto vfxEntity : world.view<Engine::CompositeVFXComponent, Engine::ActionVFXLifecycleComponent>())
			{
				const auto& vfxComp = world.get<Engine::CompositeVFXComponent>(vfxEntity);
				finishedAfterLifetime = vfxComp.isFinished &&
					vfxComp.particleRuntimes[0].emitterInstances[0].particles.empty();
			}
			passed &= Check(finishedAfterLifetime,
				"Spawned particle notify VFX must finish after particle lifetimes expire.");

			system->OnUpdate(world, { scene.get() }, 0.016f);
			std::size_t remainingCount = 0;
			for (auto vfxEntity : world.view<Engine::ActionVFXLifecycleComponent>())
			{
				(void)vfxEntity;
				++remainingCount;
			}
			passed &= Check(remainingCount == 0,
				"Finished particle notify entity must be auto-destroyed.");

			timeline.Play();
			system->OnUpdate(world, { scene.get() }, 0.1f);
			Engine::ResourceHandle<Engine::CompositeEffect> secondEffectHandle;
			std::size_t respawnedCount = 0;
			for (auto vfxEntity : world.view<Engine::CompositeVFXComponent, Engine::ActionVFXLifecycleComponent>())
			{
				++respawnedCount;
				secondEffectHandle = world.get<Engine::CompositeVFXComponent>(vfxEntity).effectHandle;
			}
			passed &= Check(respawnedCount == 1 && secondEffectHandle == firstEffectHandle,
				"Re-triggering the same particle notify must reuse the cached CompositeEffect handle.");
		}
		return passed;
	}

	bool CheckAnimatorOverrideLifecycle()
	{
		ResourceManagerScope resources;
		if (!resources.Created) return Check(false, "Resource manager creation failed.");

		bool passed = true;
		{
			auto& rm = Engine::ResourceManager::Get();
			SkeletonCreateDesc skelDesc;
			Bone rootBone;
			rootBone.Name = L"Root";
			skelDesc.Bones.push_back(rootBone);
			auto skeleton = rm.AddResource(L"TimelineLifecycleSkeleton", Engine::Skeleton::Create(&skelDesc));

			AnimationCreateDesc origAnimDesc;
			origAnimDesc.Name = L"OrigClip";
			origAnimDesc.Duration = 30.0;
			origAnimDesc.TicksPerSecond = 30.0;
			auto origClip = rm.AddResource(L"TimelineLifecycleOrigClip", Engine::Animation::Create(&origAnimDesc));

			AnimationCreateDesc actionAnimDesc;
			actionAnimDesc.Name = L"ActionClip";
			actionAnimDesc.Duration = 6.0;
			actionAnimDesc.TicksPerSecond = 30.0;
			auto actionClip = rm.AddResource(L"TimelineLifecycleActionClip", Engine::Animation::Create(&actionAnimDesc));

			Engine::ModelCreateDesc modelDesc;
			modelDesc.Skeleton = skeleton;
			modelDesc.Animations.push_back(actionClip);
			auto model = rm.AddResource(L"TimelineLifecycleModel", Engine::Model::Create(&modelDesc));

			Engine::ActionTimelineCreateDesc timelineDesc;
			timelineDesc.ClipName = "ActionClip";
			timelineDesc.Duration = 0.2f;
			auto asset = rm.AddResource(L"TimelineLifecycleAsset", Engine::ActionTimelineAsset::Create(&timelineDesc));
			if (asset.IsValid()) asset->SetBlendOutTime(0.1f);

			ScenePtr scene(Engine::Scene::Create());
			TimelineSystemPtr system(Engine::ActionTimelineSystem::Create());
			if (!skeleton.IsValid() || !origClip.IsValid() || !actionClip.IsValid() ||
				!model.IsValid() || !asset.IsValid() || !scene || !system)
				return Check(false, "Animator override lifecycle setup failed.");

			scene->SetActive(true);
			auto& world = scene->GetRegistry();
			const auto ownerB = scene->CreateEntity().GetEntityHandle();
			const auto ownerA = scene->CreateEntity().GetEntityHandle();
			const auto animatorEntity = scene->CreateEntity().GetEntityHandle();

			world.get_or_emplace<Engine::HierarchyComponent>(ownerB).children = { ownerA };
			world.get_or_emplace<Engine::HierarchyComponent>(ownerA).parent = ownerB;
			world.get_or_emplace<Engine::HierarchyComponent>(ownerA).children = { animatorEntity };
			world.get_or_emplace<Engine::HierarchyComponent>(animatorEntity).parent = ownerA;
			world.get_or_emplace<Engine::FlagComponent>(ownerB);
			world.get_or_emplace<Engine::FlagComponent>(ownerA);
			world.get_or_emplace<Engine::FlagComponent>(animatorEntity);

			auto& animator = world.emplace<Engine::AnimatorComponent>(animatorEntity);
			animator.currentClip = origClip;
			animator.currentTime = 0.3f;
			animator.isPlaying = true;
			world.emplace<Engine::SkinnedMeshRendererComponent>(animatorEntity).skeletonHandle = skeleton;
			world.emplace<Engine::ModelInstanceComponent>(animatorEntity).modelHandle = model;

			auto& timelineA = world.emplace<Engine::ActionTimelineComponent>(ownerA);
			timelineA.timelineAsset = asset;
			timelineA.Play();

			// 1) 자연 종료 후 원래 Animator 상태로 복원 + blendFromPose 설정 검증
			system->OnUpdate(world, { scene.get() }, 0.1f);
			passed &= Check(animator.timelineOverrideOwner == ownerA && animator.currentClip == actionClip &&
				!animator.isPlaying && timelineA.animatorOverrideEntity == animatorEntity,
				"Active timeline must acquire animator override ownership.");

			system->OnUpdate(world, { scene.get() }, 0.15f);
			system->OnUpdate(world, { scene.get() }, 0.016f);
			passed &= Check(animator.timelineOverrideOwner == entt::null && animator.currentClip == origClip &&
				std::abs(animator.currentTime - 0.3f) <= 1.0e-5f && animator.isPlaying &&
				animator.blendFromPose.size() == 1 && std::abs(animator.blendFromDuration - 0.1f) <= 1.0e-5f,
				"Natural timeline end must restore saved animator clip, time, playing state, and blend pose.");

			// 2) 오버라이드 도중 컴포넌트 제거 훅(OnActionTimelineComponentDestroy) 복원 검증
			animator.blendFromPose.clear();
			timelineA.Play();
			system->OnUpdate(world, { scene.get() }, 0.05f);
			passed &= Check(animator.timelineOverrideOwner == ownerA && animator.currentClip == actionClip,
				"Second playback must re-acquire animator override.");
			world.remove<Engine::ActionTimelineComponent>(ownerA);
			passed &= Check(animator.timelineOverrideOwner == entt::null && animator.currentClip == origClip &&
				std::abs(animator.currentTime - 0.3f) <= 1.0e-5f && animator.isPlaying,
				"Removing ActionTimelineComponent must immediately restore the overridden animator.");

			// 3) 다른 타임라인으로 소유권 인계 후 이전 소유자 제거 시 중복 복원 차단 검증
			auto& timelineA2 = world.emplace<Engine::ActionTimelineComponent>(ownerA);
			timelineA2.timelineAsset = asset;
			timelineA2.Play();
			system->OnUpdate(world, { scene.get() }, 0.05f);
			passed &= Check(animator.timelineOverrideOwner == ownerA,
				"ownerA must hold animator override before handoff.");

			auto& timelineB = world.emplace<Engine::ActionTimelineComponent>(ownerB);
			timelineB.timelineAsset = asset;
			timelineB.Play();
			system->OnUpdate(world, { scene.get() }, 0.05f);
			passed &= Check(animator.timelineOverrideOwner == ownerB && animator.timelineSavedClip == origClip &&
				timelineA2.animatorOverrideEntity == entt::null && timelineB.animatorOverrideEntity == animatorEntity,
				"ownerB must take over animator override while preserving the original saved clip.");

			world.remove<Engine::ActionTimelineComponent>(ownerA);
			passed &= Check(animator.timelineOverrideOwner == ownerB && animator.currentClip == actionClip &&
				!animator.isPlaying,
				"Destroying previous override owner must not overwrite active handoff owner.");

			system->OnUpdate(world, { scene.get() }, 0.2f);
			system->OnUpdate(world, { scene.get() }, 0.016f);
			passed &= Check(animator.timelineOverrideOwner == entt::null && animator.currentClip == origClip &&
				std::abs(animator.currentTime - 0.3f) <= 1.0e-5f && animator.isPlaying,
				"Ending the handoff timeline must restore the original animator state once.");
		}
		return passed;
	}

	bool CheckHeldVFXReleaseLifecycle()
	{
		ResourceManagerScope resources;
		if (!resources.Created) return Check(false, "Resource manager creation failed.");

		bool passed = true;
		{
			Engine::ActionTimelineCreateDesc timelineDesc;
			timelineDesc.Duration = 1.0f;
			auto asset = Engine::ResourceManager::Get().AddResource(
				L"TimelineHoldAsset", Engine::ActionTimelineAsset::Create(&timelineDesc));

			ScenePtr scene(Engine::Scene::Create());
			TimelineSystemPtr system(Engine::ActionTimelineSystem::Create());
			if (!asset.IsValid() || !scene || !system)
				return Check(false, "Held VFX release lifecycle setup failed.");

			scene->SetActive(true);
			auto& world = scene->GetRegistry();
			const auto owner = scene->CreateEntity().GetEntityHandle();
			world.get_or_emplace<Engine::FlagComponent>(owner);
			auto& timeline = world.emplace<Engine::ActionTimelineComponent>(owner);
			timeline.timelineAsset = asset;
			timeline.Play();
			system->OnUpdate(world, { scene.get() }, 0.05f);

			const auto held1 = scene->CreateEntity().GetEntityHandle();
			auto& vfx1 = world.emplace<Engine::CompositeVFXComponent>(held1);
			vfx1.holdUntilRelease = true;
			vfx1.isPlaying = true;
			vfx1.isLooping = true;
			auto& hold1 = world.emplace<Engine::ActionVFXHoldComponent>(held1);
			hold1.ownerEntity = owner;
			hold1.playSerial = timeline.playSerial;
			hold1.releaseElapsed = 0.2f;

			// 일시정지 중에는 해제되지 않고 동기 일시정지만 반영된다.
			timeline.Pause();
			system->OnUpdate(world, { scene.get() }, 0.1f);
			passed &= Check(world.all_of<Engine::ActionVFXHoldComponent>(held1) && vfx1.timelinePaused && !vfx1.isDraining,
				"Paused timeline must pause held VFX without releasing it.");

			// 재개 후 releaseElapsed(0.2s)를 넘기면 해제(Release)되고 Hold 컴포넌트가 제거된다.
			timeline.Resume();
			system->OnUpdate(world, { scene.get() }, 0.2f);
			passed &= Check(!world.all_of<Engine::ActionVFXHoldComponent>(held1) && vfx1.isDraining && !vfx1.holdUntilRelease,
				"Passing releaseElapsed must release held VFX and remove ActionVFXHoldComponent.");

			// 소유 타임라인 컴포넌트 제거 시 지속 중이던 VFX도 즉시 해제된다.
			const auto held2 = scene->CreateEntity().GetEntityHandle();
			auto& vfx2 = world.emplace<Engine::CompositeVFXComponent>(held2);
			vfx2.holdUntilRelease = true;
			vfx2.isPlaying = true;
			auto& hold2 = world.emplace<Engine::ActionVFXHoldComponent>(held2);
			hold2.ownerEntity = owner;
			hold2.playSerial = timeline.playSerial;
			hold2.releaseElapsed = 10.0f;

			world.remove<Engine::ActionTimelineComponent>(owner);
			passed &= Check(!world.all_of<Engine::ActionVFXHoldComponent>(held2) && vfx2.isDraining && !vfx2.holdUntilRelease,
				"Destroying owning ActionTimelineComponent must release any remaining held VFX.");
		}
		return passed;
	}
}

int main()
{
	bool passed = true;
	const auto firstTick = Engine::AdvanceActionTimeline(0.0f, 0.1f, 1.0f, false, true);
	passed &= Check(CountCrossings(firstTick, 0.0f) == 1, "The initial zero-time notify must fire once.");
	passed &= Check(CountCrossings(firstTick, 0.1f) == 1, "The end of the first interval must be included.");

	const auto boundaryNextTick = Engine::AdvanceActionTimeline(firstTick.CurrentTime, 0.1f, 1.0f, false, false);
	passed &= Check(CountCrossings(boundaryNextTick, 0.1f) == 0, "A notify on the previous interval boundary must not fire twice.");
	passed &= Check(CountCrossings(boundaryNextTick, 0.2f) == 1, "The end of the next interval must be included.");

	const auto resumed = Engine::AdvanceActionTimeline(firstTick.CurrentTime, 0.1f, 1.0f, false, false);
	passed &= Check(CountCrossings(resumed, 0.0f) == 0, "Resume must not re-fire the start notify.");
	const auto paused = Engine::AdvanceActionTimeline(firstTick.CurrentTime, 0.0f, 1.0f, false, false);
	passed &= Check(CountCrossings(paused, 0.0f) == 0, "A paused step must not produce notify crossings.");

	const auto multipleLoops = Engine::AdvanceActionTimeline(0.8f, 2.4f, 1.0f, true, false);
	passed &= Check(multipleLoops.LoopCount == 3, "A multi-loop frame must report every crossed loop.");
	passed &= Check(std::abs(multipleLoops.CurrentTime - 0.2f) < 0.0001f, "A multi-loop frame must retain the wrapped remainder.");
	passed &= Check(CountCrossings(multipleLoops, 0.0f) == 3, "A zero-time notify must fire once at each crossed loop start.");

	const auto exactLoop = Engine::AdvanceActionTimeline(0.0f, 1.0f, 1.0f, true, false);
	passed &= Check(exactLoop.LoopCount == 1 && exactLoop.CurrentTime == 0.0f, "An exact loop boundary must wrap to zero.");
	passed &= Check(CountCrossings(exactLoop, 0.0f) == 1, "An exact loop boundary must include the new loop's zero-time notify.");

	const auto finish = Engine::AdvanceActionTimeline(0.9f, 0.2f, 1.0f, false, false);
	passed &= Check(finish.Finished && finish.CurrentTime == 1.0f, "A non-looping step must clamp and finish at the duration.");
	passed &= Check(CountCrossings(finish, 1.0f) == 1, "The final duration boundary must be included.");

	Engine::CompositeVFXComponent vfx;
	vfx.holdUntilRelease = true;
	vfx.particleRuntimes.resize(1);
	vfx.particleRuntimes[0].emitterInstances.resize(1);
	vfx.particleRuntimes[0].emitterInstances[0].particles.emplace_back();
	vfx.timelineTimeScale = 0.5f;
	vfx.SetTimelinePaused(true);
	vfx.Release();
	passed &= Check(vfx.isDraining && !vfx.isFinished && !vfx.holdUntilRelease, "Release must begin draining.");
	passed &= Check(!vfx.timelinePaused && vfx.timelineTimeScale == 1.0f, "Released effects must no longer follow the owner.");
	passed &= Check(vfx.particleRuntimes[0].emitterInstances[0].particles.size() == 1, "Release must preserve live particles.");
	vfx.Release();
	passed &= Check(vfx.particleRuntimes[0].emitterInstances[0].particles.size() == 1, "Release must be idempotent while draining.");

	{
		entt::registry world;
		const auto root = world.create();
		const auto disabled = world.create();
		const auto enabled = world.create();
		world.emplace<Engine::HierarchyComponent>(root).children = { disabled, enabled };
		world.emplace<Engine::AnimatorComponent>(disabled);
		world.emplace<Engine::AnimatorComponent>(enabled);
		Engine::detail::SetComponentEnabled<Engine::AnimatorComponent>(world, disabled, false);
		passed &= Check(Engine::FindAnimatorEntityInHierarchy(world, root) == enabled,
			"Animator search must skip disabled siblings and continue through the hierarchy.");
		Engine::detail::SetComponentEnabled<Engine::AnimatorComponent>(world, disabled, true);
		passed &= Check(Engine::FindAnimatorEntityInHierarchy(world, root) == disabled,
			"Re-enabling an Animator must restore its normal lookup priority.");
	}

	passed &= CheckParticleTimeline();
	passed &= CheckParticleNotifySpawnAndAutoDestroy();
	passed &= CheckAnimatorOverrideLifecycle();
	passed &= CheckHeldVFXReleaseLifecycle();

	return passed ? 0 : 1;
}
