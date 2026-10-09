#pragma once

#include <archive/GlazeArchiveBase.h>
#include <reflection/TypeInfo.h>
#include <reflection/Registry.h>

#include "PropertyArchiveCodecs.h"

#include <variant>
#include <concepts>
#include <type_traits>
#include <cstdint>
#include <string>
#include <string_view>
#include <memory>
#include <typeindex>
#include <utility>
#include <vector>

namespace archive_reflection
{
    [[nodiscard]] inline archive::ArchiveResult IsSerializationEnabled(
        const reflection::Metadata& metadata, std::string_view subject, bool& enabled)
    {
        enabled = true;
        constexpr std::string_view keys[]{ "Serialize", "NoSerialize", "Transient" };

        for (const auto key : keys)
        {
            const auto* entry = metadata.Find(key);

            if (entry == nullptr)
            {
                continue;
            }

            const auto* value = std::get_if<bool>(entry);

            if (value == nullptr)
            {
                enabled = false;
                return { archive::ArchiveErrorCode::InvalidValue, std::string{ subject } };
            }

            enabled = enabled && (key == "Serialize" ? *value : !*value);
        }

        return {};
    }

    namespace detail
    {
        inline archive::ArchiveResult WriteProperties(archive::GlazeArchiveBase& ar,
            const reflection::TypeInfo& type, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs);

        inline archive::ArchiveResult ReadProperties(archive::GlazeArchiveBase& ar,
            const reflection::TypeInfo& type, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs);

        inline archive::ArchiveResult ReadBoolMetadata(const reflection::PropertyInfo& property,
            std::string_view key, bool defaultValue, bool& value)
        {
            value = defaultValue;
            const auto* metadata = property.Metadata.Find(key);

            if (metadata == nullptr)
            {
                return {};
            }

            const auto* boolean = std::get_if<bool>(metadata);

            if (boolean == nullptr)
            {
                return { archive::ArchiveErrorCode::InvalidValue, property.Name };
            }

            value = *boolean;
            return {};
        }

        inline archive::ArchiveResult ReadSerializationPolicy(const reflection::PropertyInfo& property, bool& enabled)
        {
            return archive_reflection::IsSerializationEnabled(property.Metadata, property.Name, enabled);
        }

        template<typename T>
        bool TryWriteScalar(archive::GlazeArchiveBase& ar, const reflection::PropertyInfo& property,
            const reflection::ValueView& value, archive::ArchiveResult& result)
        {
            const auto* typedValue = value.Get<T>();

            if (typedValue == nullptr)
            {
                return false;
            }

            result = ar.Write(property.Name, *typedValue);
            return true;
        }

        template<typename... Types>
        archive::ArchiveResult WriteScalar(archive::GlazeArchiveBase& ar,
            const reflection::PropertyInfo& property, const reflection::ValueView& value)
        {
            archive::ArchiveResult result;

            if ((TryWriteScalar<Types>(ar, property, value, result) || ...))
            {
                return result;
            }

            return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
        }

        inline archive::ArchiveResult WriteEnum(archive::GlazeArchiveBase& ar,
            const reflection::PropertyInfo& property, const reflection::ValueView& value,
            const reflection::Registry* registry)
        {
            const auto* enumeration = registry != nullptr ? registry->FindEnum(value) : nullptr;

            if (enumeration == nullptr || !enumeration->IsScoped)
            {
                return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
            }

            const auto number = enumeration->ReadValue(value);

            if (!number)
            {
                return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
            }

            return std::visit([&](auto stored)
                {
                    return ar.Write(property.Name, stored);
                }, *number);
        }

        inline archive::ArchiveResult ReadEnum(const archive::GlazeArchiveBase& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object,
            const reflection::Registry* registry)
        {
            if (registry == nullptr || !property.CanWriteEnumValue())
            {
                return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
            }

            const auto current = property.TryReadValue(object);

            if (!current)
            {
                return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
            }

            const auto* enumeration = registry->FindEnum(current.Value);

            if (enumeration == nullptr || !enumeration->IsScoped)
            {
                return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
            }

            const auto currentNumber = enumeration->ReadValue(current.Value);

            if (!currentNumber)
            {
                return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
            }

            return std::visit([&](auto existing) -> archive::ArchiveResult
                {
                    using Storage = decltype(existing);
                    Storage stored{};
                    const auto result = ar.Read(property.Name, stored);

                    if (!result)
                    {
                        return result;
                    }

                    const auto error = property.TryWriteEnumValue(object, reflection::EnumValue{ stored });

                    if (error == reflection::PropertyAccessError::None)
                    {
                        return {};
                    }

                    const auto code = error == reflection::PropertyAccessError::ValueOutOfRange
                        ? archive::ArchiveErrorCode::InvalidValue : archive::ArchiveErrorCode::InvalidOperation;

                    return { code, property.Name };
                }, *currentNumber);
        }

        inline archive::ArchiveResult WithParentField(std::string_view parent, archive::ArchiveResult result)
        {
            if (!result)
            {
                const auto suffix = result.Field.empty() ? std::string{} : "." + result.Field;
                result.Field = std::string{ parent } + suffix;
            }

            return result;
        }

        inline archive::ArchiveResult WriteNestedObject(archive::GlazeArchiveBase& ar,
            const reflection::PropertyInfo& property, const reflection::ValueView& value,
            const reflection::Registry* registry, const PropertyCodecs* codecs)
        {
            const auto* nestedType = registry != nullptr ? registry->FindType(value) : nullptr;

            if (nestedType == nullptr)
            {
                return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
            }

            const auto nestedObject = nestedType->AsObject(value);

            if (!nestedObject.IsValid())
            {
                return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
            }

            return ar.WriteObject(property.Name, nestedObject,
                [&property, nestedType, registry, codecs](archive::GlazeArchiveBase& temporary,
                    const reflection::ObjectView& source)
                {
                    return WithParentField(
                        property.Name, WriteProperties(temporary, *nestedType, source, registry, codecs));
                });
        }

        inline archive::ArchiveResult ReadNestedObject(archive::GlazeArchiveBase& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs)
        {
            if (registry == nullptr)
            {
                return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
            }

            const auto nested = property.TryEditObject(object);

            if (!nested)
            {
                return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
            }

            const auto* nestedType = registry->FindType(nested.Object);

            if (nestedType == nullptr)
            {
                return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
            }

            const auto beginResult = ar.BeginReadObject(property.Name);

            if (!beginResult)
            {
                return beginResult;
            }

            const auto result = ReadProperties(ar, *nestedType, nested.Object, registry, codecs);
            const auto endResult = ar.EndObject();

            if (!result)
            {
                return WithParentField(property.Name, result);
            }

            return WithParentField(property.Name, endResult);
        }

        inline archive::ArchiveResult WriteProperties(archive::GlazeArchiveBase& ar,
            const reflection::TypeInfo& type, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs)
        {
            for (const auto& property : type.Properties)
            {
                bool enabled = true;
                const auto policyResult = ReadSerializationPolicy(property, enabled);

                if (!policyResult)
                {
                    return policyResult;
                }

                if (!enabled)
                {
                    continue;
                }

                const auto readResult = property.TryReadValue(object);

                if (!readResult)
                {
                    return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
                }

                archive::ArchiveResult result;

                if (codecs != nullptr)
                {
                    const auto custom = codecs->WriteValue(ar, property.Name, readResult.Value);

                    if (custom)
                    {
                        if (!*custom)
                        {
                            return *custom;
                        }

                        continue;
                    }
                }

                if (property.GetValueKind() == reflection::PropertyValueKind::Enum)
                {
                    result = WriteEnum(ar, property, readResult.Value, registry);
                }
                else if (property.GetValueKind() == reflection::PropertyValueKind::Class)
                {
                    result = WriteNestedObject(ar, property, readResult.Value, registry, codecs);
                }
                else
                {
                    result = WriteScalar<
                        bool, std::int8_t, std::uint8_t, std::int16_t, std::uint16_t,
                        std::int32_t, std::uint32_t, std::int64_t, std::uint64_t,
                        float, double, std::string, std::wstring>(ar, property, readResult.Value);
                }

                if (!result)
                {
                    return result;
                }
            }

            return {};
        }

        template<typename T>
        bool TryReadScalar(const archive::GlazeArchiveBase& ar, const reflection::PropertyInfo& property,
            const reflection::ObjectView& object, archive::ArchiveResult& result)
        {
            if (!property.IsValueType<T>())
            {
                return false;
            }

            T value{};
            result = ar.Read(property.Name, value);

            if (result && !property.WriteTo<T>(object, value))
            {
                result = { archive::ArchiveErrorCode::InvalidOperation, property.Name };
            }

            return true;
        }

        template<typename... Types>
        archive::ArchiveResult ReadScalar(const archive::GlazeArchiveBase& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object)
        {
            archive::ArchiveResult result;

            if ((TryReadScalar<Types>(ar, property, object, result) || ...))
            {
                return result;
            }

            return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
        }

        inline archive::ArchiveResult ReadProperties(archive::GlazeArchiveBase& ar,
            const reflection::TypeInfo& type, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs)
        {
            for (const auto& property : type.Properties)
            {
                bool enabled = true;
                const auto policyResult = ReadSerializationPolicy(property, enabled);

                if (!policyResult)
                {
                    return policyResult;
                }

                if (!enabled)
                {
                    continue;
                }

                bool optional = false;
                const auto optionalResult = ReadBoolMetadata(property, "Optional", false, optional);

                if (!optionalResult)
                {
                    return optionalResult;
                }

                if (optional)
                {
                    bool exists = false;
                    const auto fieldResult = ar.HasField(property.Name, exists);

                    if (!fieldResult)
                    {
                        return fieldResult;
                    }

                    if (!exists)
                    {
                        continue;
                    }
                }

                if (!property.CanWrite())
                {
                    return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
                }

                archive::ArchiveResult result;

                if (codecs != nullptr)
                {
                    const auto custom = codecs->ReadProperty(ar, property, object);

                    if (custom)
                    {
                        if (!*custom)
                        {
                            return *custom;
                        }

                        continue;
                    }
                }

                if (property.GetValueKind() == reflection::PropertyValueKind::Enum)
                {
                    result = ReadEnum(ar, property, object, registry);
                }
                else if (property.GetValueKind() == reflection::PropertyValueKind::Class)
                {
                    result = ReadNestedObject(ar, property, object, registry, codecs);
                }
                else
                {
                    result = ReadScalar<
                        bool, std::int8_t, std::uint8_t, std::int16_t, std::uint16_t,
                        std::int32_t, std::uint32_t, std::int64_t, std::uint64_t,
                        float, double, std::string, std::wstring>(ar, property, object);
                }

                if (!result)
                {
                    return result;
                }
            }

            return {};
        }
    }


    [[nodiscard]] inline archive::ArchiveResult WriteObject(archive::GlazeArchiveBase& ar,
        std::string_view key, const reflection::TypeInfo& type, const reflection::ObjectView& object,
        const reflection::Registry* registry = nullptr, const PropertyCodecs* codecs = nullptr)
    {
        if (!object.IsValid() || object.GetCppType() != type.GetCppType())
        {
            return { archive::ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        return ar.WriteObject(key, object,
            [&type, registry, codecs](archive::GlazeArchiveBase& temporary, const reflection::ObjectView& source)
            {
                return detail::WriteProperties(temporary, type, source, registry, codecs);
            });
    }

    template<typename T>
        requires ((std::is_class_v<T> || std::is_union_v<T>) &&
    !std::is_const_v<T> && !std::is_volatile_v<T>&&
        std::copy_constructible<T>&& std::assignable_from<T&, T>)
        [[nodiscard]] archive::ArchiveResult ReadObject(const archive::GlazeArchiveBase& ar,
            std::string_view key, const reflection::TypeInfo& type, T& object,
            const reflection::Registry* registry = nullptr, const PropertyCodecs* codecs = nullptr)
    {
        if (reflection::ObjectView::From(object).GetCppType() != type.GetCppType())
        {
            return { archive::ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        return ar.ReadObject(key, object,
            [&type, registry, codecs](archive::GlazeArchiveBase& temporary, T& candidate)
            {
                return detail::ReadProperties(temporary, type, reflection::ObjectView::From(candidate), registry, codecs);
            });
    }

    template<archive::ArchiveObjectMap Map>
        requires (std::default_initializable<Map>&&
    std::default_initializable<typename Map::mapped_type>&&
        std::move_constructible<typename Map::mapped_type>&&
        reflection::detail::SupportsPropertyCopyConstruction<Map>::value)
        [[nodiscard]] bool RegisterObjectMap(PropertyCodecs& codecs, reflection::TypeInfo valueType,
            const reflection::Registry* registry = nullptr, const PropertyCodecs* valueCodecs = nullptr)
    {
        using Mapped = typename Map::mapped_type;

        if (valueType.GetCppType() != std::type_index(typeid(Mapped)))
        {
            return false;
        }

        auto type = std::make_shared<const reflection::TypeInfo>(std::move(valueType));

        return codecs.Register<Map>(
            [type, registry, valueCodecs](archive::GlazeArchiveBase& ar,
                std::string_view key, const Map& values) -> archive::ArchiveResult
            {
                return ar.WriteObjectMap(key, values,
                    [type, registry, valueCodecs](archive::GlazeArchiveBase& entry,
                        const Mapped& value) -> archive::ArchiveResult
                    {
                        return detail::WriteProperties(
                            entry, *type, reflection::ObjectView::From(value), registry, valueCodecs);
                    });
            },
            [type, registry, valueCodecs](const archive::GlazeArchiveBase& ar,
                std::string_view key, Map& values) -> archive::ArchiveResult
            {
                return ar.ReadObjectMap(key, values,
                    [type, registry, valueCodecs](archive::GlazeArchiveBase& entry,
                        Mapped& value) -> archive::ArchiveResult
                    {
                        return detail::ReadProperties(
                            entry, *type, reflection::ObjectView::From(value), registry, valueCodecs);
                    });
            });
    }

    template<typename T>
        requires ((std::is_class_v<T> || std::is_union_v<T>) &&
    !std::is_const_v<T> && !std::is_volatile_v<T>&&
        std::default_initializable<T>&& std::move_constructible<T>&&
        reflection::detail::SupportsPropertyCopyConstruction<std::vector<T>>::value)
        [[nodiscard]] bool RegisterObjectArray(PropertyCodecs& codecs, reflection::TypeInfo elementType,
            const reflection::Registry* registry = nullptr, const PropertyCodecs* elementCodecs = nullptr)
    {
        if (elementType.GetCppType() != std::type_index{ typeid(T) })
        {
            return false;
        }

        auto type = std::make_shared<const reflection::TypeInfo>(std::move(elementType));

        return codecs.Register<std::vector<T>>(
            [type, registry, elementCodecs](archive::GlazeArchiveBase& ar,
                std::string_view key, const std::vector<T>& values) -> archive::ArchiveResult
            {
                return ar.WriteObjectArray(key, values,
                    [type, registry, elementCodecs](archive::GlazeArchiveBase& entry,
                        const T& value) -> archive::ArchiveResult
                    {
                        return detail::WriteProperties(
                            entry, *type, reflection::ObjectView::From(value), registry, elementCodecs);
                    });
            },
            [type, registry, elementCodecs](const archive::GlazeArchiveBase& ar,
                std::string_view key, std::vector<T>& values) -> archive::ArchiveResult
            {
                return ar.ReadObjectArray(key, values,
                    [type, registry, elementCodecs](archive::GlazeArchiveBase& entry,
                        T& value) -> archive::ArchiveResult
                    {
                        return detail::ReadProperties(
                            entry, *type, reflection::ObjectView::From(value), registry, elementCodecs);
                    });
            });
    }
}