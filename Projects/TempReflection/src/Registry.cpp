#include <reflection/Registry.h>

#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace reflection
{
    namespace
    {
        RegistrationResult ValidateFunction(
            std::string_view owner, const FunctionInfo& function)
        {
            if (function.Name.empty() || function.ReturnType.empty() ||
                function.Signature.empty())
            {
                return { RegistrationError::InvalidFunction,
                    std::string(owner), function.Name };
            }

            for (std::size_t index = 0; index < function.Parameters.size(); ++index)
            {
                if (function.Parameters[index].TypeName.empty())
                {
                    return { RegistrationError::InvalidParameter, std::string(owner),
                        function.Name + " parameter #" + std::to_string(index) };
                }
            }

            return {};
        }
    }

    bool Registry::Register(TypeInfo type)
    {
        return TryRegister(std::move(type)).Succeeded();
    }

    RegistrationResult Registry::TryRegister(TypeInfo type)
    {
        if (type.QualifiedName.empty())
        {
            return { RegistrationError::InvalidTypeName, {}, {} };
        }

        if (m_types.contains(type.QualifiedName))
        {
            return { RegistrationError::DuplicateType, type.QualifiedName, {} };
        }

        std::unordered_set<std::string> propertyNames;
        for (const PropertyInfo& property : type.Properties)
        {
            if (property.Name.empty() || property.TypeName.empty())
            {
                return { RegistrationError::InvalidProperty,
                    type.QualifiedName, property.Name };
            }

            if (!propertyNames.insert(property.Name).second)
            {
                return { RegistrationError::DuplicateProperty,
                    type.QualifiedName, property.Name };
            }
        }

        std::unordered_map<std::string, std::unordered_set<std::string>> signatures;
        for (const FunctionInfo& function : type.Functions)
        {
            const auto validation = ValidateFunction(type.QualifiedName, function);
            if (!validation)
            {
                return validation;
            }

            if (!signatures[function.Name].insert(function.Signature).second)
            {
                return { RegistrationError::DuplicateFunction, type.QualifiedName,
                    function.Name + " " + function.Signature };
            }
        }

        std::string key = type.QualifiedName;
        const auto [position, inserted] =
            m_types.emplace(std::move(key), std::move(type));

        if (!inserted)
        {
            return { RegistrationError::DuplicateType, position->first, {} };
        }

        return {};
    }

    const TypeInfo* Registry::FindType(std::string_view qualifiedName) const
    {
        const auto found = m_types.find(std::string(qualifiedName));
        return found == m_types.end() ? nullptr : &found->second;
    }

    bool Registry::RegisterEnum(EnumInfo enumeration)
    {
        return TryRegisterEnum(std::move(enumeration)).Succeeded();
    }

    RegistrationResult Registry::TryRegisterEnum(EnumInfo enumeration)
    {
        if (enumeration.QualifiedName.empty() || enumeration.UnderlyingType.empty())
        {
            return { RegistrationError::InvalidEnum, enumeration.QualifiedName, {} };
        }

        if (m_enums.contains(enumeration.QualifiedName))
        {
            return { RegistrationError::DuplicateEnum, enumeration.QualifiedName, {} };
        }

        std::unordered_set<std::string> entryNames;
        for (const EnumEntry& entry : enumeration.Entries)
        {
            if (entry.Name.empty())
            {
                return { RegistrationError::InvalidEnumEntry,
                    enumeration.QualifiedName, {} };
            }

            if (!entryNames.insert(entry.Name).second)
            {
                return { RegistrationError::DuplicateEnumEntry,
                    enumeration.QualifiedName, entry.Name };
            }
        }

        std::string key = enumeration.QualifiedName;
        const auto [position, inserted] =
            m_enums.emplace(std::move(key), std::move(enumeration));

        if (!inserted)
        {
            return { RegistrationError::DuplicateEnum, position->first, {} };
        }

        return {};
    }

    const EnumInfo* Registry::FindEnum(std::string_view qualifiedName) const
    {
        const auto found = m_enums.find(std::string(qualifiedName));
        return found == m_enums.end() ? nullptr : &found->second;
    }

    bool Registry::RegisterFreeFunction(std::string qualifiedName, FunctionInfo function)
    {
        return TryRegisterFreeFunction(
            std::move(qualifiedName), std::move(function)).Succeeded();
    }

    RegistrationResult Registry::TryRegisterFreeFunction(
        std::string qualifiedName, FunctionInfo function)
    {
        if (qualifiedName.empty() || function.IsStaticMember || function.IsConstMember)
        {
            return { RegistrationError::InvalidFreeFunction, qualifiedName, function.Name };
        }

        const auto validation = ValidateFunction(qualifiedName, function);
        if (!validation)
        {
            return validation;
        }

        std::string signature = function.Signature;
        const auto [position, inserted] = m_freeFunctions[qualifiedName]
            .emplace(std::move(signature), std::move(function));

        if (!inserted)
        {
            return { RegistrationError::DuplicateFreeFunction,
                qualifiedName, position->first };
        }

        return {};
    }

    const FunctionInfo* Registry::FindFreeFunction(
        std::string_view qualifiedName, std::string_view signature) const
    {
        const auto group = m_freeFunctions.find(std::string(qualifiedName));
        if (group == m_freeFunctions.end())
        {
            return nullptr;
        }

        const auto found = group->second.find(std::string(signature));
        return found == group->second.end() ? nullptr : &found->second;
    }

    RegistrationResult Registry::TryMerge(Registry&& source)
    {
        if (this == &source)
        {
            return { RegistrationError::InvalidMergeSource, {}, {} };
        }

        // 먼저 모든 충돌을 검사합니다. 이 단계에서는 정보를 이동하지 않습니다.
        for (const auto& [name, type] : source.m_types)
        {
            if (m_types.contains(name))
            {
                return { RegistrationError::DuplicateType, name, {} };
            }
        }

        for (const auto& [name, enumeration] : source.m_enums)
        {
            if (m_enums.contains(name))
            {
                return { RegistrationError::DuplicateEnum, name, {} };
            }
        }

        for (const auto& [name, overloads] : source.m_freeFunctions)
        {
            const auto destination = m_freeFunctions.find(name);
            if (destination == m_freeFunctions.end())
            {
                continue;
            }

            for (const auto& [signature, function] : overloads)
            {
                if (destination->second.contains(signature))
                {
                    return {
                        RegistrationError::DuplicateFreeFunction, name, signature
                    };
                }
            }
        }

        // 노드를 이동하기 전에 필요한 버킷 공간을 준비합니다.
        m_types.reserve(m_types.size() + source.m_types.size());
        m_enums.reserve(m_enums.size() + source.m_enums.size());
        m_freeFunctions.reserve(m_freeFunctions.size() + source.m_freeFunctions.size());

        for (const auto& [name, overloads] : source.m_freeFunctions)
        {
            const auto destination = m_freeFunctions.find(name);
            if (destination != m_freeFunctions.end())
            {
                destination->second.reserve(destination->second.size() + overloads.size());
            }
        }

        m_types.merge(source.m_types);
        m_enums.merge(source.m_enums);

        // 대상에 없는 함수 이름은 오버로드 그룹 전체를 이동합니다.
        m_freeFunctions.merge(source.m_freeFunctions);

        // 대상에 이미 있는 함수 이름은 각 오버로드 노드를 이동합니다.
        for (auto& [name, overloads] : source.m_freeFunctions)
        {
            m_freeFunctions.at(name).merge(overloads);
        }

        source.m_freeFunctions.clear();
        return {};
    }
}