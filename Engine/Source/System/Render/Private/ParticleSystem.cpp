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
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

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
					UpdateEmitter(emitter, transform, step);
			}
			UpdateEmitter(emitter, transform, dt);
		}
	}
}

void Engine::ParticleSystem::UpdateEmitter(ParticleEmitterComponent& emitter, const WorldTransformComponent& transform, f32 dt)
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

		// 속도 및 위치 적분
		p.velocity += emitter.gravity * dt;
		if (emitter.drag > 0.0f)
			p.velocity *= glm::clamp(1.0f - emitter.drag * dt, 0.0f, 1.0f);

		p.position += p.velocity * dt;
		p.rotation += p.angularVelocity * dt;

		// 수명 진행에 따른 색상 및 크기 보간
		f32 t = p.GetNormalizedAge();
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