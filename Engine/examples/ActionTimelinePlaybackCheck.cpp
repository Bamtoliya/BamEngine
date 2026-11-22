#include "ActionTimelineAsset.h"
#include "ActionTimelineComponent.h"
#include "ActionTimelinePlayback.h"
#include "ActionTimelineSystem.h"
#include "Animation.h"
#include "AnimationSceneQuery.h"
#include "AnimatorComponent.h"
#include "CameraComponent.h"
#include "ComponentEnable.h"
#include "HierarchyComponent.h"
#include "CompositeVFXComponent.h"
#include "CompositeVFXSystem.h"
#include "FlagComponent.h"
#include "Model.h"
#include "ModelInstanceComponent.h"
#include "NameComponent.h"
#include "ReflectionRegistry.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "Skeleton.h"
#include "SkinnedMeshRendererComponent.h"
#include "SplineComponent.h"
#include "SplineCurve.h"
#include "TransformComponent.h"
#include "TransformSystem.h"
#include "WorldTransformComponent.h"
#include <archive/JsonArchive.h>
#include "GameplayCombatSystem.h"
#include "HealthComponent.h"
#include "HitboxComponent.h"
#include "InteractableComponent.h"
#include "RigidBodyComponent.h"
#include "CharacterControllerComponent.h"
#include "TagComponent.h"
#include "RuntimeSystemManifest.h"
#include "SystemManager.h"
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
			std::ignore = world.get_or_emplace<Engine::FlagComponent>(owner);

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
			std::ignore = world.get_or_emplace<Engine::FlagComponent>(ownerB);
			std::ignore = world.get_or_emplace<Engine::FlagComponent>(ownerA);
			std::ignore = world.get_or_emplace<Engine::FlagComponent>(animatorEntity);

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
			std::ignore = world.get_or_emplace<Engine::FlagComponent>(owner);
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

	bool CheckGameplayCombat()
	{
		bool passed = true;
		ScenePtr scene(Engine::Scene::Create());
		if (!scene)
			return Check(false, "Scene creation failed in CheckGameplayCombat.");

		scene->SetActive(true);
		auto& world = scene->GetRegistry();

		// 1. Attacker entity at (0, 0, 0) with active hitbox
		const auto attacker = scene->CreateEntity().GetEntityHandle();
		world.emplace<Engine::TransformComponent>(attacker).position = Engine::vec3(0.0f);
		world.emplace<Engine::FlagComponent>(attacker);
		auto& attackerHitbox = world.emplace<Engine::HitboxComponent>(attacker);
		attackerHitbox.damage = 25.0f;
		attackerHitbox.knockbackForce = 10.0f;
		attackerHitbox.fallbackRadius = 3.0f;
		attackerHitbox.teamTag = "PlayerTeam";
		attackerHitbox.active = true;

		// 2. Enemy entity at (1.5, 0, 0) with Health and RigidBody
		const auto enemy = scene->CreateEntity().GetEntityHandle();
		world.emplace<Engine::TransformComponent>(enemy).position = Engine::vec3(1.5f, 0.0f, 0.0f);
		world.emplace<Engine::FlagComponent>(enemy);
		auto& enemyHealth = world.emplace<Engine::HealthComponent>(enemy);
		enemyHealth.maxHealth = 100.0f;
		enemyHealth.currentHealth = 100.0f;
		auto& enemyRb = world.emplace<Engine::RigidBodyComponent>(enemy);
		enemyRb.LinearVelocity = Engine::vec3(0.0f);

		// 3. Friendly entity at (-1.5, 0, 0) with Health and matching teamTag
		const auto ally = scene->CreateEntity().GetEntityHandle();
		world.emplace<Engine::TransformComponent>(ally).position = Engine::vec3(-1.5f, 0.0f, 0.0f);
		world.emplace<Engine::FlagComponent>(ally);
		auto& allyHealth = world.emplace<Engine::HealthComponent>(ally);
		allyHealth.maxHealth = 100.0f;
		allyHealth.currentHealth = 100.0f;
		auto& allyHitbox = world.emplace<Engine::HitboxComponent>(ally);
		allyHitbox.teamTag = "PlayerTeam";
		allyHitbox.active = false;

		// Frame 1: active hitbox damages enemy once, skips friendly teamTag, applies knockback
		Engine::GameplayCombatSystem::ProcessSceneCombat(*scene, 0.016f);
		passed &= Check(enemyHealth.currentHealth == 75.0f, "Enemy must take 25 damage on first combat frame.");
		passed &= Check(allyHealth.currentHealth == 100.0f, "Friendly entity with matching teamTag must skip damage (no friendly fire).");
		passed &= Check(enemyRb.LinearVelocity.x > 0.0f, "Knockback must apply positive X linear velocity away from attacker.");
		passed &= Check(attackerHitbox.HasHit(enemy), "Attacker hitbox must record hit on enemy for deduplication.");

		// Frame 2: swing deduplication across multiple frames
		Engine::GameplayCombatSystem::ProcessSceneCombat(*scene, 0.016f);
		passed &= Check(enemyHealth.currentHealth == 75.0f, "Enemy must NOT take damage again in the same swing (swing deduplication).");

		// Reset swing: SetActive(false) -> SetActive(true) resets hit list
		attackerHitbox.SetActive(false);
		passed &= Check(attackerHitbox.hitEntitiesInSwing.empty(), "SetActive(false) must clear swing hits.");
		attackerHitbox.SetActive(true);
		passed &= Check(attackerHitbox.hitEntitiesInSwing.empty(), "SetActive(true) must clear swing hits.");

		// Frame 3: second swing damages enemy again
		Engine::GameplayCombatSystem::ProcessSceneCombat(*scene, 0.016f);
		passed &= Check(enemyHealth.currentHealth == 50.0f, "Enemy must take damage again on new swing after SetActive toggle.");

		// Destroy on hit test
		attackerHitbox.SetActive(false);
		const auto proj = scene->CreateEntity().GetEntityHandle();
		world.emplace<Engine::TransformComponent>(proj).position = Engine::vec3(1.5f, 0.0f, 0.0f);
		world.emplace<Engine::FlagComponent>(proj);
		auto& projHitbox = world.emplace<Engine::HitboxComponent>(proj);
		projHitbox.damage = 10.0f;
		projHitbox.fallbackRadius = 2.0f;
		projHitbox.destroyOnHit = true;
		projHitbox.active = true;

		Engine::GameplayCombatSystem::ProcessSceneCombat(*scene, 0.016f);
		passed &= Check(enemyHealth.currentHealth == 40.0f, "Projectile must damage enemy.");
		passed &= Check(!projHitbox.active, "destroyOnHit hitbox must deactivate after dealing damage.");

		return passed;
	}

	bool CheckTimelineHitboxWindow()
	{
		ResourceManagerScope resources;
		if (!resources.Created) return Check(false, "Resource manager creation failed.");

		bool passed = true;
		ScenePtr scene(Engine::Scene::Create());
		TimelineSystemPtr system(Engine::ActionTimelineSystem::Create());
		if (!scene || !system)
			return Check(false, "Scene or ActionTimelineSystem creation failed.");

		scene->SetActive(true);

		// Create timeline asset with "HitboxOn" at 0.2s and "HitboxOff" at 0.6s
		Engine::ActionTimelineCreateDesc desc;
		desc.Duration = 1.0f;

		Engine::TimelineNotifyTrack notifyTrack;
		notifyTrack.TrackName = "Combat Track";
		notifyTrack.Kind = Engine::ETimelineNotifyTrackKind::Event;

		Engine::AnimNotifyData notifyOn;
		notifyOn.Name = "HitboxOn";
		notifyOn.EventTag = "HitboxOn";
		notifyOn.TriggerTime = 0.2f;
		notifyOn.Type = Engine::EAnimNotifyType::CustomEvent;
		notifyTrack.Notifies.push_back(notifyOn);

		Engine::AnimNotifyData notifyOff;
		notifyOff.Name = "HitboxOff";
		notifyOff.EventTag = "HitboxOff";
		notifyOff.TriggerTime = 0.6f;
		notifyOff.Type = Engine::EAnimNotifyType::CustomEvent;
		notifyTrack.Notifies.push_back(notifyOff);

		desc.NotifyTracks.push_back(notifyTrack);

		auto assetHandle = Engine::ResourceManager::Get().AddResource(
			L"TimelineHitboxWindowAsset", Engine::ActionTimelineAsset::Create(&desc));
		if (!assetHandle.IsValid())
			return Check(false, "ActionTimelineAsset creation failed.");

		auto& world = scene->GetRegistry();
		const auto entity = scene->CreateEntity().GetEntityHandle();
		world.emplace<Engine::WorldTransformComponent>(entity);
		world.emplace<Engine::FlagComponent>(entity);

		auto& timeline = world.emplace<Engine::ActionTimelineComponent>(entity);
		timeline.timelineAsset = assetHandle;

		auto& hitbox = world.emplace<Engine::HitboxComponent>(entity);
		hitbox.active = false;
		hitbox.damage = 20.0f;

		timeline.Play();

		// Tick 1: dt = 0.1s -> currentTime = 0.1s (< 0.2s). Hitbox must still be inactive.
		system->OnUpdate(world, { scene.get() }, 0.1f);
		passed &= Check(!hitbox.active, "Hitbox must remain inactive before 0.2s HitboxOn notify.");

		// Tick 2: dt = 0.15s -> currentTime = 0.25s (> 0.2s). HitboxOn notify fires!
		system->OnUpdate(world, { scene.get() }, 0.15f);
		passed &= Check(hitbox.active, "Hitbox must be active after crossing 0.2s HitboxOn notify.");

		// Simulate a hit during active window
		hitbox.RecordHit(entity);
		passed &= Check(hitbox.HasHit(entity), "Hitbox should record hit during swing.");

		// Tick 3: dt = 0.4s -> currentTime = 0.65s (> 0.6s). HitboxOff notify fires!
		system->OnUpdate(world, { scene.get() }, 0.4f);
		passed &= Check(!hitbox.active, "Hitbox must be deactivated after crossing 0.6s HitboxOff notify.");
		passed &= Check(hitbox.hitEntitiesInSwing.empty(), "Hitbox swing hits must be cleared on deactivation.");

		return passed;
	}

	bool CheckInteractableQueries()
	{
		bool passed = true;
		ScenePtr scene(Engine::Scene::Create());
		if (!scene)
			return Check(false, "Scene creation failed in CheckInteractableQueries.");

		scene->SetActive(true);
		auto& world = scene->GetRegistry();

		// Object A at (2, 0, 0): Chest, range 3.0, oneShot = true
		const auto objA = scene->CreateEntity().GetEntityHandle();
		world.emplace<Engine::TransformComponent>(objA).position = Engine::vec3(2.0f, 0.0f, 0.0f);
		world.emplace<Engine::FlagComponent>(objA);
		auto& intA = world.emplace<Engine::InteractableComponent>(objA);
		intA.interactRange = 3.0f;
		intA.interactTag = "Chest";
		intA.oneShot = true;

		// Object B at (5, 0, 0): NPC, range 2.0
		const auto objB = scene->CreateEntity().GetEntityHandle();
		world.emplace<Engine::TransformComponent>(objB).position = Engine::vec3(5.0f, 0.0f, 0.0f);
		world.emplace<Engine::FlagComponent>(objB);
		auto& intB = world.emplace<Engine::InteractableComponent>(objB);
		intB.interactRange = 2.0f;
		intB.interactTag = "NPC";

		// From (0, 0, 0): nearest interactable is objA (dist 2.0 <= 3.0)
		const auto nearestFromOrigin = Engine::GameplayCombatSystem::FindNearestInteractable(*scene, Engine::vec3(0.0f));
		passed &= Check(nearestFromOrigin == objA, "FindNearestInteractable from origin must find objA.");

		// Tag filter "NPC" from origin (dist 5.0 > 2.0 range) -> none
		const auto npcFromOrigin = Engine::GameplayCombatSystem::FindNearestInteractable(*scene, Engine::vec3(0.0f), "NPC");
		passed &= Check(npcFromOrigin == entt::null, "NPC from origin is out of range, must return null.");

		// From (4.5, 0, 0): nearest NPC is objB (dist 0.5 <= 2.0)
		const auto npcNear = Engine::GameplayCombatSystem::FindNearestInteractable(*scene, Engine::vec3(4.5f, 0.0f, 0.0f), "NPC");
		passed &= Check(npcNear == objB, "FindNearestInteractable near objB with 'NPC' tag must find objB.");

		// TryInteractNearest on objA (oneShot): first interaction succeeds and marks consumed
		entt::entity interacted = entt::null;
		const bool firstInteract = Engine::GameplayCombatSystem::TryInteractNearest(*scene, Engine::vec3(0.0f), &interacted, "Chest");
		passed &= Check(firstInteract && interacted == objA, "First interaction on oneShot chest must succeed.");
		passed &= Check(intA.consumed, "OneShot chest must be consumed after interaction.");

		// Second interaction on oneShot chest must fail
		interacted = entt::null;
		const bool secondInteract = Engine::GameplayCombatSystem::TryInteractNearest(*scene, Engine::vec3(0.0f), &interacted, "Chest");
		passed &= Check(!secondInteract && interacted == entt::null, "Subsequent interaction on consumed oneShot chest must fail.");

		return passed;
	}

	struct SystemManagerScope
	{
		bool Created = static_cast<bool>(Engine::SystemManager::Create());
		~SystemManagerScope()
		{
			if (Created) Engine::SystemManager::Destroy();
		}
	};

	bool CheckRuntimeSystemManifestRegistersCombat()
	{
		SystemManagerScope systems;
		if (!systems.Created)
			return Check(false, "SystemManager creation failed in CheckRuntimeSystemManifestRegistersCombat.");

		bool passed = true;
		auto& systemManager = Engine::SystemManager::Get();
		passed &= Check(Engine::RegisterCommonRuntimeSystems(systemManager) == Engine::EResult::Success,
			"RegisterCommonRuntimeSystems must succeed.");

		auto* registeredCombat = systemManager.GetSystem<Engine::GameplayCombatSystem>();
		passed &= Check(registeredCombat != nullptr,
			"RegisterCommonRuntimeSystems must register GameplayCombatSystem.");
		if (registeredCombat)
		{
			passed &= Check(registeredCombat->GetExecutionPhases() == static_cast<Engine::uint8>(Engine::ESystemPhase::Gameplay),
				"GameplayCombatSystem must execute in ESystemPhase::Gameplay.");
		}

		// 페이즈 마스크 가드 검증: 다른 시스템 싱글톤 의존성을 피하기 위해 GameplayCombatSystem만 단독 유지한다.
		systemManager.ClearSystems();
		auto* isolatedCombat = systemManager.AddSystem<Engine::GameplayCombatSystem>();
		ScenePtr scene(Engine::Scene::Create());
		if (!isolatedCombat || !scene)
			return Check(false, "Failed to setup isolated GameplayCombatSystem phase guard check.");

		scene->SetActive(true);
		auto& world = scene->GetRegistry();

		const auto attacker = scene->CreateEntity().GetEntityHandle();
		world.emplace<Engine::TransformComponent>(attacker).position = Engine::vec3(0.0f);
		world.emplace<Engine::FlagComponent>(attacker);
		auto& hitbox = world.emplace<Engine::HitboxComponent>(attacker);
		hitbox.damage = 25.0f;
		hitbox.fallbackRadius = 3.0f;
		hitbox.active = true;

		const auto enemy = scene->CreateEntity().GetEntityHandle();
		world.emplace<Engine::TransformComponent>(enemy).position = Engine::vec3(1.0f, 0.0f, 0.0f);
		world.emplace<Engine::FlagComponent>(enemy);
		auto& health = world.emplace<Engine::HealthComponent>(enemy);
		health.maxHealth = 100.0f;
		health.currentHealth = 100.0f;

		// Presentation 전용 페이즈에서는 전투가 실행되지 않아야 한다.
		systemManager.Update(world, { scene.get() }, 0.016f, static_cast<Engine::uint8>(Engine::ESystemPhase::Presentation));
		passed &= Check(health.currentHealth == 100.0f,
			"GameplayCombatSystem must not apply damage during Presentation-only phase.");

		// OnUpdate 직접 호출 시에도 SystemManager의 마지막 페이즈 마스크에 Gameplay가 없으면 차단된다.
		isolatedCombat->OnUpdate(world, { scene.get() }, 0.016f);
		passed &= Check(health.currentHealth == 100.0f,
			"GameplayCombatSystem::OnUpdate must honor SystemManager Gameplay phase mask.");

		// Gameplay 페이즈에서는 정상적으로 피해를 적용한다.
		systemManager.Update(world, { scene.get() }, 0.016f, static_cast<Engine::uint8>(Engine::ESystemPhase::Gameplay));
		passed &= Check(health.currentHealth == 75.0f,
			"GameplayCombatSystem must apply damage during Gameplay phase.");

		return passed;
	}

	bool CheckCameraCutTrackAndRestore()
	{
		ResourceManagerScope resources;
		if (!resources.Created) return Check(false, "Resource manager creation failed.");

		bool passed = true;
		{
			Engine::TimelineCameraCutSection cut;
			cut.TrackName = "BossIntroCut";
			cut.IsActive = true;
			cut.StartTime = 0.0f;
			cut.EndTime = 0.6f;
			cut.TargetCameraName = "CutCam";
			cut.LocalPositionOffset = Engine::vec3(0.0f, 1.0f, 2.0f);
			cut.LocalEulerRotationOffset = Engine::vec3(0.0f, 0.0f, 0.0f);
			cut.TargetFOV = 35.0f;
			cut.BlendInDuration = 0.2f;

			Engine::ActionTimelineCreateDesc desc;
			desc.Duration = 0.6f;
			desc.CameraCutTracks.push_back(cut);
			auto asset = Engine::ResourceManager::Get().AddResource(
				L"TimelineCameraCutAsset", Engine::ActionTimelineAsset::Create(&desc));

			ScenePtr scene(Engine::Scene::Create());
			TimelineSystemPtr system(Engine::ActionTimelineSystem::Create());
			if (!asset.IsValid() || !scene || !system)
				return Check(false, "Camera cut test setup failed.");

			scene->SetActive(true);
			auto& world = scene->GetRegistry();

			// 1) 원래 메인 카메라
			const auto mainCamEntity = scene->CreateEntity().GetEntityHandle();
			world.get_or_emplace<Engine::NameComponent>(mainCamEntity).name = L"MainCam";
			auto& mainTrans = world.get_or_emplace<Engine::TransformComponent>(mainCamEntity);
			auto& mainWorld = world.get_or_emplace<Engine::WorldTransformComponent>(mainCamEntity);
			(void)mainTrans.SetLocalPosition(Engine::vec3(0.0f, 2.0f, -5.0f));
			(void)mainTrans.SetEulerRotation(Engine::vec3(0.0f, 0.0f, 0.0f));
			(void)Engine::TransformSystem::UpdateRootWorld(mainTrans, mainWorld);
			std::ignore = world.get_or_emplace<Engine::FlagComponent>(mainCamEntity);
			auto& mainCam = world.emplace<Engine::CameraComponent>(mainCamEntity);
			mainCam.FOV = 60.0f;
			mainCam.IsMainCamera = true;

			// 2) 시네마틱 컷 타깃 카메라
			const auto cutCamEntity = scene->CreateEntity().GetEntityHandle();
			world.get_or_emplace<Engine::NameComponent>(cutCamEntity).name = L"CutCam";
			auto& cutTrans = world.get_or_emplace<Engine::TransformComponent>(cutCamEntity);
			auto& cutWorld = world.get_or_emplace<Engine::WorldTransformComponent>(cutCamEntity);
			const Engine::vec3 cutBasePos(10.0f, 4.0f, 5.0f);
			const Engine::vec3 cutBaseEuler(0.0f, 90.0f, 0.0f);
			(void)cutTrans.SetLocalPosition(cutBasePos);
			(void)cutTrans.SetEulerRotation(cutBaseEuler);
			(void)Engine::TransformSystem::UpdateRootWorld(cutTrans, cutWorld);
			const Engine::quat cutBaseRot = cutTrans.rotation;
			std::ignore = world.get_or_emplace<Engine::FlagComponent>(cutCamEntity);
			auto& cutCam = world.emplace<Engine::CameraComponent>(cutCamEntity);
			cutCam.FOV = 45.0f;
			cutCam.IsMainCamera = false;

			const Engine::vec3 expectedTargetPos = cutBasePos + cutBaseRot * cut.LocalPositionOffset;

			// 3) 타임라인 소유자
			const auto owner = scene->CreateEntity().GetEntityHandle();
			std::ignore = world.get_or_emplace<Engine::FlagComponent>(owner);
			auto& timeline = world.emplace<Engine::ActionTimelineComponent>(owner);
			timeline.timelineAsset = asset;
			timeline.Play();

			// 틱 1: t = 0.1s (BlendInDuration 0.2s의 50% 지점)
			system->OnUpdate(world, { scene.get() }, 0.1f);
			const Engine::vec3 expectedMidPos = glm::mix(Engine::vec3(0.0f, 2.0f, -5.0f), expectedTargetPos, 0.5f);
			passed &= Check(cutCam.IsMainCamera && !mainCam.IsMainCamera,
				"Camera Cut section must switch IsMainCamera to the target camera.");
			passed &= Check(glm::distance(cutTrans.position, expectedMidPos) <= 1.0e-3f,
				"Camera Cut must blend position midway during BlendInDuration.");
			passed &= Check(std::abs(cutCam.FOV - 47.5f) <= 1.0e-3f,
				"Camera Cut must blend FOV midway from previous main camera FOV to TargetFOV.");

			// 틱 2: t = 0.25s (BlendIn 완료)
			system->OnUpdate(world, { scene.get() }, 0.15f);
			passed &= Check(glm::distance(cutTrans.position, expectedTargetPos) <= 1.0e-3f,
				"Camera Cut must reach target offset position after BlendInDuration.");
			passed &= Check(std::abs(cutCam.FOV - 35.0f) <= 1.0e-3f,
				"Camera Cut must reach TargetFOV after BlendInDuration.");

			// 4) 중도 취소(Stop) 후 원래 카메라 상태 무손실 복원 검증
			timeline.Stop();
			system->OnUpdate(world, { scene.get() }, 0.016f);
			passed &= Check(mainCam.IsMainCamera && !cutCam.IsMainCamera,
				"Stopping timeline must restore original IsMainCamera flags.");
			passed &= Check(std::abs(mainCam.FOV - 60.0f) <= 1.0e-4f && std::abs(cutCam.FOV - 45.0f) <= 1.0e-4f,
				"Stopping timeline must restore original FOV on both cameras.");
			passed &= Check(glm::distance(mainTrans.position, Engine::vec3(0.0f, 2.0f, -5.0f)) <= 1.0e-4f &&
				glm::distance(cutTrans.position, cutBasePos) <= 1.0e-4f,
				"Stopping timeline must restore original camera positions.");

			// 5) 자연 종료 후 무손실 복원 검증
			timeline.Play();
			system->OnUpdate(world, { scene.get() }, 0.25f);
			passed &= Check(cutCam.IsMainCamera && std::abs(cutCam.FOV - 35.0f) <= 1.0e-3f,
				"Replaying timeline must re-apply Camera Cut.");
			system->OnUpdate(world, { scene.get() }, 0.4f);
			passed &= Check(mainCam.IsMainCamera && !cutCam.IsMainCamera &&
				std::abs(mainCam.FOV - 60.0f) <= 1.0e-4f && std::abs(cutCam.FOV - 45.0f) <= 1.0e-4f &&
				glm::distance(cutTrans.position, cutBasePos) <= 1.0e-4f,
				"Natural timeline end must restore original camera states losslessly.");

			// 6) 컴포넌트 제거(OnActionTimelineComponentDestroy) 시 즉시 복원 검증
			timeline.Play();
			system->OnUpdate(world, { scene.get() }, 0.25f);
			passed &= Check(cutCam.IsMainCamera, "Third playback must switch to CutCam before removal.");
			world.remove<Engine::ActionTimelineComponent>(owner);
			passed &= Check(mainCam.IsMainCamera && !cutCam.IsMainCamera &&
				std::abs(cutCam.FOV - 45.0f) <= 1.0e-4f &&
				glm::distance(cutTrans.position, cutBasePos) <= 1.0e-4f,
				"Removing ActionTimelineComponent must immediately restore camera states.");
		}
		return passed;
	}

	bool CheckCameraSplinePathAndLookAt()
	{
		ResourceManagerScope resources;
		if (!resources.Created) return Check(false, "Resource manager creation failed.");

		bool passed = true;
		{
			Engine::TimelineCameraSplineSection lookAtSection;
			lookAtSection.TrackName = "SplineLookAt";
			lookAtSection.IsActive = true;
			lookAtSection.StartTime = 0.0f;
			lookAtSection.EndTime = 0.5f;
			lookAtSection.SplineEntityName = "CamSpline";
			lookAtSection.StartT = 0.0f;
			lookAtSection.EndT = 1.0f;
			lookAtSection.LookAtTargetName = "HeroTarget";
			lookAtSection.FollowTangent = false;
			lookAtSection.TargetFOV = 50.0f;

			Engine::TimelineCameraSplineSection tangentSection;
			tangentSection.TrackName = "SplineTangent";
			tangentSection.IsActive = true;
			tangentSection.StartTime = 0.5f;
			tangentSection.EndTime = 1.0f;
			tangentSection.SplineEntityName = "CamSpline";
			tangentSection.StartT = 0.0f;
			tangentSection.EndT = 1.0f;
			tangentSection.LookAtTargetName = "";
			tangentSection.FollowTangent = true;

			Engine::ActionTimelineCreateDesc desc;
			desc.Duration = 1.0f;
			desc.CameraSplineTracks.push_back(lookAtSection);
			desc.CameraSplineTracks.push_back(tangentSection);
			auto asset = Engine::ResourceManager::Get().AddResource(
				L"TimelineCameraSplineAsset", Engine::ActionTimelineAsset::Create(&desc));

			ScenePtr scene(Engine::Scene::Create());
			TimelineSystemPtr system(Engine::ActionTimelineSystem::Create());
			if (!asset.IsValid() || !scene || !system)
				return Check(false, "Camera spline test setup failed.");

			scene->SetActive(true);
			auto& world = scene->GetRegistry();

			// 메인 카메라
			const auto camEntity = scene->CreateEntity().GetEntityHandle();
			world.get_or_emplace<Engine::NameComponent>(camEntity).name = L"MainCam";
			auto& camTrans = world.get_or_emplace<Engine::TransformComponent>(camEntity);
			auto& camWorld = world.get_or_emplace<Engine::WorldTransformComponent>(camEntity);
			(void)camTrans.SetLocalPosition(Engine::vec3(0.0f));
			(void)camTrans.SetEulerRotation(Engine::vec3(0.0f));
			(void)Engine::TransformSystem::UpdateRootWorld(camTrans, camWorld);
			std::ignore = world.get_or_emplace<Engine::FlagComponent>(camEntity);
			auto& cam = world.emplace<Engine::CameraComponent>(camEntity);
			cam.FOV = 60.0f;
			cam.IsMainCamera = true;

			// 스플라인 엔티티 (월드 오프셋 (1, 2, 3))
			const auto splineEntity = scene->CreateEntity().GetEntityHandle();
			world.get_or_emplace<Engine::NameComponent>(splineEntity).name = L"CamSpline";
			auto& splineTrans = world.get_or_emplace<Engine::TransformComponent>(splineEntity);
			auto& splineWorld = world.get_or_emplace<Engine::WorldTransformComponent>(splineEntity);
			(void)splineTrans.SetLocalPosition(Engine::vec3(1.0f, 2.0f, 3.0f));
			(void)Engine::TransformSystem::UpdateRootWorld(splineTrans, splineWorld);
			std::ignore = world.get_or_emplace<Engine::FlagComponent>(splineEntity);
			auto& spline = world.emplace<Engine::SplineComponent>(splineEntity);
			spline.Closed = false;
			spline.Points = {
				Engine::SplinePoint{ Engine::vec3(0.0f, 0.0f, 0.0f), 0.0f },
				Engine::SplinePoint{ Engine::vec3(0.0f, 0.0f, 10.0f), 0.0f },
				Engine::SplinePoint{ Engine::vec3(10.0f, 0.0f, 10.0f), 0.0f }
			};

			// Look-At 타깃 엔티티
			const auto targetEntity = scene->CreateEntity().GetEntityHandle();
			world.get_or_emplace<Engine::NameComponent>(targetEntity).name = L"HeroTarget";
			auto& targetTrans = world.get_or_emplace<Engine::TransformComponent>(targetEntity);
			auto& targetWorld = world.get_or_emplace<Engine::WorldTransformComponent>(targetEntity);
			const Engine::vec3 heroWorldPos(5.0f, 2.0f, 8.0f);
			(void)targetTrans.SetLocalPosition(heroWorldPos);
			(void)Engine::TransformSystem::UpdateRootWorld(targetTrans, targetWorld);
			std::ignore = world.get_or_emplace<Engine::FlagComponent>(targetEntity);

			// 기대 스플라인 샘플 (t = 0.5)
			const std::vector<Engine::vec3> ctrlPts = {
				Engine::vec3(0.0f, 0.0f, 0.0f),
				Engine::vec3(0.0f, 0.0f, 10.0f),
				Engine::vec3(10.0f, 0.0f, 10.0f)
			};
			Engine::SplineSample expectedSample{};
			passed &= Check(Engine::SplineCurve::Evaluate(ctrlPts, false, 0.5, expectedSample),
				"SplineCurve::Evaluate must succeed at t = 0.5.");
			const Engine::vec3 expectedWorldPos = Engine::vec3(1.0f, 2.0f, 3.0f) + expectedSample.Position;

			// 타임라인 재생
			const auto owner = scene->CreateEntity().GetEntityHandle();
			std::ignore = world.get_or_emplace<Engine::FlagComponent>(owner);
			auto& timeline = world.emplace<Engine::ActionTimelineComponent>(owner);
			timeline.timelineAsset = asset;
			timeline.Play();

			// 틱 1: t = 0.25s (섹션 1 중간 -> spline t = 0.5, HeroTarget 주시)
			system->OnUpdate(world, { scene.get() }, 0.25f);
			const Engine::vec3 expectedLookDir = glm::normalize(heroWorldPos - expectedWorldPos);
			const Engine::vec3 actualLookFwd = glm::normalize(camTrans.GetLocalForward());
			passed &= Check(glm::distance(camTrans.position, expectedWorldPos) <= 1.0e-3f,
				"Spline camera must move along the spline world path at t = 0.5.");
			passed &= Check(glm::dot(actualLookFwd, expectedLookDir) >= 0.999f,
				"Spline camera with LookAtTargetName must orient its forward (+Z) toward the target.");
			passed &= Check(std::abs(cam.FOV - 50.0f) <= 1.0e-3f,
				"Spline camera must apply TargetFOV when specified.");

			// 틱 2: t = 0.75s (섹션 2 중간 -> spline t = 0.5, 접선 방향 바라보기)
			system->OnUpdate(world, { scene.get() }, 0.5f);
			const Engine::vec3 expectedTangentDir = glm::normalize(expectedSample.Tangent);
			const Engine::vec3 actualTangentFwd = glm::normalize(camTrans.GetLocalForward());
			passed &= Check(glm::distance(camTrans.position, expectedWorldPos) <= 1.0e-3f,
				"Second spline section must sample the spline path accurately.");
			passed &= Check(glm::dot(actualTangentFwd, expectedTangentDir) >= 0.999f,
				"Spline camera with FollowTangent must orient its forward (+Z) along the spline tangent.");

			// 틱 3: t = 1.05s -> 자연 종료 시 원래 위치·회전·FOV 복원
			system->OnUpdate(world, { scene.get() }, 0.3f);
			passed &= Check(glm::distance(camTrans.position, Engine::vec3(0.0f)) <= 1.0e-4f &&
				std::abs(cam.FOV - 60.0f) <= 1.0e-4f &&
				glm::dot(glm::normalize(camTrans.GetLocalForward()), Engine::vec3(0.0f, 0.0f, 1.0f)) >= 0.999f,
				"Ending spline camera timeline must restore original camera transform and FOV.");
		}
		return passed;
	}

	bool CheckCameraSequencerArchiveBackwardCompatibility()
	{
		struct ReflectionScope
		{
			bool Created = Engine::ReflectionRegistry::Create() != nullptr;
			~ReflectionScope()
			{
				if (Created) Engine::ReflectionRegistry::Destroy();
			}
		};

		ReflectionScope reflection;
		ResourceManagerScope resources;
		if (!reflection.Created || !resources.Created)
			return Check(false, "ReflectionRegistry or ResourceManager creation failed.");

		bool passed = true;
		{
			Engine::ActionTimelineCreateDesc desc;
			desc.Duration = 2.0f;
			Engine::TimelineCameraCutSection cut;
			cut.TrackName = "IntroCut";
			cut.StartTime = 0.1f;
			cut.EndTime = 0.9f;
			cut.TargetCameraName = "CutCam";
			cut.LocalPositionOffset = Engine::vec3(1.0f, 2.0f, 3.0f);
			cut.TargetFOV = 42.0f;
			cut.BlendInDuration = 0.25f;
			desc.CameraCutTracks.push_back(cut);

			Engine::TimelineCameraSplineSection spline;
			spline.TrackName = "OrbitPath";
			spline.StartTime = 0.9f;
			spline.EndTime = 1.9f;
			spline.SplineEntityName = "OrbitSpline";
			spline.StartT = 0.2f;
			spline.EndT = 0.8f;
			spline.LookAtTargetName = "Boss";
			desc.CameraSplineTracks.push_back(spline);

			auto* srcAsset = Engine::ResourceManager::Get().AddResource(
				L"CamSeqArchiveSrc", Engine::ActionTimelineAsset::Create(&desc)).Get();
			auto* dstAsset = Engine::ResourceManager::Get().AddResource(
				L"CamSeqArchiveDst", Engine::ActionTimelineAsset::Create(nullptr)).Get();
			if (!srcAsset || !dstAsset)
				return Check(false, "Failed to create ActionTimelineAsset for archive test.");

			archive::JsonArchive writeAr;
			std::string serialized;
			passed &= Check(static_cast<bool>(srcAsset->WriteArchive(writeAr)), "WriteArchive must succeed.");
			passed &= Check(static_cast<bool>(writeAr.ToJson(serialized)), "JsonArchive::ToJson must succeed.");

			archive::JsonArchive readAr;
			passed &= Check(static_cast<bool>(readAr.Parse(serialized)), "JsonArchive::Parse must succeed.");
			passed &= Check(static_cast<bool>(dstAsset->ReadArchive(readAr)), "ReadArchive must succeed.");
			passed &= Check(dstAsset->GetCameraCutTracks().size() == 1 &&
				dstAsset->GetCameraCutTracks()[0] == cut,
				"CameraCutTracks must round-trip cleanly through JsonArchive.");
			passed &= Check(dstAsset->GetCameraSplineTracks().size() == 1 &&
				dstAsset->GetCameraSplineTracks()[0] == spline,
				"CameraSplineTracks must round-trip cleanly through JsonArchive.");

			// 구버전 JSON(m_CameraCutTracks, m_CameraSplineTracks 누락) 하위 호환 로드 검증
			Engine::ActionTimelineCreateDesc legacyDesc;
			legacyDesc.Duration = 1.5f;
			legacyDesc.ClipName = "Idle";
			auto* legacySrc = Engine::ResourceManager::Get().AddResource(
				L"CamSeqLegacySrc", Engine::ActionTimelineAsset::Create(&legacyDesc)).Get();
			auto* legacyAsset = Engine::ResourceManager::Get().AddResource(
				L"CamSeqLegacyDst", Engine::ActionTimelineAsset::Create(nullptr)).Get();
			if (!legacySrc || !legacyAsset)
				return Check(false, "Failed to create legacy ActionTimelineAsset.");

			archive::JsonArchive legacyWriteAr;
			std::string legacyJson;
			passed &= Check(static_cast<bool>(legacySrc->WriteArchive(legacyWriteAr)), "Legacy WriteArchive must succeed.");
			passed &= Check(static_cast<bool>(legacyWriteAr.ToJson(legacyJson)), "Legacy ToJson must succeed.");

			const auto replaceKey = [](std::string& doc, const std::string& from, const std::string& to)
			{
				const auto pos = doc.find(from);
				if (pos != std::string::npos)
					doc.replace(pos, from.size(), to);
			};
			replaceKey(legacyJson, "\"m_CameraCutTracks\"", "\"m_IgnoredLegacyCut\"");
			replaceKey(legacyJson, "\"m_CameraSplineTracks\"", "\"m_IgnoredLegacySpline\"");

			archive::JsonArchive legacyReadAr;
			passed &= Check(static_cast<bool>(legacyReadAr.Parse(legacyJson)), "Legacy JSON parse must succeed.");
			passed &= Check(static_cast<bool>(legacyAsset->ReadArchive(legacyReadAr)),
				"Legacy ActionTimelineAsset without camera sequencer tracks must load without error.");
			passed &= Check(legacyAsset->GetCameraCutTracks().empty() &&
				legacyAsset->GetCameraSplineTracks().empty() &&
				std::abs(legacyAsset->GetDuration() - 1.5f) <= 1.0e-4f,
				"Legacy asset must default missing camera sequencer tracks to empty.");
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
	passed &= CheckGameplayCombat();
	passed &= CheckTimelineHitboxWindow();
	passed &= CheckInteractableQueries();
	passed &= CheckRuntimeSystemManifestRegistersCombat();
	passed &= CheckCameraCutTrackAndRestore();
	passed &= CheckCameraSplinePathAndLookAt();
	passed &= CheckCameraSequencerArchiveBackwardCompatibility();

	return passed ? 0 : 1;
}
