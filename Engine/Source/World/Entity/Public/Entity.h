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
public:
	using WeakRef = std::weak_ptr<Entity*>;

	Entity(const Entity&) = delete;
	Entity& operator=(const Entity&) = delete;

	virtual ~Entity()
	{
		if (m_Observer) *m_Observer = nullptr;
	}

	WeakRef GetWeakRef()
	{
		if (!m_Observer) m_Observer = std::make_shared<Entity*>(this);
		return m_Observer;
	}

	static Entity* Resolve(const WeakRef& ref)
	{
		const auto observer = ref.lock();
		return observer ? *observer : nullptr;
	}

	static Entity* Create(entt::entity handle, Scene* scene) { return new Entity(handle, scene); }
	template<typename T, typename... Args>
	T& AddComponent(Args&&... args);
	template<typename T>
	T& GetComponent();

	entt::entity GetEntityHandle() const { return m_EntityHandle; }
	Scene* GetScene() const { return m_Scene; }
private:
	entt::entity m_EntityHandle = entt::null;
	Scene* m_Scene = nullptr;
	std::shared_ptr<Entity*> m_Observer;
};

END