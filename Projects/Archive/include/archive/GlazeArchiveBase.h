#pragma once

#include <archive/ArchiveResult.h>
#include <glaze/json/generic.hpp>
#include <archive/Unicode.h>

#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <array>
#include <filesystem>
#include <fstream>
#include <cstddef>
#include <vector>
#include <optional>
#include <functional>
#include <type_traits>
#include <ranges>
#include <unordered_set>
#include <set>
#include <map>
#include <unordered_map>

namespace archive
{
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

    class GlazeArchiveBase
    {
    public:
        virtual ~GlazeArchiveBase() = default;

        [[nodiscard]] std::size_t GetObjectDepth() const noexcept
        {
            return m_ObjectPath.size();
        }

        [[nodiscard]] ArchiveResult HasField(std::string_view key, bool& exists) const
        {
            const auto* object = FindObject(m_Root, m_ObjectPath);

            if (!object)
            {
                return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
            }

            exists = object->find(key) != object->end();
            return {};
        }

        [[nodiscard]] ArchiveResult GetFieldNames(std::vector<std::string>& names) const
        {
            const auto* object = FindObject(m_Root, m_ObjectPath);

            if (object == nullptr)
            {
                return { ArchiveErrorCode::InvalidOperation, {} };
            }

            std::vector<std::string> candidate;
            candidate.reserve(object->size());

            for (const auto& entry : *object)
            {
                candidate.push_back(entry.first);
            }

            names = std::move(candidate);
            return {};
        }

        [[nodiscard]] ArchiveResult BeginWriteObject(std::string_view key)
        {
            auto* object = FindObject(m_Root, m_ObjectPath);

            if (!object)
            {
                return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
            }

            auto it = object->find(key);

            if (it == object->end())
            {
                Value candidate;
                candidate.data = Value::object_t{};
                it = object->emplace(std::string{ key }, std::move(candidate)).first;
            }

            if (!it->second.is_object())
            {
                return InvalidValue(key);
            }

            m_ObjectPath.push_back({ std::string{ key }, std::nullopt });
            return {};
        }

        [[nodiscard]] ArchiveResult BeginReadObject(std::string_view key)
        {
            const auto* object = FindObject(m_Root, m_ObjectPath);

            if (!object)
            {
                return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
            }

            const auto it = object->find(key);

            if (it == object->end())
            {
                return { ArchiveErrorCode::MissingField, std::string{ key } };
            }

            if (!it->second.is_object())
            {
                return InvalidValue(key);
            }

            m_ObjectPath.push_back({ std::string{ key }, std::nullopt });
            return {};
        }

        [[nodiscard]] ArchiveResult EndObject()
        {
            if (m_ObjectPath.empty())
            {
                return { ArchiveErrorCode::InvalidOperation, {} };
            }

            m_ObjectPath.pop_back();
            return {};
        }

        [[nodiscard]] ArchiveResult GetArraySize(std::string_view key, std::size_t& size) const
        {
            const auto* object = FindObject(m_Root, m_ObjectPath);

            if (!object)
            {
                return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
            }

            const auto it = object->find(key);

            if (it == object->end())
            {
                return { ArchiveErrorCode::MissingField, std::string{ key } };
            }

            const auto* elements = it->second.get_if<Value::array_t>();

            if (!elements)
            {
                return InvalidValue(key);
            }

            size = elements->size();
            return {};
        }

        [[nodiscard]] ArchiveResult BeginWriteObjectElement(std::string_view key)
        {
            auto* object = FindObject(m_Root, m_ObjectPath);

            if (!object)
            {
                return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
            }

            auto it = object->find(key);

            if (it == object->end())
            {
                Value candidate;
                candidate.data = Value::array_t{};
                it = object->emplace(std::string{ key }, std::move(candidate)).first;
            }

            auto* elements = it->second.get_if<Value::array_t>();

            if (!elements)
            {
                return InvalidValue(key);
            }

            Value element;
            element.data = Value::object_t{};

            const std::size_t index = elements->size();
            elements->push_back(std::move(element));
            m_ObjectPath.push_back({ std::string{ key }, index });
            return {};
        }

        [[nodiscard]] ArchiveResult BeginReadObjectElement(std::string_view key, std::size_t index)
        {
            const auto* object = FindObject(m_Root, m_ObjectPath);

            if (!object)
            {
                return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
            }

            const auto it = object->find(key);

            if (it == object->end())
            {
                return { ArchiveErrorCode::MissingField, std::string{ key } };
            }

            const auto* elements = it->second.get_if<Value::array_t>();

            if (!elements)
            {
                return InvalidValue(key);
            }

            if (index >= elements->size())
            {
                return { ArchiveErrorCode::IndexOutOfRange, std::string{ key } };
            }

            if (!(*elements)[index].is_object())
            {
                return InvalidValue(key);
            }

            m_ObjectPath.push_back({ std::string{ key }, index });
            return {};
        }

        [[nodiscard]] ArchiveResult SaveToFile(const std::filesystem::path& path) const
        {
            if (GetObjectDepth() != 0)
            {
                return { ArchiveErrorCode::InvalidOperation, {} };
            }

            std::string document;
            const auto result = EncodeDocument(document);

            if (!result)
            {
                return result;
            }

            if (std::cmp_greater(document.size(), (std::numeric_limits<std::streamsize>::max)()))
            {
                return { ArchiveErrorCode::FileWriteFailed, {} };
            }

            std::ofstream file{ path, std::ios::binary | std::ios::trunc };

            if (!file)
            {
                return { ArchiveErrorCode::FileOpenFailed, {} };
            }

            file.write(document.data(), static_cast<std::streamsize>(document.size()));
            file.close();

            if (!file)
            {
                return { ArchiveErrorCode::FileWriteFailed, {} };
            }

            return {};
        }

        [[nodiscard]] ArchiveResult LoadFromFile(const std::filesystem::path& path)
        {
            if (GetObjectDepth() != 0)
            {
                return { ArchiveErrorCode::InvalidOperation, {} };
            }

            std::ifstream file{ path, std::ios::binary };

            if (!file)
            {
                return { ArchiveErrorCode::FileOpenFailed, {} };
            }

            std::string document;
            std::array<char, 4096> buffer{};

            while (file.read(buffer.data(), static_cast<std::streamsize>(buffer.size())))
            {
                document.append(buffer.data(), buffer.size());
            }

            if (file.bad() || !file.eof())
            {
                return { ArchiveErrorCode::FileReadFailed, {} };
            }

            document.append(buffer.data(), static_cast<std::size_t>(file.gcount()));
            return DecodeDocument(document);
        }

        template<ArchiveScalar T>
        [[nodiscard]] ArchiveResult Write(std::string_view key, const T& value)
        {
            auto* object = FindObject(m_Root, m_ObjectPath);

            if (!object)
            {
                return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
            }

            Value candidate;
            const auto result = EncodeScalar(key, value, candidate);

            if (!result)
            {
                return result;
            }

            object->insert_or_assign(std::string{ key }, std::move(candidate));
            return {};
        }

        template<typename Range>
            requires (std::ranges::input_range<const Range>&& ArchiveScalar<std::ranges::range_value_t<const Range>>)
        [[nodiscard]] ArchiveResult WriteArray(std::string_view key, const Range& values)
        {
            using T = std::ranges::range_value_t<const Range>;

            auto* object = FindObject(m_Root, m_ObjectPath);

            if (!object)
            {
                return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
            }

            Value candidate;
            candidate.data = Value::array_t{};

            auto& elements = candidate.get_array();

            if constexpr (std::ranges::sized_range<const Range>)
            {
                elements.reserve(std::ranges::size(values));
            }

            for (const auto& value : values)
            {
                Value element;
                const auto result = EncodeScalar<T>(key, value, element);

                if (!result)
                {
                    return result;
                }

                elements.push_back(std::move(element));
            }

            object->insert_or_assign(std::string{ key }, std::move(candidate));
            return {};
        }

        template<ArchiveScalar T>
        [[nodiscard]] ArchiveResult Read(std::string_view key, T& value) const
        {
            const auto* object = FindObject(m_Root, m_ObjectPath);

            if (!object)
            {
                return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
            }

            const auto it = object->find(key);

            if (it == object->end())
            {
                return { ArchiveErrorCode::MissingField, std::string{ key } };
            }

            return DecodeScalar(key, it->second, value);
        }
        template<ArchiveScalar T>
        [[nodiscard]] ArchiveResult ReadArray(std::string_view key, std::vector<T>& values) const
        {
            const auto* object = FindObject(m_Root, m_ObjectPath);

            if (!object)
            {
                return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
            }

            const auto it = object->find(key);

            if (it == object->end())
            {
                return { ArchiveErrorCode::MissingField, std::string{ key } };
            }

            const auto* elements = it->second.get_if<Value::array_t>();

            if (!elements)
            {
                return InvalidValue(key);
            }

            std::vector<T> candidate;
            candidate.reserve(elements->size());

            for (const auto& source : *elements)
            {
                T element{};
                const auto result = DecodeScalar(key, source, element);

                if (!result)
                {
                    return result;
                }

                candidate.push_back(std::move(element));
            }

            values.swap(candidate);
            return {};
        }

        template<ArchiveScalar T, typename Hash, typename Equal, typename Allocator>
        [[nodiscard]] ArchiveResult ReadArray(std::string_view key, std::unordered_set<T, Hash, Equal, Allocator>& values) const
        {
            const auto* object = FindObject(m_Root, m_ObjectPath);

            if (!object)
            {
                return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
            }

            const auto it = object->find(key);

            if (it == object->end())
            {
                return { ArchiveErrorCode::MissingField, std::string{ key } };
            }

            const auto* elements = it->second.get_if<Value::array_t>();

            if (!elements)
            {
                return InvalidValue(key);
            }

            using Set = std::unordered_set<T, Hash, Equal, Allocator>;
            Set candidate{ 0, values.hash_function(), values.key_eq(), values.get_allocator() };
            candidate.reserve(elements->size());

            for (const auto& source : *elements)
            {
                T element{};
                const auto result = DecodeScalar(key, source, element);

                if (!result)
                {
                    return result;
                }

                if (!candidate.insert(std::move(element)).second)
                {
                    return InvalidValue(key);
                }
            }

            values.swap(candidate);
            return {};
        }

        template<ArchiveScalar T, typename Compare, typename Allocator>
        [[nodiscard]] ArchiveResult ReadArray(std::string_view key, std::set<T, Compare, Allocator>& values) const
        {
            const auto* object = FindObject(m_Root, m_ObjectPath);

            if (!object)
            {
                return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
            }

            const auto found = object->find(key);

            if (found == object->end())
            {
                return { ArchiveErrorCode::MissingField, std::string{ key } };
            }

            const auto* elements = found->second.get_if<Value::array_t>();

            if (!elements)
            {
                return InvalidValue(key);
            }

            using Set = std::set<T, Compare, Allocator>;
            Set candidate{ values.key_comp(), values.get_allocator() };

            for (const auto& source : *elements)
            {
                T element{};
                const auto result = DecodeScalar(key, source, element);

                if (!result)
                {
                    return result;
                }

                if (!candidate.insert(std::move(element)).second)
                {
                    return InvalidValue(key);
                }
            }

            values.swap(candidate);
            return {};
        }

        template<ArchiveScalar T, std::size_t N>
        [[nodiscard]] ArchiveResult ReadArray(std::string_view key, std::array<T, N>& values) const
        {
            const auto* object = FindObject(m_Root, m_ObjectPath);

            if (!object)
            {
                return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
            }

            const auto it = object->find(key);

            if (it == object->end())
            {
                return { ArchiveErrorCode::MissingField, std::string{ key } };
            }

            const auto* elements = it->second.get_if<Value::array_t>();

            if (!elements || elements->size() != N)
            {
                return InvalidValue(key);
            }

            std::array<T, N> candidate{};

            for (std::size_t index = 0; index < N; ++index)
            {
                const auto result = DecodeScalar(key, (*elements)[index], candidate[index]);

                if (!result)
                {
                    return result;
                }
            }

            values = std::move(candidate);
            return {};
        }

        template<ArchiveMap Map>
        [[nodiscard]] ArchiveResult WriteMap(std::string_view key, const Map& values)
        {
            auto* object = FindObject(m_Root, m_ObjectPath);

            if (!object)
            {
                return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
            }

            Value candidate;
            candidate.data = Value::array_t{};
            auto& elements = candidate.get_array();
            elements.reserve(values.size());
            std::size_t index = 0;

            for (const auto& [mapKey, mapValue] : values)
            {
                const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
                Value encodedKey;
                Value encodedValue;
                const auto keyResult = EncodeScalar(path + ".Key", mapKey, encodedKey);

                if (!keyResult)
                {
                    return keyResult;
                }

                const auto valueResult = EncodeScalar(path + ".Value", mapValue, encodedValue);

                if (!valueResult)
                {
                    return valueResult;
                }

                Value entry;
                entry.data = Value::object_t{};
                entry.get_object().emplace("Key", std::move(encodedKey));
                entry.get_object().emplace("Value", std::move(encodedValue));
                elements.push_back(std::move(entry));
                ++index;
            }

            object->insert_or_assign(std::string{ key }, std::move(candidate));
            return {};
        }

        template<ArchiveMap Map>
        [[nodiscard]] ArchiveResult ReadMap(std::string_view key, Map& values) const
        {
            const auto* object = FindObject(m_Root, m_ObjectPath);

            if (!object)
            {
                return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
            }

            const auto found = object->find(key);

            if (found == object->end())
            {
                return { ArchiveErrorCode::MissingField, std::string{ key } };
            }

            const auto* elements = found->second.get_if<Value::array_t>();

            if (!elements)
            {
                return InvalidValue(key);
            }

            Map candidate = [&values]()
                {
                    if constexpr (detail::ArchiveMapTraits<Map>::IsUnordered)
                    {
                        return Map{ 0, values.hash_function(), values.key_eq(), values.get_allocator() };
                    }
                    else
                    {
                        return Map{ values.key_comp(), values.get_allocator() };
                    }
                }();

            if constexpr (detail::ArchiveMapTraits<Map>::IsUnordered)
            {
                candidate.max_load_factor(values.max_load_factor());
                candidate.reserve(elements->size());
            }

            for (std::size_t index = 0; index < elements->size(); ++index)
            {
                const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
                const auto* entry = (*elements)[index].get_if<Value::object_t>();

                if (!entry)
                {
                    return InvalidValue(path);
                }

                const auto keyField = entry->find("Key");
                const auto valueField = entry->find("Value");

                if (keyField == entry->end())
                {
                    return { ArchiveErrorCode::MissingField, path + ".Key" };
                }

                if (valueField == entry->end())
                {
                    return { ArchiveErrorCode::MissingField, path + ".Value" };
                }

                typename Map::key_type decodedKey{};
                typename Map::mapped_type decodedValue{};
                const auto keyResult = DecodeScalar(path + ".Key", keyField->second, decodedKey);

                if (!keyResult)
                {
                    return keyResult;
                }

                const auto valueResult = DecodeScalar(path + ".Value", valueField->second, decodedValue);

                if (!valueResult)
                {
                    return valueResult;
                }

                if (!candidate.emplace(std::move(decodedKey), std::move(decodedValue)).second)
                {
                    return InvalidValue(path + ".Key");
                }
            }

            values.swap(candidate);
            return {};
        }

        template<ArchiveObjectMap Map, typename Writer>
        [[nodiscard]] ArchiveResult WriteObjectMap(std::string_view key, const Map& values, Writer&& writeObject);

        template<ArchiveObjectMap Map, typename Reader>
            requires (std::default_initializable<typename Map::mapped_type>&&
        std::move_constructible<typename Map::mapped_type>)
            [[nodiscard]] ArchiveResult ReadObjectMap(std::string_view key, Map& values, Reader&& readObject) const;

        template<typename T, typename Writer>
        [[nodiscard]] ArchiveResult WriteObjectArray(std::string_view key, const std::vector<T>& values, Writer&& writeObject);

        template<typename T, typename Reader>
        [[nodiscard]] ArchiveResult ReadObjectArray(std::string_view key, std::vector<T>& values, Reader&& readObject) const;

        template<typename T, typename Writer>
        [[nodiscard]] ArchiveResult WriteObject(std::string_view key, const T& value, Writer&& writeObject);

        template<typename T, typename Reader>
            requires (std::copy_constructible<T>&& std::assignable_from<T&, T>)
        [[nodiscard]] ArchiveResult ReadObject(std::string_view key, T& value, Reader&& readObject) const;

    protected:
        using Value = glz::generic_u64;

        virtual ArchiveResult EncodeDocument(std::string& document) const = 0;
        virtual ArchiveResult DecodeDocument(std::string_view document) = 0;

        GlazeArchiveBase()
        {
            m_Root.data = Value::object_t{};
        }

        Value m_Root;

    private:
        class ObjectArchive;
        struct ObjectPathEntry
        {
            std::string Key;
            std::optional<std::size_t> ElementIndex;
        };

        static ArchiveResult InvalidValue(std::string_view key)
        {
            return { ArchiveErrorCode::InvalidValue, std::string{ key } };
        }
        template<ArchiveScalar T>
        static ArchiveResult EncodeScalar(std::string_view key, const T& value, Value& destination)
        {
            if constexpr (std::is_enum_v<T>)
            {
                using Underlying = std::underlying_type_t<T>;
                using Storage = std::conditional_t<std::is_signed_v<Underlying>, std::int64_t, std::uint64_t>;

                static_assert(sizeof(Underlying) <= sizeof(Storage), "Enum storage exceeds 64 bits.");

                destination.data = static_cast<Storage>(static_cast<Underlying>(value));
            }
            else if constexpr (std::same_as<T, std::wstring>)
            {
                std::string utf8;

                if (!detail::WideToUtf8(value, utf8))
                {
                    return InvalidValue(key);
                }

                destination.data = std::move(utf8);
            }
            else if constexpr (std::same_as<T, bool> || std::same_as<T, std::string>)
            {
                destination.data = value;
            }
            else if constexpr (std::signed_integral<T>)
            {
                destination.data = static_cast<std::int64_t>(value);
            }
            else if constexpr (std::unsigned_integral<T>)
            {
                destination.data = static_cast<std::uint64_t>(value);
            }
            else
            {
                if (!std::isfinite(value))
                {
                    return InvalidValue(key);
                }

                destination.data = static_cast<double>(value);
            }

            return {};
        }
        template<ArchiveScalar T>
        static ArchiveResult DecodeScalar(std::string_view key, const Value& source, T& value)
        {
            T candidate{};

            if constexpr (std::is_enum_v<T>)
            {
                using Underlying = std::underlying_type_t<T>;
                using Storage = std::conditional_t<std::is_signed_v<Underlying>, std::int64_t, std::uint64_t>;

                static_assert(sizeof(Underlying) <= sizeof(Storage), "Enum storage exceeds 64 bits.");

                Storage stored{};
                const auto result = DecodeScalar(key, source, stored);

                if (!result)
                {
                    return result;
                }

                const Storage minimum = static_cast<Storage>((std::numeric_limits<Underlying>::min)());
                const Storage maximum = static_cast<Storage>((std::numeric_limits<Underlying>::max)());

                if (stored < minimum || stored > maximum)
                {
                    return InvalidValue(key);
                }

                candidate = static_cast<T>(static_cast<Underlying>(stored));
            }
            else if constexpr (std::same_as<T, std::wstring>)
            {
                const auto* stored = source.get_if<std::string>();

                if (!stored || !detail::Utf8ToWide(*stored, candidate))
                {
                    return InvalidValue(key);
                }
            }
            else if constexpr (std::same_as<T, bool> || std::same_as<T, std::string>)
            {
                const auto* stored = source.get_if<T>();

                if (!stored)
                {
                    return InvalidValue(key);
                }

                candidate = *stored;
            }
            else if constexpr (std::integral<T>)
            {
                if (const auto* signedValue = source.get_if<std::int64_t>())
                {
                    if (!std::in_range<T>(*signedValue))
                    {
                        return InvalidValue(key);
                    }

                    candidate = static_cast<T>(*signedValue);
                }
                else if (const auto* unsignedValue = source.get_if<std::uint64_t>())
                {
                    if (!std::in_range<T>(*unsignedValue))
                    {
                        return InvalidValue(key);
                    }

                    candidate = static_cast<T>(*unsignedValue);
                }
                else
                {
                    return InvalidValue(key);
                }
            }
            else
            {
                double number = 0.0;

                if (const auto* floatingValue = source.get_if<double>())
                {
                    number = *floatingValue;
                }
                else if (const auto* signedValue = source.get_if<std::int64_t>())
                {
                    number = static_cast<double>(*signedValue);
                }
                else if (const auto* unsignedValue = source.get_if<std::uint64_t>())
                {
                    number = static_cast<double>(*unsignedValue);
                }
                else
                {
                    return InvalidValue(key);
                }

                const double limit = static_cast<double>((std::numeric_limits<T>::max)());

                if (!std::isfinite(number) || number < -limit || number > limit)
                {
                    return InvalidValue(key);
                }

                candidate = static_cast<T>(number);
            }

            value = std::move(candidate);
            return {};
        }
        template<typename Node>
        static auto FindObject(Node& root, const std::vector<ObjectPathEntry>& path)
        {
            using ObjectPointer = decltype(root.template get_if<Value::object_t>());
            auto* current = &root;

            for (const auto& entry : path)
            {
                auto* object = current->template get_if<Value::object_t>();

                if (!object)
                {
                    return ObjectPointer{};
                }

                const auto it = object->find(entry.Key);

                if (it == object->end())
                {
                    return ObjectPointer{};
                }

                current = &it->second;

                if (entry.ElementIndex)
                {
                    auto* elements = current->template get_if<Value::array_t>();

                    if (!elements || *entry.ElementIndex >= elements->size())
                    {
                        return ObjectPointer{};
                    }

                    current = &(*elements)[*entry.ElementIndex];
                }
            }

            return current->template get_if<Value::object_t>();
        }

        std::vector<ObjectPathEntry> m_ObjectPath;
    };

    class GlazeArchiveBase::ObjectArchive final : public GlazeArchiveBase
    {
    public:
        ObjectArchive() = default;

    private:
        ArchiveResult EncodeDocument(std::string&) const override
        {
            return { ArchiveErrorCode::InvalidOperation, {} };
        }

        ArchiveResult DecodeDocument(std::string_view) override
        {
            return { ArchiveErrorCode::InvalidOperation, {} };
        }
    };

    template<ArchiveObjectMap Map, typename Writer>
    ArchiveResult GlazeArchiveBase::WriteObjectMap(std::string_view key, const Map& values, Writer&& writeObject)
    {
        auto* object = FindObject(m_Root, m_ObjectPath);
        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        Value candidate;
        candidate.data = Value::array_t{};
        auto& elements = candidate.get_array();
        elements.reserve(values.size());
        std::size_t index = 0;

        for (const auto& [mapKey, mapValue] : values)
        {
            const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
            Value encodedKey;
            const auto keyResult = EncodeScalar(path + ".Key", mapKey, encodedKey);

            if (!keyResult)
            {
                return keyResult;
            }

            ObjectArchive temporary;
            auto result = std::invoke(writeObject, temporary, mapValue);

            if (!result)
            {
                result.Field = path + ".Value" + (result.Field.empty() ? "" : "." + result.Field);
                return result;
            }

            if (temporary.GetObjectDepth() != 0)
            {
                return { ArchiveErrorCode::InvalidOperation, path + ".Value" };
            }

            Value entry;
            entry.data = Value::object_t{};
            entry.get_object().emplace("Key", std::move(encodedKey));
            entry.get_object().emplace("Value", std::move(temporary.m_Root));
            elements.push_back(std::move(entry));
            ++index;
        }

        object->insert_or_assign(std::string{ key }, std::move(candidate));
        return {};
    }

    template<ArchiveObjectMap Map, typename Reader>
        requires (std::default_initializable<typename Map::mapped_type>&&
    std::move_constructible<typename Map::mapped_type>)
        ArchiveResult GlazeArchiveBase::ReadObjectMap(std::string_view key, Map& values, Reader&& readObject) const
    {
        const auto* object = FindObject(m_Root, m_ObjectPath);
        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        const auto found = object->find(key);
        if (found == object->end())
        {
            return { ArchiveErrorCode::MissingField, std::string{ key } };
        }

        const auto* elements = found->second.get_if<Value::array_t>();
        if (!elements)
        {
            return InvalidValue(key);
        }

        Map candidate = [&values]()
            {
                if constexpr (detail::ArchiveMapTraits<Map>::IsUnordered)
                {
                    return Map{ 0, values.hash_function(), values.key_eq(), values.get_allocator() };
                }
                else
                {
                    return Map{ values.key_comp(), values.get_allocator() };
                }
            }();

        if (elements->size() > candidate.max_size())
        {
            return InvalidValue(key);
        }

        if constexpr (detail::ArchiveMapTraits<Map>::IsUnordered)
        {
            candidate.max_load_factor(values.max_load_factor());
            candidate.reserve(elements->size());
        }

        for (std::size_t index = 0; index < elements->size(); ++index)
        {
            const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
            const auto* entry = (*elements)[index].get_if<Value::object_t>();

            if (!entry)
            {
                return InvalidValue(path);
            }

            const auto keyField = entry->find("Key");
            const auto valueField = entry->find("Value");

            if (keyField == entry->end())
            {
                return { ArchiveErrorCode::MissingField, path + ".Key" };
            }

            if (valueField == entry->end())
            {
                return { ArchiveErrorCode::MissingField, path + ".Value" };
            }

            if (!valueField->second.is_object())
            {
                return InvalidValue(path + ".Value");
            }

            typename Map::key_type decodedKey{};
            const auto keyResult = DecodeScalar(path + ".Key", keyField->second, decodedKey);

            if (!keyResult)
            {
                return keyResult;
            }

            ObjectArchive temporary;
            temporary.m_Root = valueField->second;
            typename Map::mapped_type decodedValue{};
            auto result = std::invoke(readObject, temporary, decodedValue);

            if (!result)
            {
                result.Field = path + ".Value" + (result.Field.empty() ? "" : "." + result.Field);
                return result;
            }

            if (temporary.GetObjectDepth() != 0)
            {
                return { ArchiveErrorCode::InvalidOperation, path + ".Value" };
            }

            if (!candidate.emplace(std::move(decodedKey), std::move(decodedValue)).second)
            {
                return InvalidValue(path + ".Key");
            }
        }

        values.swap(candidate);
        return {};
    }

    template<typename T, typename Writer>
    ArchiveResult GlazeArchiveBase::WriteObjectArray(
        std::string_view key, const std::vector<T>& values, Writer&& writeObject)
    {
        auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        Value candidate;
        candidate.data = Value::array_t{};

        auto& elements = candidate.get_array();
        elements.reserve(values.size());

        for (std::size_t index = 0; index < values.size(); ++index)
        {
            const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
            ObjectArchive temporary;
            ArchiveResult result = std::invoke(writeObject, temporary, values[index]);

            if (!result)
            {
                result.Field = path + (result.Field.empty() ? "" : "." + result.Field);
                return result;
            }

            if (temporary.GetObjectDepth() != 0)
            {
                return { ArchiveErrorCode::InvalidOperation, path };
            }

            elements.push_back(std::move(temporary.m_Root));
        }

        object->insert_or_assign(std::string{ key }, std::move(candidate));
        return {};
    }

    template<typename T, typename Reader>
    ArchiveResult GlazeArchiveBase::ReadObjectArray(
        std::string_view key, std::vector<T>& values, Reader&& readObject) const
    {
        const auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        const auto it = object->find(key);

        if (it == object->end())
        {
            return { ArchiveErrorCode::MissingField, std::string{ key } };
        }

        const auto* elements = it->second.get_if<Value::array_t>();

        if (!elements)
        {
            return InvalidValue(key);
        }

        std::vector<T> candidate;
        candidate.reserve(elements->size());

        for (std::size_t index = 0; index < elements->size(); ++index)
        {
            const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
            const auto& source = (*elements)[index];

            if (!source.is_object())
            {
                return InvalidValue(path);
            }

            ObjectArchive temporary;
            temporary.m_Root = source;

            T value{};
            ArchiveResult result = std::invoke(readObject, temporary, value);

            if (!result)
            {
                result.Field = path + (result.Field.empty() ? "" : "." + result.Field);
                return result;
            }

            if (temporary.GetObjectDepth() != 0)
            {
                return { ArchiveErrorCode::InvalidOperation, path };
            }

            candidate.push_back(std::move(value));
        }

        values.swap(candidate);
        return {};
    }

    template<typename T, typename Writer>
    ArchiveResult GlazeArchiveBase::WriteObject(std::string_view key, const T& value, Writer&& writeObject)
    {
        auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        ObjectArchive temporary;
        ArchiveResult result = std::invoke(writeObject, temporary, value);

        if (!result)
        {
            return result;
        }

        if (temporary.GetObjectDepth() != 0)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        object->insert_or_assign(std::string{ key }, std::move(temporary.m_Root));
        return {};
    }

    template<typename T, typename Reader>
        requires (std::copy_constructible<T>&& std::assignable_from<T&, T>)
    ArchiveResult GlazeArchiveBase::ReadObject(std::string_view key, T& value, Reader&& readObject) const
    {
        const auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        const auto it = object->find(key);

        if (it == object->end())
        {
            return { ArchiveErrorCode::MissingField, std::string{ key } };
        }

        if (!it->second.is_object())
        {
            return InvalidValue(key);
        }

        ObjectArchive temporary;
        temporary.m_Root = it->second;

        T candidate = value;
        ArchiveResult result = std::invoke(readObject, temporary, candidate);

        if (!result)
        {
            return result;
        }

        if (temporary.GetObjectDepth() != 0)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        value = std::move(candidate);
        return {};
    }
}