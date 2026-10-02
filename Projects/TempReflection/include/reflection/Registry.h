#pragma once

#include <reflection/EnumInfo.h>
#include <reflection/TypeInfo.h>
#include <reflection/RegistrationResult.h>

#include <string>
#include <string_view>
#include <unordered_map>

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