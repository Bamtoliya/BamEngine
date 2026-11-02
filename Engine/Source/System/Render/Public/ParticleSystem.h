#pragma once

#include "System.h"

BEGIN(Engine)
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

private:
	void UpdateEmitter(struct ParticleEmitterComponent& emitter, const struct WorldTransformComponent& transform, f32 dt);
	void SpawnParticle(struct ParticleEmitterComponent& emitter, const struct WorldTransformComponent& transform);
};
END