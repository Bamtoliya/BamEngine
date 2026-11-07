#pragma once

#include "System.h"
#include "CameraBuffer.h"

BEGIN(Engine)
class Scene;

class ENGINE_API ParticleSystem : public ISystem
{
private:
	ParticleSystem() = default;
	virtual ~ParticleSystem() = default;
	EResult Initialize(void* arg = nullptr);

public:
	static ParticleSystem* Create(void* arg = nullptr);
	virtual void Free() override;

public:
	virtual void OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;
	virtual void OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;

	// 독립 씬(프리뷰 뷰포트 등)용 시뮬레이션 및 렌더링 제출 정적 함수
	static void SimulateSceneParticles(Scene* scene, f32 dt);
	static void SubmitSceneParticles(Scene* scene, const CameraBuffer& cameraBuffer, uint32 targetPassId);

private:
	static void UpdateEmitter(Scene* scene, struct ParticleEmitterComponent& emitter, const struct WorldTransformComponent& transform, f32 dt);
	static void SpawnParticle(struct ParticleEmitterComponent& emitter, const struct WorldTransformComponent& transform);

	static void UpdateEffectComponent(Scene* scene, struct ParticleEffectComponent& effectComp, const struct WorldTransformComponent& transform, f32 dt);
	static void SpawnEffectParticle(const struct ParticleEmitterData& emitterData, struct ParticleEmitterInstance& inst, const struct WorldTransformComponent& transform, const vec3* overridePos = nullptr, const vec3* overrideDir = nullptr, const vec3* inheritedVel = nullptr);

	virtual uint8 GetExecutionPhases() const override { return static_cast<uint8>(ESystemPhase::Presentation); }
};
END
