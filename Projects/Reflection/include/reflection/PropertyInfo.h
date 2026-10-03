#pragma once

#include <reflection/Metadata.h>
#include <reflection/PropertyAccess.h>
#include <reflection/PropertyAccessResult.h>
#include <reflection/ObjectView.h>
#include <reflection/ValueView.h>
#include <reflection/detail/PropertyContainerAccess.h>
#include <reflection/detail/EnumValueConversion.h>
#include <reflection/detail/PropertyContainerBinding.h>
#include <reflection/EnumValue.h>

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>
#include <cstddef>

namespace reflection
{
    enum class PropertyValueKind
    {
        Unknown,
        Boolean,
        SignedInteger,
        UnsignedInteger,
        FloatingPoint,
        String,
        WideString,
        Enum,
        Pointer,
        Array,
        Class,
        Union,
        Vector
    };

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

        template<typename T>
        constexpr PropertyValueKind GetPropertyValueKind()
        {
            using Value = std::remove_cv_t<T>;

            if constexpr (std::is_same_v<Value, bool>)
            {
                return PropertyValueKind::Boolean;
            }
            else if constexpr (std::is_integral_v<Value>)
            {
                return std::is_signed_v<Value>
                    ? PropertyValueKind::SignedInteger
                    : PropertyValueKind::UnsignedInteger;
            }
            else if constexpr (std::is_floating_point_v<Value>)
            {
                return PropertyValueKind::FloatingPoint;
            }
            else if constexpr (std::is_same_v<Value, std::string>)
            {
                return PropertyValueKind::String;
            }
            else if constexpr (std::is_same_v<Value, std::wstring>)
            {
                return PropertyValueKind::WideString;
            }
            else if constexpr (std::is_enum_v<Value>)
            {
                return PropertyValueKind::Enum;
            }
            else if constexpr (std::is_pointer_v<Value>)
            {
                return PropertyValueKind::Pointer;
            }
            else if constexpr (std::is_array_v<Value>)
            {
                return PropertyValueKind::Array;
            }
            else if constexpr (VectorTraits<Value>::IsVector)
            {
                return PropertyValueKind::Vector;
            }
            else if constexpr (std::is_class_v<Value>)
            {
                return PropertyValueKind::Class;
            }
            else if constexpr (std::is_union_v<Value>)
            {
                return PropertyValueKind::Union;
            }
            else
            {
                return PropertyValueKind::Unknown;
            }
        }
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
        PropertyValueKind GetValueKind() const noexcept
        {
            return m_valueKind;
        }

        template<typename T>
            requires std::is_object_v<T>
        [[nodiscard]]
        bool IsValueType() const noexcept
        {
            using Value = std::remove_cv_t<T>;
            return m_valueType == std::type_index(typeid(Value));
        }

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

        template<typename Value>
            requires (std::is_object_v<Value> && !std::is_volatile_v<Value>)
        [[nodiscard]]
        const Value* ReadFrom(const ObjectView& object) const
        {
            if (!object.IsValid() || m_read == nullptr ||
                m_ownerType != object.m_type ||
                m_valueType != std::type_index(typeid(Value)))
            {
                return nullptr;
            }

            return static_cast<const Value*>(m_read(object.m_readAddress));
        }

        template<typename Value>
            requires (std::is_object_v<Value> &&
        !std::is_const_v<Value> && !std::is_volatile_v<Value>)
            [[nodiscard]]
        bool WriteTo(const ObjectView& object, const Value& value) const
        {
            if (!object.IsValid() || object.m_writeAddress == nullptr ||
                m_write == nullptr || m_ownerType != object.m_type ||
                m_valueType != std::type_index(typeid(Value)))
            {
                return false;
            }

            m_write(object.m_writeAddress, std::addressof(value));
            return true;
        }

        [[nodiscard]]
        PropertyReadResult TryReadValue(const ObjectView& object) const
        {
            using Error = PropertyAccessError;

            if (!object.IsValid())
            {
                return { Error::InvalidObject, {} };
            }

            if (m_read == nullptr)
            {
                return { Error::ReadUnavailable, {} };
            }

            if (m_ownerType != object.m_type)
            {
                return { Error::OwnerTypeMismatch, {} };
            }

            ValueView value;
            value.m_type = m_valueType;
            value.m_address = m_read(object.m_readAddress);
            return { Error::None, value };
        }

        [[nodiscard]]
        PropertyAccessError TryWriteValue(const ObjectView& object, const ValueView& value) const
        {
            using Error = PropertyAccessError;

            if (!object.IsValid())
            {
                return Error::InvalidObject;
            }

            if (!value.IsValid())
            {
                return Error::InvalidValue;
            }

            if (m_write == nullptr)
            {
                return Error::WriteUnavailable;
            }

            if (m_ownerType != object.m_type)
            {
                return Error::OwnerTypeMismatch;
            }

            if (object.m_writeAddress == nullptr)
            {
                return Error::ReadOnlyObject;
            }

            if (m_valueType != value.m_type)
            {
                return Error::ValueTypeMismatch;
            }

            m_write(object.m_writeAddress, value.m_address);
            return Error::None;
        }

        [[nodiscard]]
        bool CanWriteEnumValue() const noexcept
        {
            return m_writeEnumValue != nullptr;
        }

        [[nodiscard]]
        PropertyAccessError TryWriteEnumValue(const ObjectView& object, const EnumValue& value) const
        {
            using Error = PropertyAccessError;

            if (!object.IsValid())
            {
                return Error::InvalidObject;
            }

            if (m_ownerType != object.m_type)
            {
                return Error::OwnerTypeMismatch;
            }

            if (object.m_writeAddress == nullptr)
            {
                return Error::ReadOnlyObject;
            }

            if (m_write == nullptr)
            {
                return Error::WriteUnavailable;
            }

            if (m_writeEnumValue == nullptr)
            {
                return Error::EnumWriteUnavailable;
            }

            return m_writeEnumValue(*this, object, value);
        }

        [[nodiscard]]
        ValueView ReadValue(const ObjectView& object) const
        {
            return TryReadValue(object).Value;
        }

        [[nodiscard]]
        bool WriteValue(const ObjectView& object, const ValueView& value) const
        {
            return TryWriteValue(object, value) == PropertyAccessError::None;
        }

        [[nodiscard]]
        bool CanEditObject() const noexcept
        {
            return m_editObject != nullptr;
        }

        [[nodiscard]]
        PropertyObjectResult TryEditObject(const ObjectView& object) const
        {
            using Error = PropertyAccessError;

            if (!object.IsValid())
            {
                return { Error::InvalidObject, {} };
            }

            if (m_read == nullptr)
            {
                return { Error::ReadUnavailable, {} };
            }

            if (m_ownerType != object.m_type)
            {
                return { Error::OwnerTypeMismatch, {} };
            }

            if (!m_objectValue)
            {
                return { Error::ObjectUnavailable, {} };
            }

            if (object.m_writeAddress == nullptr)
            {
                return { Error::ReadOnlyObject, {} };
            }

            if (m_editObject == nullptr)
            {
                return { Error::ReadOnlyProperty, {} };
            }

            return { Error::None, m_editObject(object.m_writeAddress) };
        }

        [[nodiscard]]
        ObjectView EditObject(const ObjectView& object) const
        {
            return TryEditObject(object).Object;
        }

        [[nodiscard]]
        bool IsContainer() const noexcept
        {
            return m_container.Size != nullptr;
        }

        [[nodiscard]]
        bool CanReadElement() const noexcept
        {
            return m_container.ReadElement != nullptr;
        }

        [[nodiscard]]
        bool CanWriteElement() const noexcept
        {
            return m_container.WriteElement != nullptr;
        }

        [[nodiscard]]
        bool CanEditElementObject() const noexcept
        {
            return m_container.EditElementObject != nullptr;
        }

        [[nodiscard]]
        bool CanResize() const noexcept
        {
            return m_container.Resize != nullptr;
        }

        [[nodiscard]]
        PropertyAccessError TryResize(const ObjectView& object, std::size_t size) const
        {
            using Error = PropertyAccessError;

            const auto container = TryReadValue(object);
            if (!container)
            {
                return container.Error;
            }

            if (!IsContainer())
            {
                return Error::NotContainer;
            }

            if (object.m_writeAddress == nullptr)
            {
                return Error::ReadOnlyObject;
            }

            if (m_container.ReadOnly)
            {
                return Error::ReadOnlyProperty;
            }

            if (m_container.Resize == nullptr)
            {
                return Error::ResizeUnavailable;
            }

            return m_container.Resize(object.m_writeAddress, size);
        }

        [[nodiscard]]
        bool CanClear() const noexcept
        {
            return m_container.Clear != nullptr;
        }

        [[nodiscard]]
        PropertyAccessError TryClear(const ObjectView& object) const
        {
            using Error = PropertyAccessError;

            const auto container = TryReadValue(object);
            if (!container)
            {
                return container.Error;
            }

            if (!IsContainer())
            {
                return Error::NotContainer;
            }

            if (object.m_writeAddress == nullptr)
            {
                return Error::ReadOnlyObject;
            }

            if (m_container.ReadOnly)
            {
                return Error::ReadOnlyProperty;
            }

            if (m_container.Clear == nullptr)
            {
                return Error::ClearUnavailable;
            }

            m_container.Clear(object.m_writeAddress);
            return Error::None;
        }

        [[nodiscard]]
        bool CanAppend() const noexcept
        {
            return m_container.Append != nullptr;
        }

        [[nodiscard]]
        PropertyAccessError TryAppend(const ObjectView& object, const ValueView& value) const
        {
            using Error = PropertyAccessError;

            const auto container = TryReadValue(object);
            if (!container)
            {
                return container.Error;
            }

            if (!value.IsValid())
            {
                return Error::InvalidValue;
            }

            if (!IsContainer())
            {
                return Error::NotContainer;
            }

            if (object.m_writeAddress == nullptr)
            {
                return Error::ReadOnlyObject;
            }

            if (m_container.ReadOnly)
            {
                return Error::ReadOnlyProperty;
            }

            if (m_container.Append == nullptr)
            {
                return Error::AppendUnavailable;
            }

            if (m_container.ElementType != value.GetCppType())
            {
                return Error::ValueTypeMismatch;
            }

            return m_container.Append(object.m_writeAddress, value.m_address);
        }

        [[nodiscard]]
        bool CanInsert() const noexcept
        {
            return m_container.Insert != nullptr;
        }

        [[nodiscard]]
        PropertyAccessError TryInsert(const ObjectView& object, std::size_t index, const ValueView& value) const
        {
            using Error = PropertyAccessError;

            const auto container = TryReadValue(object);
            if (!container)
            {
                return container.Error;
            }

            if (!value.IsValid())
            {
                return Error::InvalidValue;
            }

            if (!IsContainer())
            {
                return Error::NotContainer;
            }

            if (index > m_container.Size(container.Value.m_address))
            {
                return Error::IndexOutOfRange;
            }

            if (object.m_writeAddress == nullptr)
            {
                return Error::ReadOnlyObject;
            }

            if (m_container.ReadOnly)
            {
                return Error::ReadOnlyProperty;
            }

            if (m_container.Insert == nullptr)
            {
                return Error::InsertUnavailable;
            }

            if (m_container.ElementType != value.GetCppType())
            {
                return Error::ValueTypeMismatch;
            }

            return m_container.Insert(
                object.m_writeAddress, index, value.m_address);
        }

        [[nodiscard]]
        bool CanErase() const noexcept
        {
            return m_container.Erase != nullptr;
        }

        [[nodiscard]]
        PropertyAccessError TryErase(const ObjectView& object, std::size_t index) const
        {
            using Error = PropertyAccessError;

            const auto container = TryReadValue(object);
            if (!container)
            {
                return container.Error;
            }

            if (!IsContainer())
            {
                return Error::NotContainer;
            }

            if (index >= m_container.Size(container.Value.m_address))
            {
                return Error::IndexOutOfRange;
            }

            if (object.m_writeAddress == nullptr)
            {
                return Error::ReadOnlyObject;
            }

            if (m_container.ReadOnly)
            {
                return Error::ReadOnlyProperty;
            }

            if (m_container.Erase == nullptr)
            {
                return Error::EraseUnavailable;
            }

            m_container.Erase(object.m_writeAddress, index);
            return Error::None;
        }

        template<typename T>
            requires std::is_object_v<T>
        [[nodiscard]]
        bool IsElementType() const noexcept
        {
            using Element = std::remove_cv_t<T>;
            return IsContainer() && m_container.ElementType == std::type_index(typeid(Element));
        }

        [[nodiscard]]
        PropertySizeResult TryGetSize(const ObjectView& object) const
        {
            using Error = PropertyAccessError;

            const auto container = TryReadValue(object);
            if (!container)
            {
                return { container.Error, 0 };
            }

            if (m_container.Size == nullptr)
            {
                return { Error::NotContainer, 0 };
            }

            return { Error::None, m_container.Size(container.Value.m_address) };
        }

        [[nodiscard]]
        PropertyReadResult TryReadElement(
            const ObjectView& object, std::size_t index) const
        {
            using Error = PropertyAccessError;

            const auto container = TryReadValue(object);
            if (!container)
            {
                return { container.Error, {} };
            }

            if (m_container.Size == nullptr)
            {
                return { Error::NotContainer, {} };
            }

            if (index >= m_container.Size(container.Value.m_address))
            {
                return { Error::IndexOutOfRange, {} };
            }

            if (m_container.ReadElement == nullptr)
            {
                return { Error::ElementUnavailable, {} };
            }

            return { Error::None, m_container.ReadElement(container.Value.m_address, index) };
        }

        [[nodiscard]]
        PropertyAccessError TryWriteElement(const ObjectView& object, std::size_t index, const ValueView& value) const
        {
            using Error = PropertyAccessError;

            const auto container = TryReadValue(object);
            if (!container)
            {
                return container.Error;
            }

            if (!value.IsValid())
            {
                return Error::InvalidValue;
            }

            if (m_container.Size == nullptr)
            {
                return Error::NotContainer;
            }

            if (index >= m_container.Size(container.Value.m_address))
            {
                return Error::IndexOutOfRange;
            }

            if (object.m_writeAddress == nullptr)
            {
                return Error::ReadOnlyObject;
            }

            if (m_container.ReadOnly)
            {
                return Error::ReadOnlyProperty;
            }

            if (m_container.WriteElement == nullptr)
            {
                return Error::ElementWriteUnavailable;
            }

            if (m_container.ElementType != value.GetCppType())
            {
                return Error::ValueTypeMismatch;
            }

            m_container.WriteElement(object.m_writeAddress, index, value.m_address);
            return Error::None;
        }

        [[nodiscard]]
        bool CanWriteEnumElement() const noexcept
        {
            return m_container.WriteEnumElement != nullptr;
        }

        [[nodiscard]]
        PropertyAccessError TryWriteEnumElement(
            const ObjectView& object, std::size_t index, const EnumValue& value) const
        {
            using Error = PropertyAccessError;

            const auto container = TryReadValue(object);
            if (!container)
            {
                return container.Error;
            }

            if (!IsContainer())
            {
                return Error::NotContainer;
            }

            if (index >= m_container.Size(container.Value.m_address))
            {
                return Error::IndexOutOfRange;
            }

            if (object.m_writeAddress == nullptr)
            {
                return Error::ReadOnlyObject;
            }

            if (m_container.ReadOnly)
            {
                return Error::ReadOnlyProperty;
            }

            if (m_container.WriteEnumElement == nullptr)
            {
                return Error::EnumWriteUnavailable;
            }

            return m_container.WriteEnumElement(object.m_writeAddress, index, value);
        }

        [[nodiscard]]
        bool CanAppendEnumElement() const noexcept
        {
            return m_container.AppendEnumElement != nullptr;
        }

        [[nodiscard]]
        PropertyAccessError TryAppendEnumElement(const ObjectView& object, const EnumValue& value) const
        {
            using Error = PropertyAccessError;

            const auto container = TryReadValue(object);
            if (!container)
            {
                return container.Error;
            }

            if (!IsContainer())
            {
                return Error::NotContainer;
            }

            if (object.m_writeAddress == nullptr)
            {
                return Error::ReadOnlyObject;
            }

            if (m_container.ReadOnly)
            {
                return Error::ReadOnlyProperty;
            }

            if (m_container.AppendEnumElement == nullptr)
            {
                return Error::EnumWriteUnavailable;
            }

            return m_container.AppendEnumElement(object.m_writeAddress, value);
        }

        [[nodiscard]]
        bool CanInsertEnumElement() const noexcept
        {
            return m_container.InsertEnumElement != nullptr;
        }

        [[nodiscard]]
        PropertyAccessError TryInsertEnumElement(
            const ObjectView& object, std::size_t index, const EnumValue& value) const
        {
            using Error = PropertyAccessError;

            const auto container = TryReadValue(object);
            if (!container)
            {
                return container.Error;
            }

            if (!IsContainer())
            {
                return Error::NotContainer;
            }

            if (index > m_container.Size(container.Value.m_address))
            {
                return Error::IndexOutOfRange;
            }

            if (object.m_writeAddress == nullptr)
            {
                return Error::ReadOnlyObject;
            }

            if (m_container.ReadOnly)
            {
                return Error::ReadOnlyProperty;
            }

            if (m_container.InsertEnumElement == nullptr)
            {
                return Error::EnumWriteUnavailable;
            }

            return m_container.InsertEnumElement(object.m_writeAddress, index, value);
        }

        [[nodiscard]]
        PropertyObjectResult TryEditElementObject(const ObjectView& object, std::size_t index) const
        {
            using Error = PropertyAccessError;

            const auto container = TryReadValue(object);
            if (!container)
            {
                return { container.Error, {} };
            }

            if (m_container.Size == nullptr)
            {
                return { Error::NotContainer, {} };
            }

            if (index >= m_container.Size(container.Value.m_address))
            {
                return { Error::IndexOutOfRange, {} };
            }

            if (!m_container.ObjectElement)
            {
                return { Error::ObjectUnavailable, {} };
            }

            if (object.m_writeAddress == nullptr)
            {
                return { Error::ReadOnlyObject, {} };
            }

            if (m_container.ReadOnly)
            {
                return { Error::ReadOnlyProperty, {} };
            }

            if (m_container.EditElementObject == nullptr)
            {
                return { Error::ObjectUnavailable, {} };
            }

            return { Error::None, m_container.EditElementObject(object.m_writeAddress, index) };
        }

    private:
        std::type_index m_ownerType{ typeid(void) };
        std::type_index m_valueType{ typeid(void) };
        PropertyValueKind m_valueKind = PropertyValueKind::Unknown;

        const void* (*m_read)(const void*) = nullptr;
        void (*m_write)(void*, const void*) = nullptr;
        PropertyAccessError(*m_writeEnumValue)(const PropertyInfo&, const ObjectView&, const EnumValue&) = nullptr;
        bool m_objectValue = false;
        ObjectView(*m_editObject)(void*) = nullptr;
        detail::PropertyContainerAccess m_container;

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
        property.m_valueKind = detail::GetPropertyValueKind<Value>();

        property.m_read = +[](const void* object) -> const void*
            {
                const auto* owner = static_cast<const Owner*>(object);

                return std::addressof(owner->*Member);
            };

        property.m_objectValue = std::is_class_v<Value> || std::is_union_v<Value>;

        if constexpr ((std::is_class_v<Value> || std::is_union_v<Value>) &&
            !std::is_const_v<MemberValue>)
        {
            property.m_editObject = +[](void* object) -> ObjectView
                {
                    auto* owner = static_cast<Owner*>(object);
                    return ObjectView::From(owner->*Member);
                };
        }

        if constexpr (std::is_const_v<MemberValue>)
        {
            property.m_writeStatus = PropertyAccessStatus::ConstQualified;
        }
        else if constexpr (detail::SupportsPropertyCopyAssignment<MemberValue>::value)
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

        if constexpr (std::is_enum_v<Value>)
        {
            using Underlying = std::underlying_type_t<Value>;

            if constexpr (!std::is_const_v<MemberValue> && !std::is_convertible_v<Value, Underlying>)
            {
                property.m_writeEnumValue =
                    +[](const PropertyInfo& target, const ObjectView& object,
                        const EnumValue& number) -> PropertyAccessError
                    {
                        const auto converted = detail::ConvertEnumValue<Value>(number);
                        if (!converted)
                        {
                            return PropertyAccessError::ValueOutOfRange;
                        }

                        return target.TryWriteValue(object, ValueView::From(*converted));
                    };
            }
        }

        property.m_container = detail::MakePropertyContainerAccess<Member, Owner>();
        return property;
    }
}