#pragma once

#include "ComponentRegistry.h"

IMPLEMENT_SINGLETON(ComponentRegistry)


#pragma region Constructor&Destructor
EResult ComponentRegistry::Initialize(void* arg)
{
	return EResult::Success;
}

void ComponentRegistry::Free()
{
	m_TypeMap.clear();
	m_NameMap.clear();
}
Component* ComponentRegistry::Create(const wstring& name, void* arg)
{
	auto iter = m_NameMap.find(name);
	if(iter != m_NameMap.end())
	{
		return iter->second(arg);
	}
	return nullptr;
}
#pragma endregion

