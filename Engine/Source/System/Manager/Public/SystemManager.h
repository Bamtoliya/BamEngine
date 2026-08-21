#pragma once
#include "Base.h"
#include "Systems.h"

BEGIN(Engine)
class Scene;
class ENGINE_API SystemManager : public Base
{
	DECLARE_SINGLETON(SystemManager)
protected:
	SystemManager() {}
	virtual ~SystemManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free();
public:
	void FixedUpdate(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt);
	void Update(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt);
	void LateUpdate(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt);
	void Sumbit(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt);
public:
	template<typename T, typename... Args>
	T* AddSystem(Args&&... args);

	template<typename T>
	void RemoveSystem();

	template<typename T>
	T* GetSystem();
public:
	void InitializeSystems();
	void ClearSystems();
private:
	vector<ISystem*> m_Systems;
	unordered_map<entt::id_type, ISystem*> m_SystemMap;
};

#pragma region Template
template<typename T, typename ...Args>
inline T* SystemManager::AddSystem(Args && ...args)
{
	static_assert(std::is_base_of<ISystem, T>::value, "T must be derived from ISystem");

	entt::id_type typeId = entt::type_hash<T>::value();
	if (m_SystemMap.find(typeId) != m_SystemMap.end())
	{
		return static_cast<T*>(m_SystemMap[typeId]);
	}
	T* newSystem = T::Create(std::forward<Args>(args)...);
	m_Systems.push_back(newSystem);
	m_SystemMap[typeId] = newSystem;
	return newSystem;
}

template<typename T>
inline void SystemManager::RemoveSystem()
{
	entt::id_type typeId = entt::type_hash<T>::value();
	auto it = m_SystemMap.find(typeId);
	if (it != m_SystemMap.end())
	{
		ISystem* system = it->second;
		m_Systems.erase(std::remove(m_Systems.begin(), m_Systems.end(), system), m_Systems.end());
		Safe_Delete(system);
		m_SystemMap.erase(it);
	}
}

template<typename T>
inline T* SystemManager::GetSystem()
{
	entt::id_type typeId = entt::type_hash<T>::value();
	auto it = m_SystemMap.find(typeId);
	if (it != m_SystemMap.end())
	{
		return static_cast<T*>(it->second);
	}
	return nullptr;
}
#pragma endregion
END