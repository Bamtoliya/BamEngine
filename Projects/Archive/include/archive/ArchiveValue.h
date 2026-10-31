#pragma once

#include <glaze/json/generic.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace archive::detail
{
    struct ArchiveValue
    {
        using array_t = std::vector<ArchiveValue>;
        using object_t = std::map<std::string, ArchiveValue, std::less<>>;
        using bytes_t = std::vector<std::uint8_t>;

        // 기존 generic_u64의 앞 8개 타입 순서는 BEVE 호환성을 위해 유지합니다.
        using storage_t = std::variant<std::nullptr_t, std::uint64_t, std::int64_t, double,
            std::string, bool, array_t, object_t, bytes_t>;

        storage_t data = nullptr;

        template<typename T>
        T* get_if() noexcept { return std::get_if<T>(&data); }

        template<typename T>
        const T* get_if() const noexcept { return std::get_if<T>(&data); }

        array_t& get_array() { return std::get<array_t>(data); }
        const array_t& get_array() const { return std::get<array_t>(data); }

        object_t& get_object() { return std::get<object_t>(data); }
        const object_t& get_object() const { return std::get<object_t>(data); }

        bool is_object() const noexcept { return get_if<object_t>() != nullptr; }
        bool is_array() const noexcept { return get_if<array_t>() != nullptr || get_if<bytes_t>() != nullptr; }

        bool is_number() const noexcept
        {
            return get_if<std::uint64_t>() || get_if<std::int64_t>() || get_if<double>();
        }
    };
}

template<>
struct glz::meta<archive::detail::ArchiveValue>
{
    using T = archive::detail::ArchiveValue;
    static constexpr auto value = &T::data;
};