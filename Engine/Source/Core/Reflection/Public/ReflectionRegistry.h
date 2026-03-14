#pragma once

#include "Base.h"
#include "ReflectionTypes.h"

BEGIN(Engine)
void InitReflectionSystem();
class ENGINE_API ReflectionRegistry : public Base
{
	DECLARE_SINGLETON(ReflectionRegistry)

#pragma region Constructor&Destructor
private:
	ReflectionRegistry() {}
	virtual ~ReflectionRegistry() = default;
	EResult Initialize(void* arg = nullptr) { return EResult::Success; }
public:
	virtual void Free();
#pragma endregion

#pragma region Type Management
public:
	void RegisterType(uint64 hash, const TypeInfo& typeInfo);
	const TypeInfo* GetType(uint64 hash) const;
	const TypeInfo* GetType(const string& name) const;
#pragma endregion

#pragma region Enum Management
public:
	void RegisterEnum(uint64 hash, const EnumInfo& enumInfo);
	const EnumInfo* GetEnum(uint64 hash) const;
	const EnumInfo* GetEnum(const string& name) const;
#pragma endregion

#pragma region Function Management
public:
	void RegisterFunction(uint64 hash, const FunctionInfo& functionInfo);
	const FunctionInfo* GetFunction(uint64 hash) const;
	const FunctionInfo* GetFunction(const string& name) const;
#pragma endregion

#pragma region CDO Management
public:
	vector<uint8>* GetCDO(uint64 hash);
	vector<uint8>* GetCDO(const string& name);
public:
	bool ResetPropertyToDefault(void* instance, const TypeInfo& type, const PropertyInfo& prop);
	bool ResetObjectToDefault(void* instance, const TypeInfo& type);
	bool IsPropertyDefault(const void* instance, const TypeInfo& type, const PropertyInfo& prop);
private:
	void EnsureCDO(const TypeInfo& type);
#pragma endregion

#pragma region Constructor Management
public:
	template<typename T>
	void RegisterConstructor(uint64 hash)
	{
		m_Constructors[hash] = []() -> void* { return new T(); };
	}
	void* CreateInstance(uint64 hash) const;
	void* CreateInstance(const string& name) const;


#pragma endregion



#pragma region Variable
private:
	unordered_map<uint64, const TypeInfo*> m_Types;
	unordered_map<uint64, const EnumInfo*> m_Enums;
	unordered_map<uint64, const FunctionInfo*> m_Functions;
	unordered_map<uint64, function<void*()>> m_Constructors;
	unordered_map<uint64, vector<uint8>> m_CDOs;
#pragma endregion
};

END