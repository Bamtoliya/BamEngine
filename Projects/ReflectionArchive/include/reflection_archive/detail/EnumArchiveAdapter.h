#pragma once

#include <archive/ArchiveInterface.h>
#include <reflection/Registry.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <typeindex>
#include <variant>
#include <vector>

namespace archive_reflection::detail
{
    inline const reflection::EnumInfo* FindSupportedEnum(const reflection::Registry* registry, std::type_index type)
    {
        const auto* enumeration = registry != nullptr ? registry->FindEnum(type) : nullptr;
        return enumeration != nullptr && enumeration->IsScoped && enumeration->CanReadValue() &&
            enumeration->CanVisitValue() ? enumeration : nullptr;
    }

    inline archive::ArchiveResult WriteEnumValue(archive::IArchive& ar, std::string_view field,
        const reflection::ValueView& value, const reflection::Registry* registry)
    {
        const auto* enumeration = FindSupportedEnum(registry, value.GetCppType());

        if (enumeration == nullptr)
        {
            return { archive::ArchiveErrorCode::UnsupportedType, std::string{ field } };
        }

        const auto number = enumeration->ReadValue(value);

        if (!number)
        {
            return { archive::ArchiveErrorCode::InvalidOperation, std::string{ field } };
        }

        return std::visit([&](auto stored) { return ar.Write(field, stored); }, *number);
    }

    template<typename Storage, typename Visitor>
    archive::ArchiveResult ReadEnumValue(const archive::IArchive& ar,
        std::string_view field, const reflection::EnumInfo& enumeration, Visitor&& visitor)
    {
        Storage stored{};
        const auto read = ar.Read(field, stored);

        if (!read)
        {
            return read;
        }

        archive::ArchiveResult result;
        const bool converted = enumeration.TryVisitValue(reflection::EnumValue{ stored },
            [&](const reflection::ValueView& value) { result = std::invoke(visitor, value); });

        if (!converted)
        {
            return { archive::ArchiveErrorCode::InvalidValue, std::string{ field } };
        }

        return result;
    }

    template<typename Visitor>
    archive::ArchiveResult ReadEnumValue(const archive::IArchive& ar,
        std::string_view field, const reflection::EnumInfo& enumeration, Visitor&& visitor)
    {
        return enumeration.IsUnderlyingSigned()
            ? ReadEnumValue<std::int64_t>(ar, field, enumeration, visitor)
            : ReadEnumValue<std::uint64_t>(ar, field, enumeration, visitor);
    }

    template<typename Storage>
    archive::ArchiveResult WriteEnumArray(archive::IArchive& ar, const reflection::PropertyInfo& property,
        const reflection::ObjectView& object, const reflection::EnumInfo& enumeration)
    {
        std::vector<Storage> values;
        archive::ArchiveResult result;
        const auto access = property.TryForEachElement(object,
            [&](std::size_t index, const reflection::ValueView& element)
            {
                const auto number = enumeration.ReadValue(element);
                const auto* stored = number ? std::get_if<Storage>(&*number) : nullptr;

                if (stored == nullptr)
                {
                    result = { archive::ArchiveErrorCode::InvalidOperation,
                        property.Name + "[" + std::to_string(index) + "]" };
                    return false;
                }

                values.push_back(*stored);
                return true;
            });

        if (access != reflection::PropertyAccessError::None)
        {
            return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
        }

        return result ? ar.WriteArray(property.Name, values) : result;
    }

    inline archive::ArchiveResult WriteEnumArray(archive::IArchive& ar, const reflection::PropertyInfo& property,
        const reflection::ObjectView& object, const reflection::Registry* registry)
    {
        const auto* enumeration = FindSupportedEnum(registry, property.GetElementCppType());

        if (enumeration == nullptr)
        {
            return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
        }

        return enumeration->IsUnderlyingSigned()
            ? WriteEnumArray<std::int64_t>(ar, property, object, *enumeration)
            : WriteEnumArray<std::uint64_t>(ar, property, object, *enumeration);
    }

    template<typename Storage>
    archive::ArchiveResult ReadEnumArray(const archive::IArchive& ar, const reflection::PropertyInfo& property,
        const reflection::ObjectView& object, const reflection::EnumInfo& enumeration)
    {
        using Error = reflection::PropertyAccessError;
        const bool fixed = property.GetValueKind() == reflection::PropertyValueKind::Array;

        if (fixed ? !property.CanWriteElement() :
            (!property.CanClear() || (property.IsSet() ? !property.CanInsertSetElement() : !property.CanAppend())))
        {
            return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
        }

        std::vector<Storage> values;
        const auto read = ar.ReadArray(property.Name, values);

        if (!read)
        {
            return read;
        }

        if (fixed)
        {
            const auto size = property.TryGetSize(object);

            if (!size || size.Size != values.size())
            {
                return { size ? archive::ArchiveErrorCode::InvalidValue : archive::ArchiveErrorCode::InvalidOperation,
                    property.Name };
            }
        }
        else if (property.TryClear(object) != Error::None)
        {
            return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
        }

        for (std::size_t index = 0; index < values.size(); ++index)
        {
            Error error = Error::None;
            const bool converted = enumeration.TryVisitValue(reflection::EnumValue{ values[index] },
                [&](const reflection::ValueView& value)
                {
                    if (fixed)
                    {
                        error = property.TryWriteElement(object, index, value);
                    }
                    else if (property.IsSet())
                    {
                        error = property.TryInsertSetElement(object, value);
                    }
                    else
                    {
                        error = property.TryAppend(object, value);
                    }
                });

            if (!converted || error != Error::None)
            {
                const auto code = !converted || error == Error::ElementAlreadyExists || error == Error::SizeOutOfRange
                    ? archive::ArchiveErrorCode::InvalidValue : archive::ArchiveErrorCode::InvalidOperation;
                return { code, property.Name + "[" + std::to_string(index) + "]" };
            }
        }

        return {};
    }

    inline archive::ArchiveResult ReadEnumArray(const archive::IArchive& ar, const reflection::PropertyInfo& property,
        const reflection::ObjectView& object, const reflection::Registry* registry)
    {
        const auto* enumeration = FindSupportedEnum(registry, property.GetElementCppType());

        if (enumeration == nullptr)
        {
            return { archive::ArchiveErrorCode::UnsupportedType, property.Name };
        }

        return enumeration->IsUnderlyingSigned()
            ? ReadEnumArray<std::int64_t>(ar, property, object, *enumeration)
            : ReadEnumArray<std::uint64_t>(ar, property, object, *enumeration);
    }
}
