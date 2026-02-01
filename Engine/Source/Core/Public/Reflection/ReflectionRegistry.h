#pragma once

#include "Base.h"
#include "ReflectionTypes.h"

BEGIN(Engine)
void InitEnumReflection();
class ENGINE_API ReflectionRegistry : public Base
{
	DECLARE_SINGLETON(ReflectionRegistry)

#pragma region Constructor&Destructor
private:
	ReflectionRegistry() {}
	virtual ~ReflectionRegistry() = default;
	EResult Initialize(void* arg = nullptr) { return EResult::Success; }
public:
	virtual void Free() {}
#pragma endregion

#pragma endregion


#pragma region Type Management
public:
	TypeInfo& RegisterType(const string& name, size_t size);
	TypeInfo* GetType(const string& name);
#pragma endregion

#pragma region Enum Management
public:
	EnumInfo& RegisterEnum(const string& name, const unordered_map<string, uint64>& entries);
	EnumInfo* GetEnum(const string& name);
#pragma endregion


#pragma region Variable
private:
	unordered_map<string, TypeInfo> m_Types;
	unordered_map<string, EnumInfo> m_Enums;
#pragma endregion
};

END