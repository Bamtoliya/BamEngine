#pragma once

#include <archive/ArchiveInterface.h>
#include <reflection/TypeInfo.h>
#include <reflection/Registry.h>

#include "PropertyArchiveCodecs.h"
#include "detail/EnumArchiveAdapter.h"

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
#include <set>
#include <unordered_set>
#include <cstddef>
#include <optional>

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
        inline archive::ArchiveResult WriteProperty(archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs);

        inline archive::ArchiveResult ReadProperty(archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs);

        inline archive::ArchiveResult WriteProperties(archive::IArchive& ar,
            const reflection::TypeInfo& type, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs);

        inline archive::ArchiveResult ReadProperties(archive::IArchive& ar,
            const reflection::TypeInfo& type, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs);

        inline archive::ArchiveResult WriteObjectSequence(archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs)
        {
            const auto size = property.TryGetSize(object);

            if (!size)
            {
                return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
            }

            if (codecs != nullptr && codecs->HasCodec(property.GetElementCppType()))
            {
                return ar.WriteArrayElements(property.Name, size.Size,
                    [&property, &object, codecs](archive::IArchive& entry,
                        std::string_view key, std::size_t index) -> archive::ArchiveResult
                    {
                        const auto element = property.TryReadElement(object, index);
                        if (!element)
                        {
                            return { archive::ArchiveErrorCode::InvalidOperation, {} };
                        }

                        const auto custom = codecs->WriteValue(entry, key, element.Value);
                        return custom.value_or(archive::ArchiveResult{
                            archive::ArchiveErrorCode::UnsupportedType, property.Name });
                    });
            }

            const auto* elementType = registry != nullptr
                ? registry->FindType(property.GetElementCppType()) : nullptr;

            if (elementType == nullptr)
            {
                return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
            }

            return ar.WriteObjectArray(property.Name, size.Size,
                [&property, &object, elementType, registry, codecs](
                    archive::IArchive& temporary, std::size_t index) -> archive::ArchiveResult
                {
                    const auto element = property.TryReadElement(object, index);

                    if (!element)
                    {
                        return { archive::ArchiveErrorCode::InvalidOperation, {} };
                    }

                    const auto source = elementType->AsObject(element.Value);

                    if (!source.IsValid())
                    {
                        return { archive::ArchiveErrorCode::InvalidOperation, {} };
                    }

                    return WriteProperties(temporary, *elementType, source, registry, codecs);
                });
        }

        inline archive::ArchiveResult PrepareSequence(archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object)
        {
            const bool fixed = property.GetValueKind() == reflection::PropertyValueKind::Array;

            if ((!property.CanEditElementObject() && !property.CanWriteElement()) ||
                (fixed ? !property.CanResetElements() : (!property.CanClear() || !property.CanResize())))
            {
                return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
            }

            std::size_t size = 0;
            const auto sizeResult = ar.GetArraySize(property.Name, size);

            if (!sizeResult)
            {
                return sizeResult;
            }

            if (fixed)
            {
                const auto current = property.TryGetSize(object);

                if (!current)
                {
                    return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
                }

                if (current.Size != size)
                {
                    return { archive::ArchiveErrorCode::InvalidValue, property.Name };
                }

                if (property.TryResetElements(object) != reflection::PropertyAccessError::None)
                {
                    return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
                }
            }
            else
            {
                if (property.TryClear(object) != reflection::PropertyAccessError::None)
                {
                    return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
                }

                const auto resizeError = property.TryResize(object, size);

                if (resizeError != reflection::PropertyAccessError::None)
                {
                    const auto code = resizeError == reflection::PropertyAccessError::SizeOutOfRange
                        ? archive::ArchiveErrorCode::InvalidValue : archive::ArchiveErrorCode::InvalidOperation;
                    return { code, property.Name };
                }
            }

            return {};
        }

        inline archive::ArchiveResult ReadObjectSequence(archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs)
        {
            if (codecs != nullptr && codecs->HasCodec(property.GetElementCppType()))
            {
                if (!property.CanWriteElement())
                {
                    return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
                }

                const auto prepared = PrepareSequence(ar, property, object);
                if (!prepared)
                {
                    return prepared;
                }

                return ar.ReadArrayElements(property.Name,
                    [&property, &object, codecs](archive::IArchive& entry,
                        std::string_view key, std::size_t index) -> archive::ArchiveResult
                    {
                        const auto custom = codecs->ReadValue(entry, key, property.GetElementCppType(),
                            [&](const reflection::ValueView& value) -> archive::ArchiveResult
                            {
                                return property.TryWriteElement(object, index, value) == reflection::PropertyAccessError::None
                                    ? archive::ArchiveResult{}
                                    : archive::ArchiveResult{ archive::ArchiveErrorCode::InvalidOperation, property.Name };
                            });
                        return custom.value_or(archive::ArchiveResult{
                            archive::ArchiveErrorCode::UnsupportedType, property.Name });
                    });
            }

            const auto* elementType = registry != nullptr
                ? registry->FindType(property.GetElementCppType()) : nullptr;

            if (elementType == nullptr || !property.CanEditElementObject())
            {
                return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
            }

            const auto prepared = PrepareSequence(ar, property, object);
            if (!prepared)
            {
                return prepared;
            }

            return ar.ReadObjectArray(property.Name,
                [&property, &object, elementType, registry, codecs](
                    archive::IArchive& temporary, std::size_t index) -> archive::ArchiveResult
                {
                    const auto element = property.TryEditElementObject(object, index);

                    if (!element || element.Object.GetCppType() != elementType->GetCppType())
                    {
                        return { archive::ArchiveErrorCode::InvalidOperation, {} };
                    }

                    return ReadProperties(temporary, *elementType, element.Object, registry, codecs);
                });
        }

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

        struct PropertyTarget
        {
            const reflection::PropertyInfo* Property = nullptr;
            reflection::ObjectView Owner;
        };

        struct PropertyTraversal
        {
            std::vector<reflection::ObjectView> Objects;
            std::unordered_set<std::string_view> Names;
            std::vector<PropertyTarget> Properties;
        };

        inline archive::ArchiveResult CollectProperties(const reflection::TypeInfo& type,
            const reflection::ObjectView& object, const reflection::Registry* registry, PropertyTraversal& traversal)
        {
            if (!object.IsValid() || object.GetCppType() != type.GetCppType())
            {
                return { archive::ArchiveErrorCode::InvalidOperation, type.QualifiedName };
            }
            for (const auto& existing : traversal.Objects)
            {
                if (object.RefersToSameObject(existing))
                {
                    return {}; // A shared virtual base is one subobject, regardless of the path used to reach it.
                }
            }
            traversal.Objects.push_back(object);
            for (const auto& base : type.Bases)
            {
                const auto baseObject = base.AsBase(object);
                if (!baseObject.IsValid())
                {
                    return { archive::ArchiveErrorCode::InvalidOperation, base.TypeName };
                }
                const auto* baseType = registry != nullptr ? registry->FindType(base.GetBaseCppType()) : nullptr;
                if (baseType == nullptr)
                {
                    // ITypeIdentity 등 리플렉션 대상이 아닌 순수 인터페이스/기반 클래스는 프로퍼티 수집을 건너뜁니다.
                    continue;
                }
                const auto result = CollectProperties(*baseType, baseObject, registry, traversal);
                if (!result)
                {
                    return result;
                }
            }
            for (const auto& property : type.Properties)
            {
                bool enabled = true;
                const auto policy = ReadSerializationPolicy(property, enabled);
                if (!policy)
                {
                    return policy;
                }
                if (!enabled)
                {
                    continue;
                }
                if (!traversal.Names.insert(property.Name).second)
                {
                    return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
                }
                traversal.Properties.push_back(PropertyTarget{ &property, object });
            }
            return {};
        }

        template<typename Visitor>
        archive::ArchiveResult VisitProperties(const reflection::TypeInfo& type,
            const reflection::ObjectView& object, const reflection::Registry* registry, Visitor&& visitor)
        {
            if (type.Bases.empty())
            {
                for (const auto& property : type.Properties)
                {
                    const auto result = std::invoke(visitor, property, object);
                    if (!result)
                    {
                        return result;
                    }
                }
                return {};
            }
            PropertyTraversal traversal;
            const auto collected = CollectProperties(type, object, registry, traversal);
            if (!collected)
            {
                return collected;
            }
            for (const auto& target : traversal.Properties)
            {
                const auto result = std::invoke(visitor, *target.Property, target.Owner);
                if (!result)
                {
                    return result;
                }
            }
            return {};
        }

        template<typename T>
        bool TryWriteScalar(archive::IArchive& ar, const reflection::PropertyInfo& property,
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
        archive::ArchiveResult WriteScalar(archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ValueView& value)
        {
            archive::ArchiveResult result;

            if ((TryWriteScalar<Types>(ar, property, value, result) || ...))
            {
                return result;
            }

            return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
        }

        inline archive::ArchiveResult WriteEnum(archive::IArchive& ar,
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

        template<typename Container>
        bool TryWriteArray(archive::IArchive& ar, const reflection::PropertyInfo& property,
            const reflection::ValueView& value, archive::ArchiveResult& result)
        {
            const auto* values = value.Get<Container>();

            if (values == nullptr)
            {
                return false;
            }

            result = ar.WriteArray(property.Name, *values);
            return true;
        }

        template<typename... Types>
        std::optional<archive::ArchiveResult> WriteScalarArray(archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ValueView& value)
        {
            archive::ArchiveResult result;

            if (((TryWriteArray<std::vector<Types>>(ar, property, value, result) ||
                TryWriteArray<std::set<Types>>(ar, property, value, result) ||
                TryWriteArray<std::unordered_set<Types>>(ar, property, value, result)) || ...))
            {
                return result;
            }

            return std::nullopt;
        }

        template<typename Container>
        bool TryReadArray(const archive::IArchive& ar, const reflection::PropertyInfo& property,
            const reflection::ObjectView& object, archive::ArchiveResult& result)
        {
            if (!property.IsValueType<Container>())
            {
                return false;
            }

            const auto* current = property.ReadFrom<Container>(object);

            if (current == nullptr)
            {
                result = { archive::ArchiveErrorCode::InvalidOperation, property.Name };
                return true;
            }

            Container values = *current;
            result = ar.ReadArray(property.Name, values);

            if (result && !property.WriteTo<Container>(object, values))
            {
                result = { archive::ArchiveErrorCode::InvalidOperation, property.Name };
            }

            return true;
        }

        template<typename... Types>
        std::optional<archive::ArchiveResult> ReadScalarArray(const archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object)
        {
            archive::ArchiveResult result;

            if (((TryReadArray<std::vector<Types>>(ar, property, object, result) ||
                TryReadArray<std::set<Types>>(ar, property, object, result) ||
                TryReadArray<std::unordered_set<Types>>(ar, property, object, result)) || ...))
            {
                return result;
            }

            return std::nullopt;
        }

        template<typename T>
        bool TryWriteFixedArray(archive::IArchive& ar, const reflection::PropertyInfo& property,
            const reflection::ObjectView& object, archive::ArchiveResult& result)
        {
            if (!property.IsElementType<T>())
            {
                return false;
            }

            const auto size = property.TryGetSize(object);

            if (!size)
            {
                result = { archive::ArchiveErrorCode::InvalidOperation, property.Name };
                return true;
            }

            std::vector<T> values;
            values.reserve(size.Size);

            for (std::size_t index = 0; index < size.Size; ++index)
            {
                const auto element = property.TryReadElement(object, index);
                const auto* value = element ? element.Value.Get<T>() : nullptr;

                if (value == nullptr)
                {
                    result = { archive::ArchiveErrorCode::InvalidOperation, property.Name };
                    return true;
                }

                values.push_back(*value);
            }

            result = ar.WriteArray(property.Name, values);
            return true;
        }

        template<typename... Types>
        std::optional<archive::ArchiveResult> WriteFixedArray(archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object)
        {
            archive::ArchiveResult result;

            if ((TryWriteFixedArray<Types>(ar, property, object, result) || ...))
            {
                return result;
            }

            return std::nullopt;
        }

        template<typename T>
        bool TryReadFixedArray(const archive::IArchive& ar, const reflection::PropertyInfo& property,
            const reflection::ObjectView& object, archive::ArchiveResult& result)
        {
            if (!property.IsElementType<T>())
            {
                return false;
            }

            const auto size = property.TryGetSize(object);

            if (!size || !property.CanWriteElement())
            {
                result = { archive::ArchiveErrorCode::InvalidOperation, property.Name };
                return true;
            }

            std::vector<T> values;
            result = ar.ReadArray(property.Name, values);

            if (!result)
            {
                return true;
            }

            if (values.size() != size.Size)
            {
                result = { archive::ArchiveErrorCode::InvalidValue, property.Name };
                return true;
            }

            for (std::size_t index = 0; index < values.size(); ++index)
            {
                const T value = values[index];
                const auto error = property.TryWriteElement(object, index, reflection::ValueView::From(value));

                if (error != reflection::PropertyAccessError::None)
                {
                    result = { archive::ArchiveErrorCode::InvalidOperation, property.Name };
                    return true;
                }
            }

            return true;
        }

        template<typename... Types>
        std::optional<archive::ArchiveResult> ReadFixedArray(const archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object)
        {
            archive::ArchiveResult result;

            if ((TryReadFixedArray<Types>(ar, property, object, result) || ...))
            {
                return result;
            }

            return std::nullopt;
        }

        inline archive::ArchiveResult ReadEnum(const archive::IArchive& ar,
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

        template<typename... Types>
        struct ScalarTypeDispatch
        {
            static bool Contains(std::type_index type) noexcept
            {
                return ((type == std::type_index{ typeid(Types) }) || ...);
            }

            template<typename Visitor>
            static archive::ArchiveResult Visit(std::type_index type, std::string_view field, Visitor&& visitor)
            {
                archive::ArchiveResult result{ archive::ArchiveErrorCode::UnsupportedType, std::string{ field } };
                ((type == std::type_index{ typeid(Types) }
                    ? (result = visitor.template operator()<Types>(), true) : false) || ...);
                return result;
            }
        };

        using MapScalarTypes = ScalarTypeDispatch<bool, std::int8_t, std::uint8_t, std::int16_t, std::uint16_t,
            std::int32_t, std::uint32_t, std::int64_t, std::uint64_t, float, double, std::string, std::wstring>;

        inline bool IsMapScalarType(std::type_index type, const reflection::Registry* registry)
        {
            return MapScalarTypes::Contains(type) || FindSupportedEnum(registry, type) != nullptr;
        }

        inline archive::ArchiveResult WriteMapScalar(archive::IArchive& ar,
            std::string_view field, const reflection::ValueView& value, const reflection::Registry* registry)
        {
            if (!MapScalarTypes::Contains(value.GetCppType()))
            {
                return WriteEnumValue(ar, field, value, registry);
            }

            return MapScalarTypes::Visit(value.GetCppType(), field, [&]<typename T>() -> archive::ArchiveResult
                {
                    const auto* source = value.Get<T>();

                    if (source == nullptr)
                    {
                        return { archive::ArchiveErrorCode::InvalidOperation, std::string{ field } };
                    }

                    return ar.Write(field, *source);
                });
        }

        inline archive::ArchiveResult MapInsertionResult(reflection::PropertyAccessError error)
        {
            using Error = reflection::PropertyAccessError;

            if (error == Error::None)
            {
                return {};
            }

            if (error == Error::KeyAlreadyExists || error == Error::SizeOutOfRange)
            {
                return { archive::ArchiveErrorCode::InvalidValue, "Key" };
            }

            return { archive::ArchiveErrorCode::InvalidOperation, "Value" };
        }

        template<typename Visitor>
        archive::ArchiveResult ReadMapScalar(const archive::IArchive& ar, std::string_view field,
            std::type_index type, const reflection::Registry* registry, Visitor&& visitor)
        {
            if (!MapScalarTypes::Contains(type))
            {
                const auto* enumeration = FindSupportedEnum(registry, type);

                if (enumeration == nullptr)
                {
                    return { archive::ArchiveErrorCode::UnsupportedType, std::string{ field } };
                }

                return ReadEnumValue(ar, field, *enumeration, visitor);
            }

            return MapScalarTypes::Visit(type, field,
                [&]<typename T>() -> archive::ArchiveResult
                {
                    T value{};
                    const auto result = ar.Read(field, value);

                    if (!result)
                    {
                        return result;
                    }

                    return std::invoke(visitor, reflection::ValueView::From(value));
                });
        }

        inline bool IsAutomaticContainerSupported(const reflection::PropertyInfo& property,
            const reflection::Registry* registry, const PropertyCodecs* codecs)
        {
            if (codecs != nullptr && codecs->HasCodec(property.GetValueCppType()))
            {
                return true;
            }
            if (property.IsMap())
            {
                if (!IsMapScalarType(property.GetMapKeyType(), registry))
                {
                    return false;
                }
                const auto* nested = property.GetMapValueContainerProperty();
                return nested != nullptr ? IsAutomaticContainerSupported(*nested, registry, codecs)
                    : IsMapScalarType(property.GetMapValueType(), registry) ||
                    (codecs != nullptr && codecs->HasCodec(property.GetMapValueType())) ||
                    (registry != nullptr && registry->FindType(property.GetMapValueType()) != nullptr);
            }
            const bool sequence = property.GetValueKind() == reflection::PropertyValueKind::Vector ||
                property.GetValueKind() == reflection::PropertyValueKind::Array;
            if (const auto* nested = property.GetElementContainerProperty())
            {
                return sequence && IsAutomaticContainerSupported(*nested, registry, codecs);
            }
            if (MapScalarTypes::Contains(property.GetElementCppType()))
            {
                if (property.GetValueKind() == reflection::PropertyValueKind::Array)
                {
                    return true;
                }
                bool supported = false;
                const auto checked = MapScalarTypes::Visit(property.GetElementCppType(), property.Name,
                    [&]<typename T>() -> archive::ArchiveResult
                    {
                        supported = property.IsValueType<std::vector<T>>() || property.IsValueType<std::set<T>>() ||
                            property.IsValueType<std::unordered_set<T>>();
                        return {};
                    });
                return checked && supported;
            }
            return FindSupportedEnum(registry, property.GetElementCppType()) != nullptr ||
                (sequence && codecs != nullptr && codecs->HasCodec(property.GetElementCppType())) ||
                (sequence && registry != nullptr && registry->FindType(property.GetElementCppType()) != nullptr);
        }

        inline archive::ArchiveResult WriteNestedSequence(archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs)
        {
            const auto* nested = property.GetElementContainerProperty();
            if (nested == nullptr || !IsAutomaticContainerSupported(property, registry, codecs))
            {
                return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
            }
            const auto size = property.TryGetSize(object);
            if (!size)
            {
                return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
            }
            return ar.WriteArrayElements(property.Name, size.Size,
                [&](archive::IArchive& entry, std::string_view key, std::size_t index) -> archive::ArchiveResult
                {
                    const auto value = property.TryReadElement(object, index);
                    if (!value)
                    {
                        return { archive::ArchiveErrorCode::InvalidOperation, {} };
                    }
                    auto element = *nested;
                    element.Name = key;
                    return WriteProperty(entry, element, element.AsObject(value.Value), registry, codecs);
                });
        }

        inline archive::ArchiveResult ReadNestedSequence(archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs)
        {
            const auto* nested = property.GetElementContainerProperty();
            if (nested == nullptr || !IsAutomaticContainerSupported(property, registry, codecs))
            {
                return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
            }
            const auto prepared = PrepareSequence(ar, property, object);
            if (!prepared)
            {
                return prepared;
            }

            return ar.ReadArrayElements(property.Name,
                [&](archive::IArchive& entry, std::string_view key, std::size_t index) -> archive::ArchiveResult
                {
                    const auto value = property.TryEditElementObject(object, index);
                    if (!value)
                    {
                        return { archive::ArchiveErrorCode::InvalidOperation, {} };
                    }
                    auto element = *nested;
                    element.Name = key;
                    return ReadProperty(entry, element, value.Object, registry, codecs);
                });
        }

        inline archive::ArchiveResult WriteMap(archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs)
        {
            const bool scalarValue = IsMapScalarType(property.GetMapValueType(), registry);
            const bool codecValue = !scalarValue && codecs != nullptr && codecs->HasCodec(property.GetMapValueType());
            const auto* valueType = registry != nullptr ? registry->FindType(property.GetMapValueType()) : nullptr;
            const auto* container = property.GetMapValueContainerProperty();

            if (!IsMapScalarType(property.GetMapKeyType(), registry) ||
                (!scalarValue && !codecValue && valueType == nullptr && container == nullptr) ||
                (container != nullptr && !IsAutomaticContainerSupported(*container, registry, codecs)))
            {
                return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
            }

            const auto size = property.TryGetSize(object);

            if (!size)
            {
                return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
            }

            std::vector<std::pair<reflection::ValueView, reflection::ValueView>> entries;
            entries.reserve(size.Size);
            const auto access = property.TryForEachMapEntry(object,
                [&entries](std::size_t, const reflection::ValueView& key, const reflection::ValueView& value)
                {
                    entries.emplace_back(key, value);
                    return true;
                });

            if (access != reflection::PropertyAccessError::None)
            {
                return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
            }

            return ar.WriteObjectArray(property.Name, entries.size(),
                [&entries, scalarValue, codecValue, valueType, container, registry, codecs](
                    archive::IArchive& entry, std::size_t index) -> archive::ArchiveResult
                {
                    const auto& [key, value] = entries[index];
                    const auto keyResult = WriteMapScalar(entry, "Key", key, registry);

                    if (!keyResult)
                    {
                        return keyResult;
                    }

                    if (scalarValue)
                    {
                        return WriteMapScalar(entry, "Value", value, registry);
                    }

                    if (codecValue)
                    {
                        return codecs->WriteValue(entry, "Value", value).value_or(
                            archive::ArchiveResult{ archive::ArchiveErrorCode::UnsupportedType, "Value" });
                    }

                    if (container != nullptr)
                    {
                        return WriteProperty(entry, *container, container->AsObject(value), registry, codecs);
                    }

                    const auto source = valueType->AsObject(value);

                    if (!source.IsValid())
                    {
                        return archive::ArchiveResult{ archive::ArchiveErrorCode::InvalidOperation, "Value" };
                    }

                    return entry.WriteObject("Value", source,
                        [valueType, registry, codecs](archive::IArchive& temporary,
                            const reflection::ObjectView& object)
                        {
                            return WithParentField("Value",
                                WriteProperties(temporary, *valueType, object, registry, codecs));
                        });
                });
        }

        inline archive::ArchiveResult ReadMap(archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs)
        {
            const bool scalarValue = IsMapScalarType(property.GetMapValueType(), registry);
            const bool codecValue = !scalarValue && codecs != nullptr && codecs->HasCodec(property.GetMapValueType());
            const auto* valueType = registry != nullptr ? registry->FindType(property.GetMapValueType()) : nullptr;
            const auto* container = property.GetMapValueContainerProperty();

            if (!IsMapScalarType(property.GetMapKeyType(), registry) ||
                (!scalarValue && !codecValue && valueType == nullptr && container == nullptr) ||
                (container != nullptr && !IsAutomaticContainerSupported(*container, registry, codecs)))
            {
                return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
            }

            if (!property.CanClear() ||
                ((scalarValue || codecValue) ? !property.CanInsertMapEntry() : !property.CanEmplaceMapValueObject()))
            {
                return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
            }

            if (property.TryClear(object) != reflection::PropertyAccessError::None)
            {
                return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
            }

            return ar.ReadObjectArray(property.Name, [&](archive::IArchive& entry, std::size_t)
                {
                    return ReadMapScalar(entry, "Key", property.GetMapKeyType(), registry,
                        [&](const reflection::ValueView& keyView) -> archive::ArchiveResult
                        {
                            if (scalarValue)
                            {
                                return ReadMapScalar(entry, "Value", property.GetMapValueType(), registry,
                                    [&](const reflection::ValueView& value)
                                    {
                                        return MapInsertionResult(property.TryInsertMapEntry(object, keyView, value));
                                    });
                            }

                            if (codecValue)
                            {
                                return codecs->ReadValue(entry, "Value", property.GetMapValueType(),
                                    [&](const reflection::ValueView& value)
                                    {
                                        return MapInsertionResult(property.TryInsertMapEntry(object, keyView, value));
                                    }).value_or(archive::ArchiveResult{
                                        archive::ArchiveErrorCode::UnsupportedType, "Value" });
                            }

                            if (container != nullptr)
                            {
                                const auto value = property.TryEmplaceMapValueObject(object, keyView);
                                const auto inserted = MapInsertionResult(value.Error);
                                return inserted ? ReadProperty(entry, *container, value.Object, registry, codecs) : inserted;
                            }

                            const auto begin = entry.BeginReadObject("Value");

                            if (!begin)
                            {
                                return begin;
                            }

                            const auto value = property.TryEmplaceMapValueObject(object, keyView);
                            auto result = MapInsertionResult(value.Error);

                            if (result)
                            {
                                if (!value || value.Object.GetCppType() != valueType->GetCppType())
                                {
                                    result = { archive::ArchiveErrorCode::InvalidOperation, "Value" };
                                }
                                else
                                {
                                    result = WithParentField("Value",
                                        ReadProperties(entry, *valueType, value.Object, registry, codecs));
                                }
                            }

                            const auto end = entry.EndObject();
                            return result ? WithParentField("Value", end) : result;
                        });
                });
        }

        inline archive::ArchiveResult WriteNestedObject(archive::IArchive& ar,
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
                [&property, nestedType, registry, codecs](archive::IArchive& temporary,
                    const reflection::ObjectView& source)
                {
                    return WithParentField(
                        property.Name, WriteProperties(temporary, *nestedType, source, registry, codecs));
                });
        }

        inline archive::ArchiveResult ReadNestedObject(archive::IArchive& ar,
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

        inline archive::ArchiveResult WriteProperty(archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs)
        {
            bool enabled = true;
            const auto policyResult = ReadSerializationPolicy(property, enabled);

            if (!policyResult)
            {
                return policyResult;
            }

            if (!enabled)
            {
                return {};
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

                    return {};
                }
            }

            if (property.GetElementContainerProperty() != nullptr &&
                (property.GetValueKind() == reflection::PropertyValueKind::Vector ||
                    property.GetValueKind() == reflection::PropertyValueKind::Array))
            {
                result = WriteNestedSequence(ar, property, object, registry, codecs);
            }
            else if (property.IsContainer() && FindSupportedEnum(registry, property.GetElementCppType()) != nullptr)
            {
                result = WriteEnumArray(ar, property, object, registry);
            }
            else if (property.GetValueKind() == reflection::PropertyValueKind::Array)
            {
                const auto scalar = WriteFixedArray<bool, std::int8_t, std::uint8_t, std::int16_t, std::uint16_t,
                    std::int32_t, std::uint32_t, std::int64_t, std::uint64_t,
                    float, double, std::string, std::wstring>(ar, property, object);

                if (scalar)
                {
                    result = *scalar;
                }
                else
                {
                    result = WriteObjectSequence(ar, property, object, registry, codecs);
                }
            }
            else if (property.GetValueKind() == reflection::PropertyValueKind::Vector ||
                property.GetValueKind() == reflection::PropertyValueKind::Set ||
                property.GetValueKind() == reflection::PropertyValueKind::UnorderedSet)
            {
                const auto scalar = WriteScalarArray<bool, std::int8_t, std::uint8_t, std::int16_t, std::uint16_t,
                    std::int32_t, std::uint32_t, std::int64_t, std::uint64_t,
                    float, double, std::string, std::wstring>(ar, property, readResult.Value);

                if (scalar)
                {
                    result = *scalar;
                }
                else if (property.GetValueKind() == reflection::PropertyValueKind::Vector)
                {
                    result = WriteObjectSequence(ar, property, object, registry, codecs);
                }
                else
                {
                    result = { archive::ArchiveErrorCode::UnsupportedType, property.Name };
                }
            }
            else if (property.IsMap())
            {
                result = WriteMap(ar, property, object, registry, codecs);
            }
            else if (property.GetValueKind() == reflection::PropertyValueKind::Enum)
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
            return {};
        }

        inline archive::ArchiveResult WriteProperties(archive::IArchive& ar,
            const reflection::TypeInfo& type, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs)
        {
            return VisitProperties(type, object, registry,
                [&](const reflection::PropertyInfo& property, const reflection::ObjectView& owner)
                {
                    return WriteProperty(ar, property, owner, registry, codecs);
                });
        }

        template<typename T>
        bool TryReadScalar(const archive::IArchive& ar, const reflection::PropertyInfo& property,
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
        archive::ArchiveResult ReadScalar(const archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object)
        {
            archive::ArchiveResult result;

            if ((TryReadScalar<Types>(ar, property, object, result) || ...))
            {
                return result;
            }

            return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
        }

        inline archive::ArchiveResult ReadProperty(archive::IArchive& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs)
        {
            bool enabled = true;
            const auto policyResult = ReadSerializationPolicy(property, enabled);

            if (!policyResult)
            {
                return policyResult;
            }

            if (!enabled)
            {
                return {};
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
                    return {};
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

                    return {};
                }
            }
            if (property.GetElementContainerProperty() != nullptr &&
                (property.GetValueKind() == reflection::PropertyValueKind::Vector ||
                    property.GetValueKind() == reflection::PropertyValueKind::Array))
            {
                result = ReadNestedSequence(ar, property, object, registry, codecs);
            }
            else if (property.IsContainer() && FindSupportedEnum(registry, property.GetElementCppType()) != nullptr)
            {
                result = ReadEnumArray(ar, property, object, registry);
            }
            else if (property.GetValueKind() == reflection::PropertyValueKind::Array)
            {
                const auto scalar = ReadFixedArray<bool, std::int8_t, std::uint8_t, std::int16_t, std::uint16_t,
                    std::int32_t, std::uint32_t, std::int64_t, std::uint64_t,
                    float, double, std::string, std::wstring>(ar, property, object);

                if (scalar)
                {
                    result = *scalar;
                }
                else
                {
                    result = ReadObjectSequence(ar, property, object, registry, codecs);
                }
            }
            else if (property.GetValueKind() == reflection::PropertyValueKind::Vector ||
                property.GetValueKind() == reflection::PropertyValueKind::Set ||
                property.GetValueKind() == reflection::PropertyValueKind::UnorderedSet)
            {
                const auto scalar = ReadScalarArray<bool, std::int8_t, std::uint8_t, std::int16_t, std::uint16_t,
                    std::int32_t, std::uint32_t, std::int64_t, std::uint64_t,
                    float, double, std::string, std::wstring>(ar, property, object);

                if (scalar)
                {
                    result = *scalar;
                }
                else if (property.GetValueKind() == reflection::PropertyValueKind::Vector)
                {
                    result = ReadObjectSequence(ar, property, object, registry, codecs);
                }
                else
                {
                    result = { archive::ArchiveErrorCode::UnsupportedType, property.Name };
                }
            }
            else if (property.IsMap())
            {
                result = ReadMap(ar, property, object, registry, codecs);
            }
            else if (property.GetValueKind() == reflection::PropertyValueKind::Enum)
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
            return {};
        }

        inline archive::ArchiveResult ReadProperties(archive::IArchive& ar,
            const reflection::TypeInfo& type, const reflection::ObjectView& object,
            const reflection::Registry* registry, const PropertyCodecs* codecs)
        {
            return VisitProperties(type, object, registry,
                [&](const reflection::PropertyInfo& property, const reflection::ObjectView& owner)
                {
                    return ReadProperty(ar, property, owner, registry, codecs);
                });
        }
    }


    [[nodiscard]] inline archive::ArchiveResult WriteObject(archive::IArchive& ar,
        std::string_view key, const reflection::TypeInfo& type, const reflection::ObjectView& object,
        const reflection::Registry* registry = nullptr, const PropertyCodecs* codecs = nullptr)
    {
        if (!object.IsValid() || object.GetCppType() != type.GetCppType())
        {
            return { archive::ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        return ar.WriteObject(key, object,
            [&type, registry, codecs](archive::IArchive& temporary, const reflection::ObjectView& source)
            {
                return detail::WriteProperties(temporary, type, source, registry, codecs);
            });
    }

    template<typename T>
        requires ((std::is_class_v<T> || std::is_union_v<T>) &&
    !std::is_const_v<T> && !std::is_volatile_v<T>&&
        std::copy_constructible<T>&& std::assignable_from<T&, T>)
        [[nodiscard]] archive::ArchiveResult ReadObject(const archive::IArchive& ar,
            std::string_view key, const reflection::TypeInfo& type, T& object,
            const reflection::Registry* registry = nullptr, const PropertyCodecs* codecs = nullptr)
    {
        if (reflection::ObjectView::From(object).GetCppType() != type.GetCppType())
        {
            return { archive::ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        return ar.ReadObject(key, object,
            [&type, registry, codecs](archive::IArchive& temporary, T& candidate)
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
            [type, registry, valueCodecs](archive::IArchive& ar,
                std::string_view key, const Map& values) -> archive::ArchiveResult
            {
                return ar.WriteObjectMap(key, values,
                    [type, registry, valueCodecs](archive::IArchive& entry,
                        const Mapped& value) -> archive::ArchiveResult
                    {
                        return detail::WriteProperties(
                            entry, *type, reflection::ObjectView::From(value), registry, valueCodecs);
                    });
            },
            [type, registry, valueCodecs](const archive::IArchive& ar,
                std::string_view key, Map& values) -> archive::ArchiveResult
            {
                return ar.ReadObjectMap(key, values,
                    [type, registry, valueCodecs](archive::IArchive& entry,
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
            [type, registry, elementCodecs](archive::IArchive& ar,
                std::string_view key, const std::vector<T>& values) -> archive::ArchiveResult
            {
                return ar.WriteObjectArray(key, values,
                    [type, registry, elementCodecs](archive::IArchive& entry,
                        const T& value) -> archive::ArchiveResult
                    {
                        return detail::WriteProperties(
                            entry, *type, reflection::ObjectView::From(value), registry, elementCodecs);
                    });
            },
            [type, registry, elementCodecs](const archive::IArchive& ar,
                std::string_view key, std::vector<T>& values) -> archive::ArchiveResult
            {
                return ar.ReadObjectArray(key, values,
                    [type, registry, elementCodecs](archive::IArchive& entry,
                        T& value) -> archive::ArchiveResult
                    {
                        return detail::ReadProperties(
                            entry, *type, reflection::ObjectView::From(value), registry, elementCodecs);
                    });
            });
    }
}
