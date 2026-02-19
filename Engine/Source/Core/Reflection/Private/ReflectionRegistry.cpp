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

#pragma region Enum Management
EnumInfo& ReflectionRegistry::RegisterEnum(const string& name, const unordered_map<string, uint64>& entries)
{
	if (m_Enums.find(name) == m_Enums.end())
	{
		vector<pair<string, uint64>> sortedEntries(entries.begin(), entries.end());
		sort(sortedEntries.begin(), sortedEntries.end(),
			[](const auto& a, const auto& b)
			{
				return a.second < b.second;
			});
		m_Enums.emplace(name, EnumInfo{ name, entries, sortedEntries });

	}
	return m_Enums.at(name);
}

EnumInfo* ReflectionRegistry::GetEnum(const string& name)
{
	if (m_Enums.find(name) != m_Enums.end())
		return &m_Enums.at(name);
	return nullptr;
}
#pragma endregion

