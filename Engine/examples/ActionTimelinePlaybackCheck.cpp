#include "ActionTimelinePlayback.h"
#include "CompositeVFXComponent.h"
#include "CompositeVFXSystem.h"
#include "FlagComponent.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "WorldTransformComponent.h"
#include <cmath>
#include <iostream>
#include <memory>

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

	passed &= CheckParticleTimeline();

	return passed ? 0 : 1;
}
