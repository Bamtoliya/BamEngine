#pragma once

#include <reflection/Metadata.h>
#include <reflection/ValueView.h>
#include <reflection/EnumValue.h>

#include <optional>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <type_traits>
#include <typeindex>
#include <utility>

namespace reflection
{
    struct EnumEntry
    {
        std::string Name;
        EnumValue Value;
    };

    struct EnumInfo
    {
        std::string QualifiedName;
        std::string UnderlyingType;
        bool IsScoped = false;

        std::vector<EnumEntry> Entries;
        reflection::Metadata Metadata;

        template<typename T>
            requires std::is_enum_v<T>
        [[nodiscard]]
        static EnumInfo For(std::string qualifiedName)
        {
            using Type = std::remove_cv_t<T>;
            using Underlying = std::underlying_type_t<Type>;

            static_assert(sizeof(Underlying) <= sizeof(std::uint64_t), "Enum values wider than 64 bits are not supported");

            EnumInfo info;
            info.QualifiedName = std::move(qualifiedName);
            info.m_cppType = std::type_index(typeid(Type));
            info.IsScoped = !std::is_convertible_v<Type, Underlying>;
            info.m_readValue = +[](const ValueView& value) -> EnumValue
                {
                    const auto number = static_cast<Underlying>(*value.Get<Type>());

                    if constexpr (std::is_signed_v<Underlying>)
                    {
                        return EnumValue{ static_cast<std::int64_t>(number) };
                    }
                    else
                    {
                        return EnumValue{ static_cast<std::uint64_t>(number) };
                    }
                };

            return info;
        }

        [[nodiscard]]
        std::type_index GetCppType() const noexcept
        {
            return m_cppType;
        }

        [[nodiscard]]
        bool CanReadValue() const noexcept
        {
            return m_readValue != nullptr;
        }

        [[nodiscard]]
        std::optional<EnumValue> ReadValue(const ValueView& value) const
        {
            if (!value.IsValid() || m_readValue == nullptr || m_cppType != value.GetCppType())
            {
                return std::nullopt;
            }

            return m_readValue(value);
        }

        [[nodiscard]]
        const EnumEntry* FindEntry(std::string_view name) const
        {
            for (const EnumEntry& entry : Entries)
            {
                if (entry.Name == name)
                {
                    return &entry;
                }
            }

            return nullptr;
        }

        [[nodiscard]]
        const EnumEntry* FindEntryByValue(const EnumValue& value) const
        {
            for (const EnumEntry& entry : Entries)
            {
                const bool matches = std::visit(
                    [](auto registeredValue, auto requestedValue)
                    {
                        return std::cmp_equal(registeredValue, requestedValue);
                    },
                    entry.Value, value);

                if (matches)
                {
                    return &entry;
                }
            }

            return nullptr;
        }

    private:
        std::type_index m_cppType{ typeid(void) };
        EnumValue(*m_readValue)(const ValueView&) = nullptr;
    };
}