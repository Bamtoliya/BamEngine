#pragma once
#include "PrototypeManager.h"
#include "GameObject.h"
#include "Component.h"
IMPLEMENT_SINGLETON(PrototypeManager)

#pragma region Constructor&Destructor
EResult PrototypeManager::Initialize(void* arg)
{
	return EResult::Success;
}

void PrototypeManager::Free()
{
	for (auto& pair : m_GameObjectPrototypeMap)
		Safe_Release(pair.second);
	m_GameObjectPrototypeMap.clear();
	for (auto& pair : m_ComponentPrototypeMap)
		Safe_Release(pair.second);
	m_ComponentPrototypeMap.clear();
}
#pragma endregion

#pragma region Prototype Management
EResult PrototypeManager::RegisterGameObjectPrototype(const wstring& prototypeTag, GameObject* prototype)
{
	if(!prototype || m_GameObjectPrototypeMap.find(prototypeTag) != m_GameObjectPrototypeMap.end())
		return EResult::InvalidArgument;

	m_GameObjectPrototypeMap.emplace(prototypeTag, prototype);
	//Safe_AddRef(prototype);
	return EResult::Success;
}

GameObject* PrototypeManager::CreateGameObjectPrototype(const wstring& prototypeTag, void* arg)
{
	auto	iter = m_GameObjectPrototypeMap.find(prototypeTag);
	if (iter == m_GameObjectPrototypeMap.end())
		return nullptr;
	return iter->second->Clone(arg);
}
EResult PrototypeManager::RegisterComponentPrototype(const wstring& prototypeTag, Component* prototype)
{
	if (!prototype || m_ComponentPrototypeMap.find(prototypeTag) != m_ComponentPrototypeMap.end())
		return EResult::InvalidArgument;
	m_ComponentPrototypeMap.emplace(prototypeTag, prototype);
	//Safe_AddRef(prototype);
	return EResult::Success;
}

Component* PrototypeManager::CreateComponentPrototype(const wstring& prototypeTag, GameObject* owner, void* arg)
{
	auto	iter = m_ComponentPrototypeMap.find(prototypeTag);
	if (iter == m_ComponentPrototypeMap.end())
		return nullptr;
	return iter->second->Clone(owner, arg);
}

#pragma endregion

