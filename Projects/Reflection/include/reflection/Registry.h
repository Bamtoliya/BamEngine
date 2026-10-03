#pragma once

#include <reflection/EnumInfo.h>
#include <reflection/TypeInfo.h>
#include <reflection/RegistrationResult.h>
#include <reflection/ObjectView.h>
#include <reflection/ValueView.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <type_traits>
#include <typeindex>

namespace reflection
{
    class Registry
    {
    public:
        [[nodiscard]]
        bool Register(TypeInfo type);

        [[nodiscard]]
        const TypeInfo* FindType(std::string_view qualifiedName) const;

        [[nodiscard]]
        bool RegisterEnum(EnumInfo enumeration);

        [[nodiscard]]
        const EnumInfo* FindEnum(std::string_view qualifiedName) const;

        [[nodiscard]]
        bool RegisterFreeFunction(std::string qualifiedName, FunctionInfo function);

        [[nodiscard]]
        const FunctionInfo* FindFreeFunction(std::string_view qualifiedName, std::string_view signature) const;

        [[nodiscard]]
        const TypeInfo* FindType(std::type_index cppType) const;

        [[nodiscard]]
        const EnumInfo* FindEnum(std::type_index cppType) const;

        [[nodiscard]]
        const TypeInfo* FindType(const ObjectView& object) const;

        [[nodiscard]]
        const TypeInfo* FindType(const ValueView& value) const;

        [[nodiscard]]
        const EnumInfo* FindEnum(const ValueView& value) const;

        template<typename T>
            requires (std::is_class_v<T> || std::is_union_v<T>)
        [[nodiscard]]
        const TypeInfo* FindType() const
        {
            using Type = std::remove_cv_t<T>;
            return FindType(std::type_index(typeid(Type)));
        }

        template<typename T>
            requires std::is_enum_v<T>
        [[nodiscard]]
        const EnumInfo* FindEnum() const
        {
            using Type = std::remove_cv_t<T>;
            return FindEnum(std::type_index(typeid(Type)));
        }

        [[nodiscard]]
        RegistrationResult TryRegister(TypeInfo type);

        [[nodiscard]]
        RegistrationResult TryRegisterEnum(EnumInfo enumeration);

        [[nodiscard]]
        RegistrationResult TryRegisterFreeFunction(std::string qualifiedName, FunctionInfo function);

        [[nodiscard]]
        RegistrationResult TryMerge(Registry&& source);

    private:
        std::unordered_map<std::string, TypeInfo> m_types;
        std::unordered_map<std::string, EnumInfo> m_enums;
        using FunctionOverloads = std::unordered_map<std::string, FunctionInfo>;
        std::unordered_map<std::string, FunctionOverloads> m_freeFunctions;
    };
}