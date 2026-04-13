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
    void RegisterType(const TypeInfo& typeInfo);

    const TypeInfo* GetType(uint64 hash) const;
    const TypeInfo* GetTypeByQualifiedName(string_view qualifiedName) const;
    const vector<const TypeInfo*>& GetTypesByShortName(string_view shortName) const;

    const TypeInfo* ResolveTypeName(string_view typeName) const;
    bool HasType(string_view qualifiedName) const;
#pragma endregion

#pragma region Enum Management
public:
    void RegisterEnum(const EnumInfo& enumInfo);

    const EnumInfo* GetEnum(uint64 hash) const;
    const EnumInfo* GetEnumByQualifiedName(string_view qualifiedName) const;
#pragma endregion

#pragma region Function Management
public:
    void RegisterFunction(const FunctionInfo& functionInfo);

    const FunctionInfo* GetFunction(uint64 hash) const;
    const FunctionInfo* GetFunctionByQualifiedName(string_view ownerQualifiedName, string_view signature) const;
#pragma endregion

#pragma region CDO Management
private:
    void EnsureCDO(const TypeInfo& type);

public:
    vector<uint8>* GetCDO(uint64 hash);
    vector<uint8>* GetCDOByQualifiedName(string_view qualifiedName);
#pragma endregion

#pragma region Constructor Management
public:
    void* CreateInstance(uint64 hash) const;
    void* CreateInstanceByQualifiedName(string_view qualifiedName) const;
    void DestroyInstance(void* instance) const;
#pragma endregion

#pragma region Variables
private:
    unordered_map<uint64, const TypeInfo*> m_TypesById;
    unordered_map<string, const TypeInfo*> m_TypesByQualifiedName;
    unordered_map<string, vector<const TypeInfo*>> m_TypesByShortName;

    unordered_map<uint64, const EnumInfo*> m_EnumsById;
    unordered_map<string, const EnumInfo*> m_EnumsByQualifiedName;

    unordered_map<uint64, const FunctionInfo*> m_FunctionsById;
    unordered_map<string, const FunctionInfo*> m_FunctionsByQualifiedName;

    unordered_map<uint64, vector<uint8>> m_CDOs;
#pragma endregion
};

END