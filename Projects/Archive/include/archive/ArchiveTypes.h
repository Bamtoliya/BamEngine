#pragma once

#include <concepts>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>

namespace archive
{
    enum class ArchiveScalarType : std::uint8_t
    {
        Bool,
        Int8,
        UInt8,
        Int16,
        UInt16,
        Int32,
        UInt32,
        Int64,
        UInt64,
        Float,
        Double,
        String
    };

    using ArchiveScalarValue = std::variant<bool, std::int8_t, std::uint8_t, std::int16_t, std::uint16_t,
        std::int32_t, std::uint32_t, std::int64_t, std::uint64_t, float, double, std::string>;

    struct ArchiveScalarMapEntry
    {
        ArchiveScalarValue Key;
        ArchiveScalarValue Value;
    };

    template<typename T>
    concept ArchiveScalar =
        std::same_as<T, bool> || std::same_as<T, std::string> || std::same_as<T, std::wstring> ||
        std::same_as<T, std::int8_t> || std::same_as<T, std::uint8_t> ||
        std::same_as<T, std::int16_t> || std::same_as<T, std::uint16_t> ||
        std::same_as<T, std::int32_t> || std::same_as<T, std::uint32_t> ||
        std::same_as<T, std::int64_t> || std::same_as<T, std::uint64_t> ||
        std::same_as<T, float> || std::same_as<T, double> || std::is_enum_v<T>;

    namespace detail
    {
        template<typename T>
        struct ArchiveMapTraits
        {
            static constexpr bool IsMap = false;
        };

        template<typename Key, typename Mapped, typename Compare, typename Allocator>
        struct ArchiveMapTraits<std::map<Key, Mapped, Compare, Allocator>>
        {
            static constexpr bool IsMap = true;
            static constexpr bool IsUnordered = false;
        };

        template<typename Key, typename Mapped, typename Hash, typename Equal, typename Allocator>
        struct ArchiveMapTraits<std::unordered_map<Key, Mapped, Hash, Equal, Allocator>>
        {
            static constexpr bool IsMap = true;
            static constexpr bool IsUnordered = true;
        };
    }

    template<typename T>
    concept ArchiveMap = detail::ArchiveMapTraits<T>::IsMap &&
        ArchiveScalar<typename T::key_type> && ArchiveScalar<typename T::mapped_type>;

    template<typename T>
    concept ArchiveObjectMap = detail::ArchiveMapTraits<T>::IsMap &&
        ArchiveScalar<typename T::key_type> &&
        (std::is_class_v<typename T::mapped_type> || std::is_union_v<typename T::mapped_type>) &&
        !std::is_const_v<typename T::mapped_type> && !std::is_volatile_v<typename T::mapped_type>;
}