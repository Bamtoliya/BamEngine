#pragma once

#include "Engine_Includes.h"

BEGIN(Engine)

// Forward declaration
class Scene;

class ENGINE_API Entity
{
protected:
	Entity() {}
	Entity(entt::entity handle) : m_EntityHandle(handle) {}
	Entity(entt::entity handle, Scene* scene) : m_EntityHandle(handle), m_Scene(scene) {}
	virtual ~Entity() {}
public:
	static Entity* Create(entt::entity handle, Scene* scene) { return new Entity(handle, scene); }
	template<typename T, typename... Args>
	T& AddComponent(Args&&... args);
	template<typename T>
	T& GetComponent();

	entt::entity GetEntityHandle() const { return m_EntityHandle; }
private:
	entt::entity m_EntityHandle = entt::null;
	Scene* m_Scene = nullptr;
};

END