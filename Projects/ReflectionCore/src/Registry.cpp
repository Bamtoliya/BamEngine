#include "reflection/runtime/Registry.h"

#include <new>

namespace reflection
{
    Registry& Registry::Get()
    {
        static Registry instance;
        return instance;
    }

    void Registry::RegisterType(const TypeInfo& type_info)
    {
        m_types_by_id[type_info.ID] = &type_info;
        m_types_by_qualified_name[std::string(type_info.QualifiedName)] = &type_info;
        m_types_by_short_name[std::string(GetShortNameFromQualifiedName(type_info.QualifiedName))].push_back(&type_info);
    }

    void Registry::RegisterEnum(const EnumInfo& enum_info)
    {
        m_enums_by_id[enum_info.ID] = &enum_info;
        m_enums_by_qualified_name[std::string(enum_info.QualifiedName)] = &enum_info;
    }

    void Registry::RegisterFunction(const FunctionInfo& function_info)
    {
        m_functions_by_id[function_info.ID] = &function_info;
        m_functions_by_qualified_name[MakeQualifiedMemberName(function_info.OwnerQualifiedName, function_info.Signature)] = &function_info;
    }

    const TypeInfo* Registry::GetType(std::uint64_t id) const
    {
        const auto it = m_types_by_id.find(id);
        return it != m_types_by_id.end() ? it->second : nullptr;
    }

    const TypeInfo* Registry::GetTypeByQualifiedName(std::string_view qualified_name) const
    {
        const auto it = m_types_by_qualified_name.find(std::string(qualified_name));
        return it != m_types_by_qualified_name.end() ? it->second : nullptr;
    }

    const std::vector<const TypeInfo*>& Registry::GetTypesByShortName(std::string_view short_name) const
    {
        static const std::vector<const TypeInfo*> empty;
        const auto it = m_types_by_short_name.find(std::string(short_name));
        return it != m_types_by_short_name.end() ? it->second : empty;
    }

    const TypeInfo* Registry::ResolveTypeName(std::string_view type_name) const
    {
        if (const TypeInfo* direct = GetTypeByQualifiedName(type_name))
        {
            return direct;
        }

        const auto& matches = GetTypesByShortName(type_name);
        return matches.empty() ? nullptr : matches.front();
    }

    bool Registry::HasType(std::string_view qualified_name) const
    {
        return GetTypeByQualifiedName(qualified_name) != nullptr;
    }

    const EnumInfo* Registry::GetEnum(std::uint64_t id) const
    {
        const auto it = m_enums_by_id.find(id);
        return it != m_enums_by_id.end() ? it->second : nullptr;
    }

    const EnumInfo* Registry::GetEnumByQualifiedName(std::string_view qualified_name) const
    {
        const auto it = m_enums_by_qualified_name.find(std::string(qualified_name));
        return it != m_enums_by_qualified_name.end() ? it->second : nullptr;
    }

    const FunctionInfo* Registry::GetFunction(std::uint64_t id) const
    {
        const auto it = m_functions_by_id.find(id);
        return it != m_functions_by_id.end() ? it->second : nullptr;
    }

    const FunctionInfo* Registry::GetFunctionByQualifiedName(std::string_view owner_qualified_name, std::string_view signature) const
    {
        const auto key = MakeQualifiedMemberName(owner_qualified_name, signature);
        const auto it = m_functions_by_qualified_name.find(key);
        return it != m_functions_by_qualified_name.end() ? it->second : nullptr;
    }

    void* Registry::CreateInstanceByQualifiedName(std::string_view qualified_name) const
    {
        const TypeInfo* type_info = GetTypeByQualifiedName(qualified_name);
        if (!type_info || !type_info->Create)
        {
            return nullptr;
        }

        void* memory = ::operator new(type_info->Size);
        type_info->Create(memory);

        if (type_info->PostLoad)
        {
            type_info->PostLoad(memory);
        }

        return memory;
    }

    void Registry::DestroyInstanceByQualifiedName(std::string_view qualified_name, void* instance) const
    {
        if (!instance)
        {
            return;
        }

        const TypeInfo* type_info = GetTypeByQualifiedName(qualified_name);
        if (type_info && type_info->Destroy)
        {
            type_info->Destroy(instance);
        }

        ::operator delete(instance);
    }

    std::vector<std::uint64_t> Registry::GetDerivedTypeIDs(std::uint64_t baseTypeID) const
    {
        std::vector<std::uint64_t> result;
        const TypeInfo* baseType = GetType(baseTypeID);
        if (!baseType) return result;
        for (const auto& pair : m_types_by_id)
        {
            const TypeInfo* candidate = pair.second;
            if (candidate->ID == baseTypeID) continue; // 자기 자신은 스킵
            // 상속 체인을 올라가며 baseType과 매칭되는지 확인
            const TypeInfo* current = candidate;
            while (current && !current->ParentQualifiedName.empty())
            {
                const TypeInfo* parent = GetTypeByQualifiedName(current->ParentQualifiedName);
                if (!parent) break;
                if (parent->ID == baseTypeID)
                {
                    result.push_back(candidate->ID);
                    break;
                }
                current = parent;
            }
        }
        return result;
    }
}