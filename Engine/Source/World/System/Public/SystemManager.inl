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
	if (!newSystem) return nullptr;

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
		Safe_Release(system);
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