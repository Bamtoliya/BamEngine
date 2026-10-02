#pragma once

#include <reflection/Metadata.h>
#include <reflection/FunctionInfo.h>
#include <reflection/PropertyAccess.h>

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace reflection
{
    namespace detail
    {
        template<typename T>
        struct MemberTraits;

        template<typename Owner, typename Value>
        struct MemberTraits<Value Owner::*>
        {
            using OwnerType = Owner;
            using ValueType = Value;
        };
    }

    struct PropertyInfo;

    template<auto Member>
    PropertyInfo MakeProperty(
        std::string name,
        std::string typeName
    );

    struct PropertyInfo
    {
        std::string Name;
        std::string TypeName;
        reflection::Metadata Metadata;

        [[nodiscard]]
        bool CanRead() const noexcept
        {
            return m_read != nullptr;
        }

        [[nodiscard]]
        bool CanWrite() const noexcept
        {
            return m_write != nullptr;
        }

        [[nodiscard]]
        PropertyAccessStatus GetReadStatus() const noexcept
        {
            return CanRead() ? PropertyAccessStatus::Available : m_readStatus;
        }

        [[nodiscard]]
        PropertyAccessStatus GetWriteStatus() const noexcept
        {
            return CanWrite() ? PropertyAccessStatus::Available : m_writeStatus;
        }

        [[nodiscard]]
        static PropertyInfo DescribeOnly(PropertyAccessStatus status)
        {
            PropertyInfo property;

            // 콜백이 없는 객체에 Available을 지정할 수는 없습니다.
            const auto reason = status == PropertyAccessStatus::Available
                ? PropertyAccessStatus::NotBound : status;

            property.m_readStatus = reason;
            property.m_writeStatus = reason;
            return property;
        }

        template<typename Value, typename Owner>
            requires (
        std::is_object_v<Value> &&
            !std::is_volatile_v<Value> &&
            !std::is_volatile_v<Owner>
            )
            [[nodiscard]]
        const Value* Read(const Owner& object) const
        {
            if (m_read == nullptr ||
                m_ownerType != std::type_index(typeid(Owner)) ||
                m_valueType != std::type_index(typeid(Value)))
            {
                return nullptr;
            }

            return static_cast<const Value*>(
                m_read(std::addressof(object))
                );
        }

        template<typename Value, typename Owner>
            requires (
        std::is_object_v<Value> &&
            !std::is_const_v<Value> &&
            !std::is_volatile_v<Value> &&
            !std::is_const_v<Owner> &&
            !std::is_volatile_v<Owner>
            )
            [[nodiscard]]
        bool Write(Owner& object, const Value& value) const
        {
            if (m_write == nullptr ||
                m_ownerType != std::type_index(typeid(Owner)) ||
                m_valueType != std::type_index(typeid(Value)))
            {
                return false;
            }

            m_write(
                std::addressof(object),
                std::addressof(value)
            );

            return true;
        }

    private:
        std::type_index m_ownerType{ typeid(void) };
        std::type_index m_valueType{ typeid(void) };

        const void* (*m_read)(const void*) = nullptr;
        void (*m_write)(void*, const void*) = nullptr;

        template<auto Member>
        friend PropertyInfo MakeProperty(
            std::string name,
            std::string typeName
        );

        PropertyAccessStatus m_readStatus = PropertyAccessStatus::NotBound;
        PropertyAccessStatus m_writeStatus = PropertyAccessStatus::NotBound;
    };

    template<auto Member>
    PropertyInfo MakeProperty(
        std::string name,
        std::string typeName
    )
    {
        static_assert(
            std::is_member_object_pointer_v<decltype(Member)>,
            "MakeProperty requires a data member pointer"
            );

        using Traits = detail::MemberTraits<decltype(Member)>;
        using Owner = typename Traits::OwnerType;
        using MemberValue = typename Traits::ValueType;
        using Value = std::remove_cv_t<MemberValue>;

        static_assert(
            !std::is_volatile_v<MemberValue>,
            "Volatile properties are not supported"
            );

        PropertyInfo property;
        property.Name = std::move(name);
        property.TypeName = std::move(typeName);
        property.m_ownerType = std::type_index(typeid(Owner));
        property.m_valueType = std::type_index(typeid(Value));

        property.m_read = +[](const void* object) -> const void*
            {
                const auto* owner = static_cast<const Owner*>(object);

                return std::addressof(owner->*Member);
            };

        if constexpr (std::is_const_v<MemberValue>)
        {
            property.m_writeStatus = PropertyAccessStatus::ConstQualified;
        }
        else if constexpr (std::is_assignable_v<MemberValue&, const Value&>)
        {
            property.m_write = +[](void* object, const void* value)
                {
                    auto* owner = static_cast<Owner*>(object);
                    owner->*Member = *static_cast<const Value*>(value);
                };
        }
        else
        {
            property.m_writeStatus = PropertyAccessStatus::NotAssignable;
        }

        return property;
    }

    struct TypeInfo
    {
        std::string QualifiedName;
        std::vector<PropertyInfo> Properties;
        std::vector<FunctionInfo> Functions;
        reflection::Metadata Metadata;

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
        const FunctionInfo* FindFunction(
            std::string_view name,
            std::string_view signature
        ) const
        {
            for (const FunctionInfo& function : Functions)
            {
                if (function.Name == name &&
                    function.Signature == signature)
                {
                    return &function;
                }
            }

            return nullptr;
        }
    };
}