#pragma once

#include "Base.h"
#include "ReflectionTypes.h"

BEGIN(Engine)
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

#pragma region Type Management
public:
	TypeInfo& RegisterType(const string& name, size_t size);
	TypeInfo* GetType(const string& name);
#pragma endregion

#pragma region Variable
private:
	unordered_map<string, TypeInfo> m_Types;
#pragma endregion
};

END