#include "Entity.h"
#include "World/Scene/Public/Scene.h"

template<typename T, typename... Args>
T& Entity::AddComponent(Args&&... args)
{
	return m_Scene->AddComponent<T>(*this, std::forward<Args>(args)...);
}

template<typename T>
T& Entity::GetComponent()
{
	return m_Scene->GetComponent<T>(*this);
}