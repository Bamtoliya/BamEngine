#pragma once

#include <reflection/Metadata.h>
#include <reflection/FunctionInfo.h>
#include <reflection/PropertyInfo.h>
#include <reflection/ObjectView.h>
#include <reflection/ValueView.h>

#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace reflection
{
    struct TypeInfo
    {
        std::string QualifiedName;
        std::vector<PropertyInfo> Properties;
        std::vector<FunctionInfo> Functions;
        reflection::Metadata Metadata;

        template<typename T>
            requires (std::is_class_v<T> || std::is_union_v<T>)
        [[nodiscard]]
        static TypeInfo For(std::string qualifiedName)
        {
            using Type = std::remove_cv_t<T>;

            TypeInfo info;
            info.QualifiedName = std::move(qualifiedName);
            info.m_cppType = std::type_index(typeid(Type));
            return info;
        }

        [[nodiscard]]
        std::type_index GetCppType() const noexcept
        {
            return m_cppType;
        }

        [[nodiscard]]
        ObjectView AsObject(const ValueView& value) const
        {
            if (!value.IsValid() ||
                m_cppType == std::type_index(typeid(void)) ||
                m_cppType != value.GetCppType())
            {
                return {};
            }

            ObjectView object;
            object.m_type = m_cppType;
            object.m_readAddress = value.m_address;
            return object;
        }

        [[nodiscard]]
        const PropertyInfo* FindProperty(std::string_view name) const
        {
            for (const PropertyInfo& property : Properties)
            {
                if (property.Name == name)
                {
                    return &property;
                }
            }

            return nullptr;
        }

        [[nodiscard]]
        const FunctionInfo* FindFunction(std::string_view name, std::string_view signature) const
        {
            for (const FunctionInfo& function : Functions)
            {
                if (function.Name == name && function.Signature == signature)
                {
                    return &function;
                }
            }

            return nullptr;
        }

    private:
        std::type_index m_cppType{ typeid(void) };
    };
}