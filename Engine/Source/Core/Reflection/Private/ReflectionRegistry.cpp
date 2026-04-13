#pragma once

#include "ReflectionRegistry.h"

#ifdef new
#undef new
#endif

IMPLEMENT_LAZY_SINGLETON(ReflectionRegistry)

void ReflectionRegistry::Free()
{
    for (auto& [typeId, buf] : m_CDOs)
    {
        auto it = m_TypesById.find(typeId);
        if (it != m_TypesById.end() && it->second->Destroy != nullptr)
        {
            it->second->Destroy(buf.data());
        }
    }

    m_CDOs.clear();

    m_TypesById.clear();
    m_TypesByQualifiedName.clear();
    m_TypesByShortName.clear();

    m_EnumsById.clear();
    m_EnumsByQualifiedName.clear();

    m_FunctionsById.clear();
    m_FunctionsByQualifiedName.clear();
}

#pragma region Type Management

void ReflectionRegistry::RegisterType(const TypeInfo& typeInfo)
{
    if (typeInfo.QualifiedName.empty())
        return;

    const string qualified(typeInfo.QualifiedName);

    if (m_TypesByQualifiedName.contains(qualified))
    {
        return;
    }

    m_TypesById[typeInfo.ID] = &typeInfo;
    m_TypesByQualifiedName[qualified] = &typeInfo;

    const string shortName(GetShortNameFromQualifiedName(typeInfo.QualifiedName));
    m_TypesByShortName[shortName].push_back(&typeInfo);
}

const TypeInfo* ReflectionRegistry::GetType(uint64 hash) const
{
    auto it = m_TypesById.find(hash);
    return it != m_TypesById.end() ? it->second : nullptr;
}

const TypeInfo* ReflectionRegistry::GetTypeByQualifiedName(string_view qualifiedName) const
{
    auto it = m_TypesByQualifiedName.find(string(qualifiedName));
    return it != m_TypesByQualifiedName.end() ? it->second : nullptr;
}

const vector<const TypeInfo*>& ReflectionRegistry::GetTypesByShortName(string_view shortName) const
{
    static const vector<const TypeInfo*> empty;

    auto it = m_TypesByShortName.find(string(shortName));
    return it != m_TypesByShortName.end() ? it->second : empty;
}

const TypeInfo* ReflectionRegistry::ResolveTypeName(string_view typeName) const
{
    if (typeName.empty())
        return nullptr;

    if (IsQualifiedName(typeName))
    {
        return GetTypeByQualifiedName(typeName);
    }

    const auto& matches = GetTypesByShortName(typeName);
    if (matches.empty())
        return nullptr;

    if (matches.size() == 1)
        return matches[0];

    return nullptr;
}

bool ReflectionRegistry::HasType(string_view qualifiedName) const
{
    return GetTypeByQualifiedName(qualifiedName) != nullptr;
}
#pragma endregion

#pragma region Enum Management

void ReflectionRegistry::RegisterEnum(const EnumInfo& enumInfo)
{
    if (enumInfo.QualifiedName.empty())
        return;

    const string qualified(enumInfo.QualifiedName);

    if (m_EnumsByQualifiedName.contains(qualified))
    {
        return;
    }

    m_EnumsById[enumInfo.ID] = &enumInfo;
    m_EnumsByQualifiedName[qualified] = &enumInfo;
}

const EnumInfo* ReflectionRegistry::GetEnum(uint64 hash) const
{
    auto it = m_EnumsById.find(hash);
    return it != m_EnumsById.end() ? it->second : nullptr;
}

const EnumInfo* ReflectionRegistry::GetEnumByQualifiedName(string_view qualifiedName) const
{
    auto it = m_EnumsByQualifiedName.find(string(qualifiedName));
    return it != m_EnumsByQualifiedName.end() ? it->second : nullptr;
}
#pragma endregion

#pragma region Function Management

void ReflectionRegistry::RegisterFunction(const FunctionInfo& functionInfo)
{
    if (functionInfo.OwnerQualifiedName.empty() || functionInfo.Signature.empty())
        return;

    const string qualifiedKey = MakeQualifiedMemberName(functionInfo.OwnerQualifiedName, functionInfo.Signature);

    if (m_FunctionsByQualifiedName.contains(qualifiedKey))
    {
        return;
    }

    m_FunctionsById[functionInfo.ID] = &functionInfo;
    m_FunctionsByQualifiedName[qualifiedKey] = &functionInfo;
}

const FunctionInfo* ReflectionRegistry::GetFunction(uint64 hash) const
{
    auto it = m_FunctionsById.find(hash);
    return it != m_FunctionsById.end() ? it->second : nullptr;
}

const FunctionInfo* ReflectionRegistry::GetFunctionByQualifiedName(string_view ownerQualifiedName, string_view signature) const
{
    const string key = MakeQualifiedMemberName(ownerQualifiedName, signature);
    auto it = m_FunctionsByQualifiedName.find(key);
    return it != m_FunctionsByQualifiedName.end() ? it->second : nullptr;
}
#pragma endregion

#pragma region CDO Management

void ReflectionRegistry::EnsureCDO(const TypeInfo& type)
{
    if (m_CDOs.contains(type.ID))
        return;
    if (type.Create == nullptr)
        return;

    auto& buf = m_CDOs[type.ID];
    buf.resize(type.Size);
    type.Create(buf.data());
}

vector<uint8>* ReflectionRegistry::GetCDO(uint64 hash)
{
    const TypeInfo* type = GetType(hash);
    if (!type)
        return nullptr;

    if (type->Create != nullptr)
    {
        EnsureCDO(*type);
        auto it = m_CDOs.find(hash);
        return it != m_CDOs.end() ? &it->second : nullptr;
    }

    for (const auto& [childId, childType] : m_TypesById)
    {
        if (childType->ParentQualifiedName == type->QualifiedName && childType->Create != nullptr)
        {
            EnsureCDO(*childType);
            auto cdoIt = m_CDOs.find(childId);
            return cdoIt != m_CDOs.end() ? &cdoIt->second : nullptr;
        }
    }

    return nullptr;
}

vector<uint8>* ReflectionRegistry::GetCDOByQualifiedName(string_view qualifiedName)
{
    const TypeInfo* type = GetTypeByQualifiedName(qualifiedName);
    if (!type)
        return nullptr;
    return GetCDO(type->ID);
}
#pragma endregion

#pragma region Constructor Management
void* ReflectionRegistry::CreateInstance(uint64 hash) const
{
    const TypeInfo* typeInfo = GetType(hash);
    if (typeInfo && typeInfo->Create)
    {
        void* instance = ::operator new(typeInfo->Size);
        typeInfo->Create(instance);
        return instance;
    }
    return nullptr;
}

void* ReflectionRegistry::CreateInstanceByQualifiedName(string_view qualifiedName) const
{
    const TypeInfo* typeInfo = GetTypeByQualifiedName(qualifiedName);
    if (!typeInfo)
        return nullptr;
    return CreateInstance(typeInfo->ID);
}

void ReflectionRegistry::DestroyInstance(void* instance) const
{
    if (!instance)
        return;

    Base* base = static_cast<Base*>(instance);
    const TypeInfo& typeInfo = base->GetTypeInfo();

    if (typeInfo.Destroy)
    {
        typeInfo.Destroy(instance);
    }

    ::operator delete(instance);
}
#pragma endregion