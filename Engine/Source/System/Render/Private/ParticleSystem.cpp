#include "ParticleSystem.h"
#include "Components/Render/ParticleEmitterComponent.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "RenderPass.h"
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
		if (!scene) continue;

		auto view = scene->GetRegistry().view<ParticleEmitterComponent, WorldTransformComponent, FlagComponent>();
		for (auto [entity, emitter, transform, flag] : view.each())
		{
			if (!flag.IsActiveInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;
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

void Engine::ParticleSystem::OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
	auto& renderer = Renderer::Get();

	const auto geometryTargets = renderer.CollectGeometryViewportCameras();
	if (geometryTargets.empty()) return;

	auto& resourceManager = ResourceManager::Get();
	Mesh* defaultQuadMesh = resourceManager.GetResourceHandle<Mesh>(L"QuadMesh").Get();

	for (Scene* scene : activeScenes)
	{
		if (!scene) continue;

		auto view = scene->GetRegistry().view<ParticleEmitterComponent, WorldTransformComponent, FlagComponent>();
		for (auto [entity, emitter, transform, flag] : view.each())
		{
			if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;
			if (emitter.particles.empty()) continue;

			Mesh* mesh = emitter.meshHandle.Get() ? emitter.meshHandle.Get() : defaultQuadMesh;
			MaterialInterface* material = emitter.materialHandle.Get();
			Texture* texture = emitter.textureHandle.Get();

			if (!mesh || !material || !texture) continue;
			if (!texture->GetRHITexture() || !mesh->GetIndexBuffer()) continue;

			const uint32 pickingId = PickingManager::Get().RegisterPickingEntity(scene, entity);
			const bool isWorldSpace = (emitter.simulationSpace == EParticleSimulationSpace::World);

			for (const auto& target : geometryTargets)
			{
				if (!target.renderPass->IsAcceptsBlendMode(material->GetBlendMode())) continue;

				// 빌보드 기저 벡터를 루프 밖에서 1회만 계산
				vec3 camRight = glm::normalize(vec3(target.cameraBuffer.invViewMatrix[0]));
				vec3 camUp = glm::normalize(vec3(target.cameraBuffer.invViewMatrix[1]));
				vec3 camForward = glm::normalize(vec3(target.cameraBuffer.invViewMatrix[2]));

				for (const auto& p : emitter.particles)
				{
					const vec3 worldPos = isWorldSpace ? p.position : vec3(transform.worldMatrix * vec4(p.position, 1.0f));

					// 회전 및 스케일이 적용된 4x4 월드 행렬 직접 조립 (O(1))
					const f32 cosR = std::cos(p.rotation);
					const f32 sinR = std::sin(p.rotation);
					const f32 halfW = p.size.x * 0.5f;
					const f32 halfH = p.size.y * 0.5f;

					vec3 axisX = (camRight * cosR + camUp * sinR) * halfW;
					vec3 axisY = (-camRight * sinR + camUp * cosR) * halfH;

					mat4 worldMatrix(1.0f);
					worldMatrix[0] = vec4(axisX, 0.0f);
					worldMatrix[1] = vec4(axisY, 0.0f);
					worldMatrix[2] = vec4(camForward, 0.0f);
					worldMatrix[3] = vec4(worldPos, 1.0f);

					SpriteDrawCommand command;
					command.mesh = mesh;
					command.texture = texture;
					command.material = material;
					command.color = p.color;
					command.uvTransform = vec4(0.0f, 0.0f, 1.0f, 1.0f);
					command.worldMatrix = worldMatrix;
					command.pickingId = pickingId;

					renderer.SubmitSprite(command, target.renderPass->GetID());
				}
			}
		}
	}
}
