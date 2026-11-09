#include "ParticleSystem.h"
#include "ParticleEmitterComponent.h"
#include "ParticleEffectComponent.h"
#include "CompositeVFXComponent.h"
#include "ParticleEffect.h"
#include "CompositeEffect.h"
#include "WorldComponents.h"
#include "RenderComponents.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "RenderPass.h"
#include "RenderPassManager.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "Texture.h"
#include "Scene.h"
#include "PickingManager.h"
#include "PhysicsSystem.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

namespace
{
	// 결정론적 O(1) 비트 해시 함수
	inline f32 FastHash3D(int x, int y, int z)
	{
		int n = x * 374761393 + y * 668265263 + z * 3266489917;
		n = (n ^ (n >> 13)) * 1274126177;
		return static_cast<f32>(n & 0x7fffffff) / static_cast<f32>(0x7fffffff);
	}

	inline f32 Fade(f32 t)
	{
		return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
	}

	// 3D 연속 그래디언트 노이즈 (-1.0f ~ 1.0f)
	f32 Noise3D(f32 x, f32 y, f32 z, int seedOffset = 0)
	{
		int xi = static_cast<int>(std::floor(x));
		int yi = static_cast<int>(std::floor(y));
		int zi = static_cast<int>(std::floor(z));

		f32 xf = x - std::floor(x);
		f32 yf = y - std::floor(y);
		f32 zf = z - std::floor(z);

		f32 u = Fade(xf);
		f32 v = Fade(yf);
		f32 w = Fade(zf);

		auto Grad = [](int h, f32 gx, f32 gy, f32 gz) -> f32
		{
			int n = h & 15;
			f32 a = n < 8 ? gx : gy;
			f32 b = n < 4 ? gy : (n == 12 || n == 14 ? gx : gz);
			return ((n & 1) == 0 ? a : -a) + ((n & 2) == 0 ? b : -b);
		};

		auto H = [seedOffset](int rx, int ry, int rz) -> int
		{
			return static_cast<int>(FastHash3D(rx + seedOffset, ry + seedOffset * 31, rz + seedOffset * 67) * 255.0f);
		};

		f32 x00 = std::lerp(Grad(H(xi, yi, zi), xf, yf, zf), Grad(H(xi + 1, yi, zi), xf - 1.0f, yf, zf), u);
		f32 x10 = std::lerp(Grad(H(xi, yi + 1, zi), xf, yf - 1.0f, zf), Grad(H(xi + 1, yi + 1, zi), xf - 1.0f, yf - 1.0f, zf), u);
		f32 x01 = std::lerp(Grad(H(xi, yi, zi + 1), xf, yf, zf - 1.0f), Grad(H(xi + 1, yi, zi + 1), xf - 1.0f, yf, zf - 1.0f), u);
		f32 x11 = std::lerp(Grad(H(xi, yi + 1, zi + 1), xf, yf - 1.0f, zf - 1.0f), Grad(H(xi + 1, yi + 1, zi + 1), xf - 1.0f, yf - 1.0f, zf - 1.0f), u);

		f32 y0 = std::lerp(x00, x10, v);
		f32 y1 = std::lerp(x01, x11, v);
		return std::lerp(y0, y1, w);
	}

	// 3D 무발산(Divergence-Free) 컬 노이즈 벡터 계산 (유체 와류 시뮬레이션)
	vec3 ComputeCurlNoise3D(const vec3& pos, f32 frequency, f32 strength)
	{
		const vec3 p = pos * frequency;
		const f32 eps = 0.05f;
		const f32 invTwoEps = 1.0f / (2.0f * eps);

		// curl = (dFz/dy - dFy/dz, dFx/dz - dFz/dx, dFy/dx - dFx/dy)
		f32 dFz_dy = (Noise3D(p.x, p.y + eps, p.z, 203) - Noise3D(p.x, p.y - eps, p.z, 203)) * invTwoEps;
		f32 dFy_dz = (Noise3D(p.x, p.y, p.z + eps, 101) - Noise3D(p.x, p.y, p.z - eps, 101)) * invTwoEps;
		f32 curlX = dFz_dy - dFy_dz;

		f32 dFx_dz = (Noise3D(p.x, p.y, p.z + eps, 0) - Noise3D(p.x, p.y, p.z - eps, 0)) * invTwoEps;
		f32 dFz_dx = (Noise3D(p.x + eps, p.y, p.z, 203) - Noise3D(p.x - eps, p.y, p.z, 203)) * invTwoEps;
		f32 curlY = dFx_dz - dFz_dx;

		f32 dFy_dx = (Noise3D(p.x + eps, p.y, p.z, 101) - Noise3D(p.x - eps, p.y, p.z, 101)) * invTwoEps;
		f32 dFx_dy = (Noise3D(p.x, p.y + eps, p.z, 0) - Noise3D(p.x, p.y - eps, p.z, 0)) * invTwoEps;
		f32 curlZ = dFy_dx - dFx_dy;

		return vec3(curlX, curlY, curlZ) * strength;
	}
}

EResult Engine::ParticleSystem::Initialize(void* arg)
{
	return EResult::Success;
}

void Engine::ParticleSystem::Free()
{
}

Engine::ParticleSystem* Engine::ParticleSystem::Create(void* arg)
{
	auto* instance = new ParticleSystem();
	if (IsFailure(instance->Initialize(arg)))
	{
		instance->Free();
		delete instance;
		return nullptr;
	}
	return instance;
}

void Engine::ParticleSystem::OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
	for (Scene* scene : activeScenes)
	{
		SimulateSceneParticles(scene, dt);
	}
}

void Engine::ParticleSystem::SimulateSceneParticles(Scene* scene, f32 dt)
{
	if (!scene || dt <= 0.0f) return;

	auto view = scene->GetRegistry().view<ParticleEmitterComponent, WorldTransformComponent, FlagComponent>();
	for (auto [entity, emitter, transform, flag] : view.each())
	{
		if (!flag.IsActiveInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;
		emitter.ClearEvents();
		// 최초 1회 예열(Warmup) 시뮬레이션
		if (emitter.warmupTime > 0.0f && !emitter.warmupDone && emitter.playState == EParticlePlayState::Playing)
		{
			emitter.warmupDone = true;
			const f32 step = 0.0333f;
			const f32 maxWarmup = std::min(emitter.warmupTime, 10.0f); // 최대 10초 상한 방어
			for (f32 simTime = 0.0f; simTime < maxWarmup; simTime += step)
				UpdateEmitter(scene, emitter, transform, step);
		}
		UpdateEmitter(scene, emitter, transform, dt);
	}

	// 모듈형 파티클 이펙트 컴포넌트 시뮬레이션
	auto effectView = scene->GetRegistry().view<ParticleEffectComponent, WorldTransformComponent, FlagComponent>();
	for (auto [entity, effectComp, transform, flag] : effectView.each())
	{
		if (!flag.IsActiveInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;
		UpdateEffectComponent(scene, effectComp, transform, dt);
	}
}

void Engine::ParticleSystem::UpdateEmitter(Scene* scene, ParticleEmitterComponent& emitter, const WorldTransformComponent& transform, f32 dt)
{
	if (emitter.playState == EParticlePlayState::Paused) return;

	// 난수 생성기 초기화
	if (!emitter.rngInitialized)
	{
		if (emitter.useFixedSeed)
			emitter.rng.seed(emitter.seed);
		else
		{
			std::random_device rd;
			emitter.rng.seed(rd());
		}
		emitter.rngInitialized = true;
	}

	// 1. 파티클 방출 (Playing 상태일 때만)
	if (emitter.playState == EParticlePlayState::Playing)
	{
		// 지속 방출 계산
		if (emitter.emissionRate > 0.0f)
		{
			emitter.emissionAccumulator += dt;
			f32 spawnInterval = 1.0f / emitter.emissionRate;
			while (emitter.emissionAccumulator >= spawnInterval && emitter.particles.size() < emitter.maxParticles)
			{
				SpawnParticle(emitter, transform);
				emitter.emissionAccumulator -= spawnInterval;
			}
		}

		// 주기적 버스트 계산
		if (emitter.burstCount > 0 && emitter.burstInterval > 0.0f)
		{
			emitter.burstTimer += dt;
			if (emitter.burstTimer >= emitter.burstInterval)
			{
				emitter.burstTimer -= emitter.burstInterval;
				uint32 spawnBurst = std::min(emitter.burstCount, emitter.maxParticles - static_cast<uint32>(emitter.particles.size()));
				for (uint32 b = 0; b < spawnBurst; ++b)
					SpawnParticle(emitter, transform);
			}
		}
	}

	// 2. 파티클 물리 적분 및 수명 갱신
	uint32 raycastChecksThisFrame = 0;
	const bool isWorld = (emitter.simulationSpace == EParticleSimulationSpace::World);
	const mat4& worldMat = transform.worldMatrix;
	const mat4 invWorldMat = isWorld ? mat4(1.0f) : glm::inverse(worldMat);

	const vec3 totalGravity = emitter.gravity + vec3(0.0f, emitter.buoyancy, 0.0f);
	const vec3 localWind = isWorld ? emitter.wind : vec3(invWorldMat * vec4(emitter.wind, 0.0f));

	for (size_t i = 0; i < emitter.particles.size();)
	{
		auto& p = emitter.particles[i];
		p.age += dt;

		// 수명 다한 입자 O(1) 삭제
		if (!p.IsAlive())
		{
			std::swap(emitter.particles[i], emitter.particles.back());
			emitter.particles.pop_back();
			continue;
		}

		// 속도 적분 (중력, 부력 및 공기 저항, 바람)
		p.velocity += totalGravity * dt;
		if (emitter.drag > 0.0f)
		{
			const vec3 relVel = localWind - p.velocity;
			p.velocity += relVel * glm::clamp(emitter.drag * dt, 0.0f, 1.0f);
		}

		// 위치 이동 및 충돌 판정
		const vec3 prevPosWorld = isWorld ? p.position : vec3(worldMat * vec4(p.position, 1.0f));
		vec3 stepVelWorld = isWorld ? p.velocity : mat3(worldMat) * p.velocity;

		// 컬 노이즈 (Turbulence 와류) 합성
		if (emitter.enableCurlNoise && emitter.curlNoiseStrength > 0.0f)
		{
			const f32 scrollTime = p.age * emitter.curlNoiseScrollSpeed;
			const vec3 samplePos = prevPosWorld + vec3(scrollTime * 0.3f, scrollTime * 0.15f, scrollTime * 0.2f);
			const vec3 curlVel = ComputeCurlNoise3D(samplePos, emitter.curlNoiseFrequency, emitter.curlNoiseStrength);
			stepVelWorld += curlVel;
		}

		vec3 nextPosWorld = prevPosWorld + stepVelWorld * dt;

		if (emitter.collisionMode != EParticleCollisionMode::None)
		{
			bool hasCollision = false;
			vec3 hitNormal(0.0f, 1.0f, 0.0f);
			vec3 hitPoint = nextPosWorld;
			f32 hitFraction = 1.0f;

			if (emitter.collisionMode == EParticleCollisionMode::FloorPlane)
			{
				const f32 floorLevel = emitter.floorHeight + emitter.collisionRadius;
				if ((prevPosWorld.y >= floorLevel && nextPosWorld.y < floorLevel) || (nextPosWorld.y < floorLevel && stepVelWorld.y < 0.0f))
				{
					hasCollision = true;
					hitNormal = vec3(0.0f, 1.0f, 0.0f);
					const f32 dy = nextPosWorld.y - prevPosWorld.y;
					hitFraction = std::abs(dy) > 0.0001f ? glm::clamp((floorLevel - prevPosWorld.y) / dy, 0.0f, 1.0f) : 0.0f;
					hitPoint = glm::mix(prevPosWorld, nextPosWorld, hitFraction);
					hitPoint.y = floorLevel;
				}
			}
			else if (emitter.collisionMode == EParticleCollisionMode::PhysicsRaycast && scene)
			{
				if (raycastChecksThisFrame < emitter.maxCollisionChecksPerFrame)
				{
					const vec3 rayDelta = nextPosWorld - prevPosWorld;
					const f32 rayDist = glm::length(rayDelta);
					if (rayDist > 0.0001f)
					{
						Ray ray(prevPosWorld, rayDelta / rayDist);
						RaycastQuery query;
						query.maxDistance = rayDist + emitter.collisionRadius;
						query.includeTrigger = false;

						SceneRaycastHit hit;
						if (PhysicsSystem::Raycast(*scene, ray, hit, query))
						{
							hasCollision = true;
							hitNormal = hit.Hit.normal;
							hitPoint = hit.Hit.point + hitNormal * emitter.collisionRadius;
							hitFraction = glm::clamp(hit.Hit.distance / rayDist, 0.0f, 1.0f);
						}
						raycastChecksThisFrame++;
					}
				}
			}

			if (hasCollision)
			{
				// 충돌 이벤트 큐에 기록 (외부 모듈/서브이미터 소비용)
				if (emitter.collisionEvents.size() < 128)
				{
					emitter.collisionEvents.push_back({ hitPoint, hitNormal, stepVelWorld, p.age });
				}

				p.bounceCount++;
				if (emitter.collisionResponse == EParticleCollisionResponse::DieOnContact)
				{
					p.age = p.lifetime;
					std::swap(emitter.particles[i], emitter.particles.back());
					emitter.particles.pop_back();
					continue;
				}
				else if (emitter.collisionResponse == EParticleCollisionResponse::Stop)
				{
					p.velocity = vec3(0.0f);
					nextPosWorld = hitPoint;
				}
				else if (emitter.collisionResponse == EParticleCollisionResponse::Bounce)
				{
					const f32 vn = glm::dot(stepVelWorld, hitNormal);
					if (vn < 0.0f)
					{
						const vec3 vt = stepVelWorld - vn * hitNormal;
						const f32 frictionFactor = glm::clamp(1.0f - emitter.friction, 0.0f, 1.0f);
						const vec3 newVt = vt * frictionFactor;
						f32 newVn = -vn * glm::clamp(emitter.restitution, 0.0f, 1.0f);

						if (newVn < emitter.minBounceSpeed)
						{
							newVn = 0.0f;
						}

						const vec3 newVelWorld = newVt + newVn * hitNormal;
						p.velocity = isWorld ? newVelWorld : vec3(invWorldMat * vec4(newVelWorld, 0.0f));

						const f32 remainingDt = dt * (1.0f - hitFraction);
						nextPosWorld = hitPoint + newVelWorld * remainingDt;
					}
					else
					{
						nextPosWorld = hitPoint;
					}
				}
				else if (emitter.collisionResponse == EParticleCollisionResponse::Slide)
				{
					const f32 vn = glm::dot(stepVelWorld, hitNormal);
					if (vn < 0.0f)
					{
						const vec3 vt = stepVelWorld - vn * hitNormal;
						const f32 frictionFactor = glm::clamp(1.0f - emitter.friction * dt, 0.0f, 1.0f);
						const vec3 newVelWorld = vt * frictionFactor;
						p.velocity = isWorld ? newVelWorld : vec3(invWorldMat * vec4(newVelWorld, 0.0f));
					}
					nextPosWorld = hitPoint;
				}
			}
		}

		p.position = isWorld ? nextPosWorld : vec3(invWorldMat * vec4(nextPosWorld, 1.0f));
		p.rotation += p.angularVelocity * dt;

		// 수명 진행에 따른 색상 및 크기 보간
		const f32 t = p.GetNormalizedAge();
		p.color = glm::mix(emitter.colorStart, emitter.colorEnd, t);
		p.size = glm::mix(emitter.sizeStart, emitter.sizeEnd, t);

		++i;
	}
}

void Engine::ParticleSystem::SpawnParticle(ParticleEmitterComponent& emitter, const WorldTransformComponent& transform)
{
	if (emitter.particles.size() >= emitter.maxParticles) return;

	auto RandomFloat = [&](f32 minVal, f32 maxVal) -> f32
	{
		if (minVal >= maxVal) return minVal;
		std::uniform_real_distribution<f32> dist(minVal, maxVal);
		return dist(emitter.rng);
	};

	Particle p;
	p.age = 0.0f;
	p.lifetime = std::max(RandomFloat(emitter.lifetimeMin, emitter.lifetimeMax), 0.001f);
	p.size = emitter.sizeStart;
	p.color = emitter.colorStart;
	p.rotation = RandomFloat(0.0f, glm::two_pi<f32>());
	p.angularVelocity = RandomFloat(emitter.rotationSpeedMin, emitter.rotationSpeedMax);
	p.bounceCount = 0;

	// 원뿔(Cone) 방향 계산
	vec3 baseDir = glm::length2(emitter.emissionDirection) > 0.0001f ? glm::normalize(emitter.emissionDirection) : vec3(0.0f, 1.0f, 0.0f);
	f32 maxAngleRad = glm::radians(glm::clamp(emitter.spreadAngleDegrees, 0.0f, 180.0f));

	vec3 up = std::abs(baseDir.y) < 0.999f ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f);
	vec3 right = glm::normalize(glm::cross(up, baseDir));
	vec3 realUp = glm::cross(baseDir, right);

	f32 cosTheta = RandomFloat(std::cos(maxAngleRad), 1.0f);
	f32 sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
	f32 phi = RandomFloat(0.0f, glm::two_pi<f32>());

	vec3 coneDir = right * (sinTheta * std::cos(phi)) + realUp * (sinTheta * std::sin(phi)) + baseDir * cosTheta;
	f32 speed = RandomFloat(emitter.initialSpeedMin, emitter.initialSpeedMax);

	// 시뮬레이션 공간 분기
	if (emitter.simulationSpace == EParticleSimulationSpace::World)
	{
		mat3 worldRot = mat3(transform.worldMatrix);
		p.position = vec3(transform.worldMatrix[3]);
		p.velocity = (worldRot * coneDir) * speed;
	}
	else
	{
		p.position = vec3(0.0f);
		p.velocity = coneDir * speed;
	}

	emitter.particles.push_back(p);
}

void Engine::ParticleSystem::UpdateEffectComponent(Scene* scene, ParticleEffectComponent& effectComp, const WorldTransformComponent& transform, f32 dt)
{
	if (effectComp.playState == EParticlePlayState::Paused || effectComp.playState == EParticlePlayState::Stopped) return;
	ParticleEffect* effect = effectComp.effectHandle.Get();
	if (!effect) return;

	const auto& emitters = effect->GetEmitters();
	if (emitters.empty()) return;

	const f32 effectiveDt = dt * std::max(effectComp.playRate, 0.0f);
	if (!std::isfinite(effectiveDt) || effectiveDt <= 0.0f) return;

	// 인스턴스 배열 크기 동기화
	if (effectComp.instances.size() != emitters.size())
	{
		effectComp.instances.resize(emitters.size());
	}

	const mat4& worldMat = transform.worldMatrix;
	const f32 effectDuration = effect->GetPlaybackDuration();
	const auto emissionDelta = [&](const ParticleEmitterData& data, const ParticleEmitterInstance& instance)
	{
		const f64 start = std::isfinite(data.emissionStartTime) ? std::max(0.0f, data.emissionStartTime) : 0.0;
		const bool bounded = std::isfinite(data.emissionDuration) && data.emissionDuration > 0.0f;
		if (start == 0.0 && !bounded && effectDuration <= 0.0f) return effectiveDt;
		f64 end = bounded ? start + data.emissionDuration : instance.elapsedTime;
		if (effectDuration > 0.0f) end = std::min(end, static_cast<f64>(effectDuration));
		const f64 stepStart = instance.elapsedTime - effectiveDt;
		return static_cast<f32>(std::max(0.0, std::min(end, instance.elapsedTime) - std::max(start, stepStart)));
	};

	// 1단계: 지속(Continuous) 방출 및 물리/수명 적분
	for (size_t eIdx = 0; eIdx < emitters.size(); ++eIdx)
	{
		const auto& emitterData = emitters[eIdx];
		auto& inst = effectComp.instances[eIdx];
		inst.ClearEvents();
		inst.elapsedTime += effectiveDt;

		if (!emitterData.enabled) continue;

		// RNG 초기화
		if (!inst.rngInitialized)
		{
			const uint32 seed = effectComp.previewSeed ? *effectComp.previewSeed : std::random_device{}();
			inst.rng.seed(seed + static_cast<uint32>(eIdx * 997));
			inst.rngInitialized = true;
		}

		// 지속 방출 (Continuous 모드일 때)
		const float emitDt = emissionDelta(emitterData, inst);
		if (emitDt > 0.0f && emitterData.triggerMode == EParticleEmitterTrigger::Continuous)
		{
			if (emitterData.emissionRate > 0.0f)
			{
				inst.emissionAccumulator += emitDt;
				f32 spawnInterval = 1.0f / emitterData.emissionRate;
				while (inst.emissionAccumulator >= spawnInterval && inst.particles.size() < emitterData.maxParticles)
				{
					SpawnEffectParticle(emitterData, inst, transform);
					inst.emissionAccumulator -= spawnInterval;
				}
			}

			if (emitterData.burstCount > 0 && emitterData.burstInterval > 0.0f)
			{
				inst.burstTimer += emitDt;
				if (inst.burstTimer >= emitterData.burstInterval)
				{
					inst.burstTimer -= emitterData.burstInterval;
					uint32 spawnBurst = std::min(emitterData.burstCount, emitterData.maxParticles - static_cast<uint32>(inst.particles.size()));
					for (uint32 b = 0; b < spawnBurst; ++b)
						SpawnEffectParticle(emitterData, inst, transform);
				}
			}
		}

		// 물리 적분 및 수명 갱신
		const bool isWorld = (emitterData.simulationSpace == EParticleSimulationSpace::World);
		const mat4 invWorldMat = isWorld ? mat4(1.0f) : glm::inverse(worldMat);
		const vec3 totalGravity = emitterData.gravity + vec3(0.0f, emitterData.buoyancy, 0.0f);
		const vec3 localWind = isWorld ? emitterData.wind : vec3(invWorldMat * vec4(emitterData.wind, 0.0f));

		for (size_t i = 0; i < inst.particles.size();)
		{
			auto& p = inst.particles[i];
			p.age += effectiveDt;

			if (!p.IsAlive())
			{
				std::swap(inst.particles[i], inst.particles.back());
				inst.particles.pop_back();
				continue;
			}

			p.velocity += totalGravity * effectiveDt;
			if (emitterData.drag > 0.0f)
			{
				const vec3 relVel = localWind - p.velocity;
				p.velocity += relVel * glm::clamp(emitterData.drag * effectiveDt, 0.0f, 1.0f);
			}

			const vec3 prevPosWorld = isWorld ? p.position : vec3(worldMat * vec4(p.position, 1.0f));
			vec3 stepVelWorld = isWorld ? p.velocity : mat3(worldMat) * p.velocity;

			if (emitterData.enableCurlNoise && emitterData.curlNoiseStrength > 0.0f)
			{
				const f32 scrollTime = p.age * emitterData.curlNoiseScrollSpeed;
				const vec3 samplePos = prevPosWorld + vec3(scrollTime * 0.3f, scrollTime * 0.15f, scrollTime * 0.2f);
				const vec3 curlVel = ComputeCurlNoise3D(samplePos, emitterData.curlNoiseFrequency, emitterData.curlNoiseStrength);
				stepVelWorld += curlVel;
			}

			vec3 nextPosWorld = prevPosWorld + stepVelWorld * effectiveDt;

			// 충돌 판정
			if (emitterData.collisionMode != EParticleCollisionMode::None)
			{
				bool hasCollision = false;
				vec3 hitNormal(0.0f, 1.0f, 0.0f);
				vec3 hitPoint = nextPosWorld;
				f32 hitFraction = 1.0f;

				if (emitterData.collisionMode == EParticleCollisionMode::FloorPlane)
				{
					const f32 floorLevel = emitterData.floorHeight + emitterData.collisionRadius;
					if ((prevPosWorld.y >= floorLevel && nextPosWorld.y < floorLevel) || (nextPosWorld.y < floorLevel && stepVelWorld.y < 0.0f))
					{
						hasCollision = true;
						hitNormal = vec3(0.0f, 1.0f, 0.0f);
						const f32 dy = nextPosWorld.y - prevPosWorld.y;
						hitFraction = std::abs(dy) > 0.0001f ? glm::clamp((floorLevel - prevPosWorld.y) / dy, 0.0f, 1.0f) : 0.0f;
						hitPoint = glm::mix(prevPosWorld, nextPosWorld, hitFraction);
						hitPoint.y = floorLevel;
					}
				}
				else if (emitterData.collisionMode == EParticleCollisionMode::PhysicsRaycast && scene)
				{
					const vec3 rayDelta = nextPosWorld - prevPosWorld;
					const f32 rayDist = glm::length(rayDelta);
					if (rayDist > 0.0001f)
					{
						Ray ray(prevPosWorld, rayDelta / rayDist);
						RaycastQuery query;
						query.maxDistance = rayDist + emitterData.collisionRadius;
						query.includeTrigger = false;

						SceneRaycastHit hit;
						if (PhysicsSystem::Raycast(*scene, ray, hit, query))
						{
							hasCollision = true;
							hitNormal = hit.Hit.normal;
							hitPoint = hit.Hit.point + hitNormal * emitterData.collisionRadius;
							hitFraction = glm::clamp(hit.Hit.distance / rayDist, 0.0f, 1.0f);
						}
					}
				}

				if (hasCollision)
				{
					if (inst.collisionEvents.size() < 128)
					{
						inst.collisionEvents.push_back({ hitPoint, hitNormal, stepVelWorld, p.age });
					}

					p.bounceCount++;
					if (emitterData.collisionResponse == EParticleCollisionResponse::DieOnContact)
					{
						p.age = p.lifetime;
						std::swap(inst.particles[i], inst.particles.back());
						inst.particles.pop_back();
						continue;
					}
					else if (emitterData.collisionResponse == EParticleCollisionResponse::Stop)
					{
						p.velocity = vec3(0.0f);
						nextPosWorld = hitPoint;
					}
					else if (emitterData.collisionResponse == EParticleCollisionResponse::Bounce)
					{
						const f32 vn = glm::dot(stepVelWorld, hitNormal);
						if (vn < 0.0f)
						{
							const vec3 vt = stepVelWorld - vn * hitNormal;
							const f32 frictionFactor = glm::clamp(1.0f - emitterData.friction, 0.0f, 1.0f);
							const vec3 newVt = vt * frictionFactor;
							f32 newVn = -vn * glm::clamp(emitterData.restitution, 0.0f, 1.0f);
							if (newVn < emitterData.minBounceSpeed) newVn = 0.0f;

							const vec3 newVelWorld = newVt + newVn * hitNormal;
							p.velocity = isWorld ? newVelWorld : vec3(invWorldMat * vec4(newVelWorld, 0.0f));

							const f32 remainingDt = effectiveDt * (1.0f - hitFraction);
							nextPosWorld = hitPoint + newVelWorld * remainingDt;
						}
						else
						{
							nextPosWorld = hitPoint;
						}
					}
					else if (emitterData.collisionResponse == EParticleCollisionResponse::Slide)
					{
						const f32 vn = glm::dot(stepVelWorld, hitNormal);
						if (vn < 0.0f)
						{
							const vec3 vt = stepVelWorld - vn * hitNormal;
							const f32 frictionFactor = glm::clamp(1.0f - emitterData.friction * effectiveDt, 0.0f, 1.0f);
							const vec3 newVelWorld = vt * frictionFactor;
							p.velocity = isWorld ? newVelWorld : vec3(invWorldMat * vec4(newVelWorld, 0.0f));
						}
						nextPosWorld = hitPoint;
					}
				}
			}

			p.position = isWorld ? nextPosWorld : vec3(invWorldMat * vec4(nextPosWorld, 1.0f));
			p.rotation += p.angularVelocity * effectiveDt;

			const f32 t = p.GetNormalizedAge();
			p.color = glm::mix(emitterData.colorStart, emitterData.colorEnd, t);
			p.size = glm::mix(emitterData.sizeStart, emitterData.sizeEnd, t);

			++i;
		}
	}

	// 2단계: 부모-자식 이벤트 체이닝 (Event Triggered Spawning)
	for (size_t eIdx = 0; eIdx < emitters.size(); ++eIdx)
	{
		const auto& emitterData = emitters[eIdx];
		auto& inst = effectComp.instances[eIdx];
		if (!emitterData.enabled) continue;

		if (emissionDelta(emitterData, inst) > 0.0f && emitterData.triggerMode == EParticleEmitterTrigger::OnParentCollision)
		{
			const int32 pIdx = emitterData.parentEmitterIndex;
			if (pIdx >= 0 && pIdx < static_cast<int32>(emitters.size()) && pIdx != static_cast<int32>(eIdx))
			{
				const auto& parentEvents = effectComp.instances[pIdx].collisionEvents;
				for (const auto& ev : parentEvents)
				{
					const uint32 burst = emitterData.triggerBurstCount;
					vec3 inheritedVel(0.0f);
					if (emitterData.inheritParentVelocity)
						inheritedVel = ev.velocity * emitterData.inheritVelocityRatio;

					for (uint32 b = 0; b < burst; ++b)
					{
						if (inst.particles.size() >= emitterData.maxParticles) break;
						SpawnEffectParticle(emitterData, inst, transform, &ev.hitPosition, emitterData.alignToSurfaceNormal ? &ev.hitNormal : nullptr, &inheritedVel);
					}
				}
			}
		}
	}
}

void Engine::ParticleSystem::SpawnEffectParticle(const ParticleEmitterData& emitterData, ParticleEmitterInstance& inst, const WorldTransformComponent& transform, const vec3* overridePos, const vec3* overrideDir, const vec3* inheritedVel)
{
	if (inst.particles.size() >= emitterData.maxParticles) return;

	auto RandomFloat = [&](f32 minVal, f32 maxVal) -> f32
	{
		if (minVal >= maxVal) return minVal;
		std::uniform_real_distribution<f32> dist(minVal, maxVal);
		return dist(inst.rng);
	};

	Particle p;
	p.age = 0.0f;
	p.lifetime = std::max(RandomFloat(emitterData.lifetimeMin, emitterData.lifetimeMax), 0.001f);
	p.size = emitterData.sizeStart;
	p.color = emitterData.colorStart;
	p.rotation = RandomFloat(0.0f, glm::two_pi<f32>());
	p.angularVelocity = RandomFloat(emitterData.rotationSpeedMin, emitterData.rotationSpeedMax);
	p.bounceCount = 0;

	// 원뿔 방향 계산
	vec3 baseDir = overrideDir ? *overrideDir : emitterData.direction;
	if (glm::length2(baseDir) < 0.0001f) baseDir = vec3(0.0f, 1.0f, 0.0f);
	baseDir = glm::normalize(baseDir);

	f32 maxAngleRad = glm::radians(glm::clamp(emitterData.spreadAngleDegrees, 0.0f, 180.0f));

	vec3 up = std::abs(baseDir.y) < 0.999f ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f);
	vec3 right = glm::normalize(glm::cross(up, baseDir));
	vec3 realUp = glm::cross(baseDir, right);

	f32 cosTheta = RandomFloat(std::cos(maxAngleRad), 1.0f);
	f32 sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
	f32 phi = RandomFloat(0.0f, glm::two_pi<f32>());

	vec3 coneDir = right * (sinTheta * std::cos(phi)) + realUp * (sinTheta * std::sin(phi)) + baseDir * cosTheta;
	f32 speed = RandomFloat(emitterData.speedMin, emitterData.speedMax);
	vec3 velWorld = coneDir * speed;
	if (inheritedVel) velWorld += *inheritedVel;

	const bool isWorld = (emitterData.simulationSpace == EParticleSimulationSpace::World);
	const mat4& worldMat = transform.worldMatrix;
	const mat4 invWorldMat = isWorld ? mat4(1.0f) : glm::inverse(worldMat);

	if (overridePos)
	{
		const vec3 spawnPosWorld = *overridePos + (overrideDir ? (*overrideDir * (emitterData.collisionRadius * 1.2f)) : vec3(0.0f));
		p.position = isWorld ? spawnPosWorld : vec3(invWorldMat * vec4(spawnPosWorld, 1.0f));
		p.velocity = isWorld ? velWorld : vec3(invWorldMat * vec4(velWorld, 0.0f));
	}
	else
	{
		if (isWorld)
		{
			mat3 worldRot = mat3(worldMat);
			p.position = vec3(worldMat[3]);
			p.velocity = (worldRot * coneDir) * speed;
			if (inheritedVel) p.velocity += *inheritedVel;
		}
		else
		{
			p.position = vec3(0.0f);
			p.velocity = coneDir * speed;
			if (inheritedVel) p.velocity += vec3(invWorldMat * vec4(*inheritedVel, 0.0f));
		}
	}

	inst.particles.push_back(p);
}

void Engine::ParticleSystem::SubmitSceneParticles(Scene* scene, const CameraBuffer& cameraBuffer, uint32 targetPassId)
{
	if (!scene) return;

	auto& renderer = Renderer::Get();
	auto* renderPass = RenderPassManager::Get().GetRenderPassByID(targetPassId);

	auto& resourceManager = ResourceManager::Get();
	Mesh* defaultQuadMesh = resourceManager.GetResourceHandle<Mesh>(L"QuadMesh").Get();
	Texture* defaultTexture = resourceManager.GetResourceHandle<Texture>(L"Resources/Texture/Particle_SoftCircle.png").Get();
	if (!defaultTexture)
		defaultTexture = resourceManager.GetResourceHandle<Texture>(L"Resources/Texture/white1x1.png").Get();
	MaterialInterface* defaultMaterial = resourceManager.GetResourceHandle<MaterialInterface>(L"Resources/Material/ParticleMaterial_AlphaBlend.bammat.json").Get();
	if (!defaultMaterial)
		defaultMaterial = resourceManager.GetResourceHandle<MaterialInterface>(L"Resources/Material/SpriteMaterial.bammat.json").Get();

	const vec3 camRight = glm::normalize(vec3(cameraBuffer.invViewMatrix[0]));
	const vec3 camUp = glm::normalize(vec3(cameraBuffer.invViewMatrix[1]));
	const vec3 camForward = glm::normalize(vec3(cameraBuffer.invViewMatrix[2]));

	// 1. 단일 Emitter 컴포넌트 순회 제출
	auto view = scene->GetRegistry().view<ParticleEmitterComponent, WorldTransformComponent, FlagComponent>();
	for (auto [entity, emitter, transform, flag] : view.each())
	{
		if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;
		if (emitter.particles.empty()) continue;

		Mesh* mesh = emitter.meshHandle.Get() ? emitter.meshHandle.Get() : defaultQuadMesh;
		MaterialInterface* material = emitter.materialHandle.Get() ? emitter.materialHandle.Get() : defaultMaterial;
		Texture* texture = emitter.textureHandle.Get() ? emitter.textureHandle.Get() : defaultTexture;

		if (!mesh || !material || !texture) continue;
		if (!texture->GetRHITexture() || !mesh->GetIndexBuffer()) continue;
		if (renderPass && renderPass->GetAcceptedBlendModes() != EBlendMode::None && !renderPass->IsAcceptsBlendMode(material->GetBlendMode())) continue;

		const uint32 pickingId = PickingManager::Get().RegisterPickingEntity(scene, entity);
		const bool isWorldSpace = (emitter.simulationSpace == EParticleSimulationSpace::World);

		for (const auto& p : emitter.particles)
		{
			const vec3 worldPos = isWorldSpace ? p.position : vec3(transform.worldMatrix * vec4(p.position, 1.0f));

			const f32 cosR = std::cos(p.rotation);
			const f32 sinR = std::sin(p.rotation);
			const f32 halfW = p.size.x * 0.5f;
			const f32 halfH = p.size.y * 0.5f;

			mat4 worldMatrix(1.0f);
			if (emitter.billboardMode == ESpriteBillboardMode::None)
			{
				const vec3 axisX = vec3(cosR, sinR, 0.0f) * halfW;
				const vec3 axisY = vec3(-sinR, cosR, 0.0f) * halfH;
				worldMatrix[0] = vec4(axisX, 0.0f);
				worldMatrix[1] = vec4(axisY, 0.0f);
				worldMatrix[2] = vec4(0.0f, 0.0f, 1.0f, 0.0f);
				worldMatrix[3] = vec4(worldPos, 1.0f);
			}
			else
			{
				const vec3 axisX = (camRight * cosR + camUp * sinR) * halfW;
				const vec3 axisY = (-camRight * sinR + camUp * cosR) * halfH;
				worldMatrix[0] = vec4(axisX, 0.0f);
				worldMatrix[1] = vec4(axisY, 0.0f);
				worldMatrix[2] = vec4(camForward, 0.0f);
				worldMatrix[3] = vec4(worldPos, 1.0f);
			}

			vec4 uvTransform = vec4(0.0f, 0.0f, 1.0f, 1.0f);
			if (emitter.subUVColumns > 1 || emitter.subUVRows > 1)
			{
				const uint32 totalFrames = emitter.subUVColumns * emitter.subUVRows;
				const f32 normAge = p.GetNormalizedAge();
				const f32 cycledAge = std::fmod(normAge * (std::max)(1.0f, emitter.subUVCycles), 1.0f);
				const uint32 frameIndex = (std::min)(static_cast<uint32>(cycledAge * totalFrames), totalFrames - 1);
				const uint32 col = frameIndex % emitter.subUVColumns;
				const uint32 row = frameIndex / emitter.subUVColumns;
				const f32 uSize = 1.0f / static_cast<f32>(emitter.subUVColumns);
				const f32 vSize = 1.0f / static_cast<f32>(emitter.subUVRows);
				uvTransform = vec4(col * uSize, row * vSize, uSize, vSize);
			}

			SpriteDrawCommand command;
			command.mesh = mesh;
			command.texture = texture;
			command.material = material;
			command.color = p.color;
			command.uvTransform = uvTransform;
			command.worldMatrix = worldMatrix;
			command.pickingId = pickingId;

			renderer.SubmitSprite(command, targetPassId);
		}
	}

	// 2. 모듈형 파티클 이펙트 컴포넌트 순회 제출
	auto effectView = scene->GetRegistry().view<ParticleEffectComponent, WorldTransformComponent, FlagComponent>();
	for (auto [entity, effectComp, transform, flag] : effectView.each())
	{
		if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;
		ParticleEffect* effect = effectComp.effectHandle.Get();
		if (!effect) continue;

		const auto& emitters = effect->GetEmitters();
		const uint32 pickingId = PickingManager::Get().RegisterPickingEntity(scene, entity);

		for (size_t eIdx = 0; eIdx < emitters.size() && eIdx < effectComp.instances.size(); ++eIdx)
		{
			const auto& emitterData = emitters[eIdx];
			const auto& inst = effectComp.instances[eIdx];
			if (!emitterData.enabled || inst.particles.empty()) continue;

			Mesh* mesh = emitterData.meshHandle.Get() ? emitterData.meshHandle.Get() : defaultQuadMesh;
			MaterialInterface* material = emitterData.materialHandle.Get() ? emitterData.materialHandle.Get() : defaultMaterial;
			Texture* texture = emitterData.textureHandle.Get() ? emitterData.textureHandle.Get() : defaultTexture;

			if (!mesh || !material || !texture) continue;
			if (!texture->GetRHITexture() || !mesh->GetIndexBuffer()) continue;
			if (renderPass && renderPass->GetAcceptedBlendModes() != EBlendMode::None && !renderPass->IsAcceptsBlendMode(material->GetBlendMode())) continue;

			const bool isWorldSpace = (emitterData.simulationSpace == EParticleSimulationSpace::World);

			for (const auto& p : inst.particles)
			{
				const vec3 worldPos = isWorldSpace ? p.position : vec3(transform.worldMatrix * vec4(p.position, 1.0f));

				mat4 worldMatrix(1.0f);
				if (emitterData.alignToSurfaceDisk)
				{
					const f32 halfW = p.size.x * 0.5f;
					const f32 halfH = p.size.y * 0.5f;
					worldMatrix[0] = vec4(halfW, 0.0f, 0.0f, 0.0f);
					worldMatrix[1] = vec4(0.0f, 0.0f, halfH, 0.0f);
					worldMatrix[2] = vec4(0.0f, 1.0f, 0.0f, 0.0f);
					worldMatrix[3] = vec4(worldPos + vec3(0.0f, 0.015f, 0.0f), 1.0f);
				}
				else
				{
					const f32 cosR = std::cos(p.rotation);
					const f32 sinR = std::sin(p.rotation);
					const f32 halfW = p.size.x * 0.5f;
					const f32 halfH = p.size.y * 0.5f;

					if (emitterData.billboardMode == ESpriteBillboardMode::None)
					{
						const vec3 axisX = vec3(cosR, sinR, 0.0f) * halfW;
						const vec3 axisY = vec3(-sinR, cosR, 0.0f) * halfH;
						worldMatrix[0] = vec4(axisX, 0.0f);
						worldMatrix[1] = vec4(axisY, 0.0f);
						worldMatrix[2] = vec4(0.0f, 0.0f, 1.0f, 0.0f);
						worldMatrix[3] = vec4(worldPos, 1.0f);
					}
					else
					{
						const vec3 axisX = (camRight * cosR + camUp * sinR) * halfW;
						const vec3 axisY = (-camRight * sinR + camUp * cosR) * halfH;
						worldMatrix[0] = vec4(axisX, 0.0f);
						worldMatrix[1] = vec4(axisY, 0.0f);
						worldMatrix[2] = vec4(camForward, 0.0f);
						worldMatrix[3] = vec4(worldPos, 1.0f);
					}
				}

				vec4 uvTransform = vec4(0.0f, 0.0f, 1.0f, 1.0f);
				if (emitterData.subUVColumns > 1 || emitterData.subUVRows > 1)
				{
					const uint32 totalFrames = emitterData.subUVColumns * emitterData.subUVRows;
					const f32 normAge = p.GetNormalizedAge();
					const f32 cycledAge = std::fmod(normAge * (std::max)(1.0f, emitterData.subUVCycles), 1.0f);
					const uint32 frameIndex = (std::min)(static_cast<uint32>(cycledAge * totalFrames), totalFrames - 1);
					const uint32 col = frameIndex % emitterData.subUVColumns;
					const uint32 row = frameIndex / emitterData.subUVColumns;
					const f32 uSize = 1.0f / static_cast<f32>(emitterData.subUVColumns);
					const f32 vSize = 1.0f / static_cast<f32>(emitterData.subUVRows);
					uvTransform = vec4(col * uSize, row * vSize, uSize, vSize);
				}

				SpriteDrawCommand command;
				command.mesh = mesh;
				command.texture = texture;
				command.material = material;
				command.color = p.color;
				command.uvTransform = uvTransform;
				command.worldMatrix = worldMatrix;
				command.pickingId = pickingId;

				renderer.SubmitSprite(command, targetPassId);
			}
		}
	}

	// 3. 복합 비주얼 이펙트(CompositeVFXComponent) 내의 파티클 트랙 순회 제출
	auto vfxView = scene->GetRegistry().view<CompositeVFXComponent, WorldTransformComponent, FlagComponent>();
	for (auto [entity, vfxComp, transform, flag] : vfxView.each())
	{
		if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;
		CompositeEffect* effect = vfxComp.effectHandle.Get();
		if (!effect) continue;

		const auto& pTracks = effect->GetParticleTracks();
		const uint32 pickingId = PickingManager::Get().RegisterPickingEntity(scene, entity);

		for (size_t tIdx = 0; tIdx < pTracks.size() && tIdx < vfxComp.particleRuntimes.size(); ++tIdx)
		{
			const auto& track = pTracks[tIdx];
			const auto& rt = vfxComp.particleRuntimes[tIdx];
			if (!track.enabled || track.isMuted) continue;

			ParticleEffect* subEffect = track.effectHandle.Get();
			if (!subEffect) continue;

			const auto& emitters = subEffect->GetEmitters();
			for (size_t eIdx = 0; eIdx < emitters.size() && eIdx < rt.emitterInstances.size(); ++eIdx)
			{
				const auto& emitterData = emitters[eIdx];
				const auto& inst = rt.emitterInstances[eIdx];
				if (!emitterData.enabled || inst.particles.empty()) continue;

				Mesh* mesh = emitterData.meshHandle.Get() ? emitterData.meshHandle.Get() : defaultQuadMesh;
				MaterialInterface* material = emitterData.materialHandle.Get() ? emitterData.materialHandle.Get() : defaultMaterial;
				Texture* texture = emitterData.textureHandle.Get() ? emitterData.textureHandle.Get() : defaultTexture;

				if (!mesh || !material || !texture) continue;
				if (!texture->GetRHITexture() || !mesh->GetIndexBuffer()) continue;
				if (renderPass && renderPass->GetAcceptedBlendModes() != EBlendMode::None && !renderPass->IsAcceptsBlendMode(material->GetBlendMode())) continue;

				const bool isWorldSpace = (emitterData.simulationSpace == EParticleSimulationSpace::World);

				for (const auto& p : inst.particles)
				{
					const vec3 worldPos = isWorldSpace ? p.position : vec3(transform.worldMatrix * vec4(p.position, 1.0f));

					mat4 worldMatrix(1.0f);
					if (emitterData.alignToSurfaceDisk)
					{
						const f32 halfW = p.size.x * 0.5f;
						const f32 halfH = p.size.y * 0.5f;
						worldMatrix[0] = vec4(halfW, 0.0f, 0.0f, 0.0f);
						worldMatrix[1] = vec4(0.0f, 0.0f, halfH, 0.0f);
						worldMatrix[2] = vec4(0.0f, 1.0f, 0.0f, 0.0f);
						worldMatrix[3] = vec4(worldPos + vec3(0.0f, 0.015f, 0.0f), 1.0f);
					}
					else
					{
						const f32 cosR = std::cos(p.rotation);
						const f32 sinR = std::sin(p.rotation);
						const f32 halfW = p.size.x * 0.5f;
						const f32 halfH = p.size.y * 0.5f;

						if (emitterData.billboardMode == ESpriteBillboardMode::None)
						{
							const vec3 axisX = vec3(cosR, sinR, 0.0f) * halfW;
							const vec3 axisY = vec3(-sinR, cosR, 0.0f) * halfH;
							worldMatrix[0] = vec4(axisX, 0.0f);
							worldMatrix[1] = vec4(axisY, 0.0f);
							worldMatrix[2] = vec4(0.0f, 0.0f, 1.0f, 0.0f);
							worldMatrix[3] = vec4(worldPos, 1.0f);
						}
						else
						{
							const vec3 axisX = (camRight * cosR + camUp * sinR) * halfW;
							const vec3 axisY = (-camRight * sinR + camUp * cosR) * halfH;
							worldMatrix[0] = vec4(axisX, 0.0f);
							worldMatrix[1] = vec4(axisY, 0.0f);
							worldMatrix[2] = vec4(camForward, 0.0f);
							worldMatrix[3] = vec4(worldPos, 1.0f);
						}
					}

					vec4 uvTransform = vec4(0.0f, 0.0f, 1.0f, 1.0f);
					if (emitterData.subUVColumns > 1 || emitterData.subUVRows > 1)
					{
						const uint32 totalFrames = emitterData.subUVColumns * emitterData.subUVRows;
						const f32 normAge = p.GetNormalizedAge();
						const f32 cycledAge = std::fmod(normAge * (std::max)(1.0f, emitterData.subUVCycles), 1.0f);
						const uint32 frameIndex = (std::min)(static_cast<uint32>(cycledAge * totalFrames), totalFrames - 1);
						const uint32 col = frameIndex % emitterData.subUVColumns;
						const uint32 row = frameIndex / emitterData.subUVColumns;
						const f32 uSize = 1.0f / static_cast<f32>(emitterData.subUVColumns);
						const f32 vSize = 1.0f / static_cast<f32>(emitterData.subUVRows);
						uvTransform = vec4(col * uSize, row * vSize, uSize, vSize);
					}

					SpriteDrawCommand command;
					command.mesh = mesh;
					command.texture = texture;
					command.material = material;
					command.color = p.color;
					command.uvTransform = uvTransform;
					command.worldMatrix = worldMatrix;
					command.pickingId = pickingId;

					renderer.SubmitSprite(command, targetPassId);
				}
			}
		}
	}
}

void Engine::ParticleSystem::OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
	auto& renderer = Renderer::Get();
	const auto geometryTargets = renderer.CollectGeometryViewportCameras();
	if (geometryTargets.empty()) return;

	for (Scene* scene : activeScenes)
	{
		if (!scene) continue;
		for (const auto& target : geometryTargets)
		{
			SubmitSceneParticles(scene, target.cameraBuffer, target.renderPass->GetID());
		}
	}
}
