#pragma once

#include "ReflectionRegistry.h"

IMPLEMENT_SINGLETON(ReflectionRegistry)

#pragma region Type Management
TypeInfo& ReflectionRegistry::RegisterType(const string& name, size_t size)
{
	if (m_Types.find(name) == m_Types.end())
	{
		m_Types.emplace(name, TypeInfo(name, size));
	}
	return m_Types.at(name);
}

TypeInfo* ReflectionRegistry::GetType(const string& name)
{
	if (m_Types.find(name) != m_Types.end())
		return &m_Types.at(name);
	return nullptr;
}
#pragma endregion
