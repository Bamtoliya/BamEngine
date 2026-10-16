#pragma once

#include "Engine_Includes.h"
#include "System.h"
#include "Components/Core/TransformComponent.h"

BEGIN(Engine)

class ENGINE_API TransformSystem : public ISystem
{
private:
	TransformSystem() {};
	virtual ~TransformSystem() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static TransformSystem* Create(void* arg = nullptr);
	virtual void Free() override;
public:
	virtual void OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*> activeScenes, f32 dt) override;
	// Commits synchronized components and world caches only after the complete graph succeeds.
	static bool UpdateHierarchy(entt::registry& registry);
	static bool TryGetWorldMatrix(const entt::registry& registry, entt::entity entity, glm::mat4& matrix);
	static bool TryComposeWorld(const TransformComponent& transform, const glm::mat4& parent, glm::mat4& matrix);
	static bool TryGetLocalMatrix(const glm::mat4& parent, ETransformAbsolute flags,
		const glm::mat4& desiredWorld, glm::dmat4& local);
	static bool TryAssignLocalMatrix(TransformComponent& candidate, const glm::dmat4& local);
	static bool TryCommitTransform(entt::registry& registry, entt::entity entity, TransformComponent candidate);
	static bool TrySetAbsolute(entt::registry& registry, entt::entity entity, ETransformAbsolute flags);
private:

};
END
