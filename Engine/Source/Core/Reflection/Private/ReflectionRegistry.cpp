#pragma once

#include "ReflectionRegistry.h"

IMPLEMENT_LAZY_SINGLETON(ReflectionRegistry)

void ReflectionRegistry::Free()
{
	for (auto& [typeId, buf] : m_CDOs)
	{
		auto it = m_Types.find(typeId);
		if (it != m_Types.end() && it->second->Destroy != nullptr)
			it->second->Destroy(buf.data());
	}

	m_CDOs.clear();
	m_Types.clear();
	m_Enums.clear();
	m_Functions.clear();
}

#pragma region Type Management
void ReflectionRegistry::RegisterType(uint64 hash, const TypeInfo& typeInfo)
{
	m_Types[hash] = &typeInfo;
}

const TypeInfo* ReflectionRegistry::GetType(uint64 hash) const
{
	auto it = m_Types.find(hash);
	return (it != m_Types.end()) ? it->second : nullptr;
}

const TypeInfo* ReflectionRegistry::GetType(const string& name) const
{
	return GetType(RunTimeHash(name));
}
#pragma endregion

#pragma region Enum Management
void ReflectionRegistry::RegisterEnum(uint64 hash, const EnumInfo& enumInfo)
{
	m_Enums[hash] = &enumInfo;
}

const EnumInfo* ReflectionRegistry::GetEnum(uint64 hash) const
{
	auto it = m_Enums.find(hash);
	return (it != m_Enums.end()) ? it->second : nullptr;
}

const EnumInfo* ReflectionRegistry::GetEnum(const string& name) const
{
	return GetEnum(RunTimeHash(name));
}
#pragma endregion

#pragma region Function Management
void ReflectionRegistry::RegisterFunction(uint64 hash, const FunctionInfo& functionInfo)
{
	m_Functions[hash] = &functionInfo;
}
const FunctionInfo* ReflectionRegistry::GetFunction(uint64 hash) const
{
	auto it = m_Functions.find(hash);
	return (it != m_Functions.end()) ? it->second : nullptr;
}
const FunctionInfo* ReflectionRegistry::GetFunction(const string& name) const
{
	return GetFunction(RunTimeHash(name));
}
#pragma endregion

#pragma region CDO Management
void ReflectionRegistry::EnsureCDO(const TypeInfo& type)
{
	if (m_CDOs.contains(type.ID)) return;
	if (type.Create == nullptr) return;       

	auto& buf = m_CDOs[type.ID];
	buf.resize(type.Size);
	type.Create(buf.data());                   
}

vector<uint8>* ReflectionRegistry::GetCDO(uint64 hash)
{
	auto typeIt = m_Types.find(hash);
	if (typeIt == m_Types.end()) return nullptr;

	const TypeInfo& type = *typeIt->second;

	if (type.Create != nullptr)
	{
		EnsureCDO(type);
		auto cdoIt = m_CDOs.find(hash);
		return (cdoIt != m_CDOs.end()) ? &cdoIt->second : nullptr;
	}

	for (const auto& [childId, childType] : m_Types)
	{
		if (childType->ParentName == type.Name && childType->Create != nullptr)
		{
			EnsureCDO(*childType);
			auto cdoIt = m_CDOs.find(childId);
			return (cdoIt != m_CDOs.end()) ? &cdoIt->second : nullptr;
		}
	}

	return nullptr;
}

vector<uint8>* ReflectionRegistry::GetCDO(const string& name)
{
	return GetCDO(RunTimeHash(name));
}

bool ReflectionRegistry::ResetPropertyToDefault(void* instance, const TypeInfo& type, const PropertyInfo& prop)
{
	const vector<uint8>* cdo = GetCDO(type.ID);
	if (!cdo) return false;

	const void* defaultVal = cdo->data() + prop.Offset;
	void* currentVal = static_cast<uint8*>(instance) + prop.Offset;

	if (prop.CopyProp)
		prop.CopyProp(currentVal, defaultVal);
	else
		memcpy(currentVal, defaultVal, prop.Size);

	return true;
}
bool ReflectionRegistry::ResetObjectToDefault(void* instance, const TypeInfo& type)
{
	const vector<uint8>* cdo = GetCDO(type.ID);
	if (!cdo) return false;

	if (type.Copy)
	{
		// copy assignment: string, vector 등 non-trivial 타입 포함 안전하게 복원
		type.Copy(instance, cdo->data());
	}
	else
	{
		// Copy 없으면 Destroy + Create 로 전체 재초기화
		if (type.Destroy) type.Destroy(instance);
		if (type.Create)  type.Create(instance);
	}
	return true;
}
bool ReflectionRegistry::IsPropertyDefault(const void* instance, const TypeInfo& type, const PropertyInfo& prop)
{
	const vector<uint8>* cdo = GetCDO(type.ID);
	if (!cdo) return false;

	const void* defaultVal = cdo->data() + prop.Offset;
	const void* currentVal = static_cast<const uint8*>(instance) + prop.Offset;

	if (prop.EqualProp)
		return prop.EqualProp(currentVal, defaultVal);

	return memcmp(currentVal, defaultVal, prop.Size) == 0;
}
#pragma endregion

#pragma region Constructor Management
void* ReflectionRegistry::CreateInstance(uint64 hash) const 
{
	auto it = m_Constructors.find(hash);
	if (it != m_Constructors.end())
	{
		return it->second();
	}

	TODO("CreateInstance 구현해야함");
	return nullptr;
}
void* ReflectionRegistry::CreateInstance(const string& name) const
{
	return CreateInstance(RunTimeHash(name));
}
#pragma endregion
