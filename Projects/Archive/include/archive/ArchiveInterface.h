#pragma once

#include <archive/ArchiveResult.h>
#include <archive/ArchiveTypes.h>
#include <archive/Unicode.h>

#include <cmath>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>
#include <ranges>
#include <set>
#include <unordered_set>
#include <functional>
#include <optional>

namespace archive
{
    class IArchive
    {
    public:
        virtual ~IArchive() = default;

#pragma region Write
        template<ArchiveScalar T>
        [[nodiscard]] ArchiveResult Write(std::string_view key, const T& value)
        {
            ArchiveScalarValue candidate;
            const auto result = MakeScalarValue(key, value, candidate);

            if (!result)
            {
                return result;
            }

            return WriteValue(key, candidate);
        }

        template<typename Range>
        requires (std::ranges::input_range<const Range> && ArchiveScalar<std::ranges::range_value_t<const Range>>)
        [[nodiscard]] ArchiveResult WriteArray(std::string_view key, const Range& values)
        {
            using T = std::ranges::range_value_t<const Range>;
            std::vector<ArchiveScalarValue> candidate;

            if constexpr (std::ranges::sized_range<const Range>)
            {
                candidate.reserve(std::ranges::size(values));
            }

            for (const auto& value : values)
            {
                ArchiveScalarValue element;
                const auto result = MakeScalarValue<T>(key, value, element);

                if (!result)
                {
                    return result;
                }

                candidate.push_back(std::move(element));
            }

            return WriteArrayValues(key, GetScalarType<T>(), candidate);
        }

        template<ArchiveMap Map>
        [[nodiscard]] ArchiveResult WriteMap(std::string_view key, const Map& values)
        {
            std::vector<ArchiveScalarMapEntry> candidate;
            candidate.reserve(values.size());
            std::size_t index = 0;

            for (const auto& [mapKey, mapValue] : values)
            {
                const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
                ArchiveScalarMapEntry entry;
                const auto keyResult = MakeScalarValue(path + ".Key", mapKey, entry.Key);

                if (!keyResult)
                {
                    return keyResult;
                }

                const auto valueResult = MakeScalarValue(path + ".Value", mapValue, entry.Value);

                if (!valueResult)
                {
                    return valueResult;
                }

                candidate.push_back(std::move(entry));
                ++index;
            }

            return WriteMapValues(key, GetScalarType<typename Map::key_type>(),
                GetScalarType<typename Map::mapped_type>(), candidate);
        }

        template<typename T, typename Writer>
        requires std::invocable<Writer&, IArchive&, const T&>
        [[nodiscard]] ArchiveResult WriteObject(std::string_view key, const T& value, Writer&& writeObject)
        {
            return WriteObjectValue(key, [&value, &writeObject, key](IArchive& temporary) -> ArchiveResult
                {
                    const auto result = std::invoke(writeObject, temporary, value);

                    if (!result)
                    {
                        return result;
                    }

                    if (temporary.GetObjectDepth() != 0)
                    {
                        return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
                    }

                    return {};
                });
        }
        template<typename T, typename Writer>
        requires std::invocable<Writer&, IArchive&, const T&>
        [[nodiscard]] ArchiveResult WriteObjectArray(std::string_view key, const std::vector<T>& values, Writer&& writeObject)
        {
            return WriteObjectArrayValues(key, values.size(),
                [&values, &writeObject, key](IArchive& temporary, std::size_t index) -> ArchiveResult
                {
                    const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
                    auto result = std::invoke(writeObject, temporary, values[index]);

                    if (!result)
                    {
                        result.Field = path + (result.Field.empty() ? "" : "." + result.Field);
                        return result;
                    }

                    if (temporary.GetObjectDepth() != 0)
                    {
                        return { ArchiveErrorCode::InvalidOperation, path };
                    }

                    return {};
                });
        }
        template<ArchiveObjectMap Map, typename Writer>
            requires std::invocable<Writer&, IArchive&, const typename Map::mapped_type&>
        [[nodiscard]] ArchiveResult WriteObjectMap(std::string_view key, const Map& values, Writer&& writeObject)
        {
            auto entry = values.begin();

            return WriteObjectMapValues(key, values.size(), GetScalarType<typename Map::key_type>(),
                [&entry, key](std::size_t index, ArchiveScalarValue& destination) -> ArchiveResult
                {
                    const auto path = std::string{ key } + "[" + std::to_string(index) + "].Key";
                    return MakeScalarValue(path, entry->first, destination);
                },
                [&entry, &writeObject, key](IArchive& temporary, std::size_t index) -> ArchiveResult
                {
                    const auto path = std::string{ key } + "[" + std::to_string(index) + "].Value";
                    auto result = std::invoke(writeObject, temporary, entry->second);

                    if (!result)
                    {
                        result.Field = path + (result.Field.empty() ? "" : "." + result.Field);
                        return result;
                    }

                    if (temporary.GetObjectDepth() != 0)
                    {
                        return { ArchiveErrorCode::InvalidOperation, path };
                    }

                    ++entry;
                    return {};
                });
        }

#pragma endregion

#pragma region Read

		//Scalar read function for scalar types that converts the value to the correct type.
        template<ArchiveScalar T>
        [[nodiscard]] ArchiveResult Read(std::string_view key, T& value) const
        {
            ArchiveScalarValue source;
            const auto result = ReadValue(key, GetScalarType<T>(), source);

            if (!result)
            {
                return result;
            }

            return ConvertScalarValue(key, source, value);
        }

		//Vector read function for std::vector<T> that converts the values to the correct type.
        template<ArchiveScalar T>
        [[nodiscard]] ArchiveResult ReadArray(std::string_view key, std::vector<T>& values) const
        {
            std::vector<ArchiveScalarValue> sources;
            const auto result = ReadArrayValues(key, GetScalarType<T>(), sources);

            if (!result)
            {
                return result;
            }

            std::vector<T> candidate;

            if (sources.size() > candidate.max_size())
            {
                return { ArchiveErrorCode::InvalidValue, std::string{ key } };
            }

            candidate.reserve(sources.size());

            for (const auto& source : sources)
            {
                T element{};
                const auto conversion = ConvertScalarValue(key, source, element);

                if (!conversion)
                {
                    return conversion;
                }

                candidate.push_back(std::move(element));
            }

            values.swap(candidate);
            return {};
        }
		//Unordered set read function for std::unordered_set<T, Hash, Equal, Allocator> that checks for duplicates and converts the values to the correct type.
        template<ArchiveScalar T, typename Hash, typename Equal, typename Allocator>
        [[nodiscard]] ArchiveResult ReadArray(
            std::string_view key, std::unordered_set<T, Hash, Equal, Allocator>& values) const
        {
            std::vector<ArchiveScalarValue> sources;
            const auto result = ReadArrayValues(key, GetScalarType<T>(), sources);

            if (!result)
            {
                return result;
            }

            using Set = std::unordered_set<T, Hash, Equal, Allocator>;
            Set candidate{ 0, values.hash_function(), values.key_eq(), values.get_allocator() };
            candidate.max_load_factor(values.max_load_factor());

            if (sources.size() > candidate.max_size())
            {
                return { ArchiveErrorCode::InvalidValue, std::string{ key } };
            }

            candidate.reserve(sources.size());

            for (const auto& source : sources)
            {
                T element{};
                const auto conversion = ConvertScalarValue(key, source, element);

                if (!conversion)
                {
                    return conversion;
                }

                if (!candidate.insert(std::move(element)).second)
                {
                    return { ArchiveErrorCode::InvalidValue, std::string{ key } };
                }
            }

            values.swap(candidate);
            return {};
        }

		//Set read function for std::set<T, Compare, Allocator> that checks for duplicates and converts the values to the correct type.
        template<ArchiveScalar T, typename Compare, typename Allocator>
        [[nodiscard]] ArchiveResult ReadArray(std::string_view key, std::set<T, Compare, Allocator>& values) const
        {
            std::vector<ArchiveScalarValue> sources;
            const auto result = ReadArrayValues(key, GetScalarType<T>(), sources);

            if (!result)
            {
                return result;
            }

            using Set = std::set<T, Compare, Allocator>;
            Set candidate{ values.key_comp(), values.get_allocator() };

            if (sources.size() > candidate.max_size())
            {
                return { ArchiveErrorCode::InvalidValue, std::string{ key } };
            }

            for (const auto& source : sources)
            {
                T element{};
                const auto conversion = ConvertScalarValue(key, source, element);

                if (!conversion)
                {
                    return conversion;
                }

                if (!candidate.insert(std::move(element)).second)
                {
                    return { ArchiveErrorCode::InvalidValue, std::string{ key } };
                }
            }

            values.swap(candidate);
            return {};
        }

		//Array read function for std::array<T, N> that checks the size of the array and converts the values to the correct type.
        template<ArchiveScalar T, std::size_t N>
        [[nodiscard]] ArchiveResult ReadArray(std::string_view key, std::array<T, N>& values) const
        {
            std::size_t size = 0;
            const auto sizeResult = GetArraySize(key, size);

            if (!sizeResult)
            {
                return sizeResult;
            }

            if (size != N)
            {
                return { ArchiveErrorCode::InvalidValue, std::string{ key } };
            }

            std::vector<ArchiveScalarValue> sources;
            const auto result = ReadArrayValues(key, GetScalarType<T>(), sources);

            if (!result)
            {
                return result;
            }

            if (sources.size() != N)
            {
                return { ArchiveErrorCode::InvalidValue, std::string{ key } };
            }

            std::array<T, N> candidate{};

            for (std::size_t index = 0; index < N; ++index)
            {
                const auto conversion = ConvertScalarValue(key, sources[index], candidate[index]);

                if (!conversion)
                {
                    return conversion;
                }
            }

            values = std::move(candidate);
            return {};
        }

        template<ArchiveMap Map>
        [[nodiscard]] ArchiveResult ReadMap(std::string_view key, Map& values) const
        {
            using Key = typename Map::key_type;
            using Mapped = typename Map::mapped_type;

            std::size_t size = 0;
            const auto sizeResult = GetMapSize(key, size);

            if (!sizeResult)
            {
                return sizeResult;
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

            if (size > candidate.max_size())
            {
                return { ArchiveErrorCode::InvalidValue, std::string{ key } };
            }

            if constexpr (detail::ArchiveMapTraits<Map>::IsUnordered)
            {
                candidate.max_load_factor(values.max_load_factor());
                candidate.reserve(size);
            }

            for (std::size_t index = 0; index < size; ++index)
            {
                const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
                ArchiveScalarValue keySource;
                const auto keyResult = ReadMapKey(key, index, GetScalarType<Key>(), keySource);

                if (!keyResult)
                {
                    return keyResult;
                }

                Key decodedKey{};
                const auto keyConversion = ConvertScalarValue(path + ".Key", keySource, decodedKey);

                if (!keyConversion)
                {
                    return keyConversion;
                }

                ArchiveScalarValue valueSource;
                const auto valueResult = ReadMapValue(key, index, GetScalarType<Mapped>(), valueSource);

                if (!valueResult)
                {
                    return valueResult;
                }

                Mapped decodedValue{};
                const auto valueConversion = ConvertScalarValue(path + ".Value", valueSource, decodedValue);

                if (!valueConversion)
                {
                    return valueConversion;
                }

                if (!candidate.emplace(std::move(decodedKey), std::move(decodedValue)).second)
                {
                    return { ArchiveErrorCode::InvalidValue, path + ".Key" };
                }
            }

            values.swap(candidate);
            return {};
        }

        template<typename T, typename Reader>
        requires (std::copy_constructible<T>&& std::assignable_from<T&, T> && std::invocable<Reader&, IArchive&, T&>)
        [[nodiscard]] ArchiveResult ReadObject(std::string_view key, T& value, Reader&& readObject) const
        {
            std::optional<T> candidate;

            const auto result = ReadObjectValue(key, [&value, &readObject, &candidate, key](IArchive& temporary)
                -> ArchiveResult
                {
                    candidate.emplace(value);
                    const auto readResult = std::invoke(readObject, temporary, *candidate);

                    if (!readResult)
                    {
                        return readResult;
                    }

                    if (temporary.GetObjectDepth() != 0)
                    {
                        return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
                    }

                    return {};
                });

            if (!result)
            {
                return result;
            }

            value = std::move(*candidate);
            return {};
        }

        template<typename T, typename Reader>
        requires (std::default_initializable<T>&& std::move_constructible<T> && std::invocable<Reader&, IArchive&, T&>)
        [[nodiscard]] ArchiveResult ReadObjectArray(std::string_view key, std::vector<T>& values, Reader&& readObject) const
        {
            std::size_t size = 0;
            const auto sizeResult = GetArraySize(key, size);

            if (!sizeResult)
            {
                return sizeResult;
            }

            std::vector<T> candidate;

            if (size > candidate.max_size())
            {
                return { ArchiveErrorCode::InvalidValue, std::string{ key } };
            }

            candidate.reserve(size);

            const auto result = ReadObjectArrayValues(key,
                [&candidate, &readObject, key](IArchive& temporary, std::size_t index) -> ArchiveResult
                {
                    const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
                    T element{};
                    auto readResult = std::invoke(readObject, temporary, element);

                    if (!readResult)
                    {
                        readResult.Field = path + (readResult.Field.empty() ? "" : "." + readResult.Field);
                        return readResult;
                    }

                    if (temporary.GetObjectDepth() != 0)
                    {
                        return { ArchiveErrorCode::InvalidOperation, path };
                    }

                    candidate.push_back(std::move(element));
                    return {};
                });

            if (!result)
            {
                return result;
            }

            values.swap(candidate);
            return {};
        }
        template<ArchiveObjectMap Map, typename Reader>
        requires (std::default_initializable<typename Map::mapped_type> && std::move_constructible<typename Map::mapped_type> && std::invocable<Reader&, IArchive&, typename Map::mapped_type&>)
        [[nodiscard]] ArchiveResult ReadObjectMap(std::string_view key, Map& values, Reader&& readObject) const
        {
            using Key = typename Map::key_type;
            using Mapped = typename Map::mapped_type;

            std::size_t size = 0;
            const auto sizeResult = GetMapSize(key, size);

            if (!sizeResult)
            {
                return sizeResult;
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

            if (size > candidate.max_size())
            {
                return { ArchiveErrorCode::InvalidValue, std::string{ key } };
            }

            if constexpr (detail::ArchiveMapTraits<Map>::IsUnordered)
            {
                candidate.max_load_factor(values.max_load_factor());
                candidate.reserve(size);
            }

            Key decodedKey{};

            const auto result = ReadObjectMapValues(key, GetScalarType<Key>(),
                [&decodedKey, key](std::size_t index, const ArchiveScalarValue& source) -> ArchiveResult
                {
                    const auto path = std::string{ key } + "[" + std::to_string(index) + "].Key";
                    return ConvertScalarValue(path, source, decodedKey);
                },
                [&candidate, &decodedKey, &readObject, key](IArchive& temporary, std::size_t index) -> ArchiveResult
                {
                    const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
                    Mapped decodedValue{};
                    auto readResult = std::invoke(readObject, temporary, decodedValue);

                    if (!readResult)
                    {
                        readResult.Field = path + ".Value" + (readResult.Field.empty() ? "" : "." + readResult.Field);
                        return readResult;
                    }

                    if (temporary.GetObjectDepth() != 0)
                    {
                        return { ArchiveErrorCode::InvalidOperation, path + ".Value" };
                    }

                    if (!candidate.emplace(std::move(decodedKey), std::move(decodedValue)).second)
                    {
                        return { ArchiveErrorCode::InvalidValue, path + ".Key" };
                    }

                    return {};
                });

            if (!result)
            {
                return result;
            }

            values.swap(candidate);
            return {};
        }
#pragma endregion

        //Field
        [[nodiscard]] virtual ArchiveResult HasField(std::string_view key, bool& exists) const = 0;
        [[nodiscard]] virtual ArchiveResult GetFieldNames(std::vector<std::string>& names) const = 0;

        //Object
        [[nodiscard]] virtual std::size_t GetObjectDepth() const noexcept = 0;
        [[nodiscard]] virtual ArchiveResult BeginWriteObject(std::string_view key) = 0;
        [[nodiscard]] virtual ArchiveResult BeginReadObject(std::string_view key) = 0;
        [[nodiscard]] virtual ArchiveResult EndObject() = 0;

        [[nodiscard]] virtual ArchiveResult GetArraySize(std::string_view key, std::size_t& size) const = 0;
        [[nodiscard]] virtual ArchiveResult BeginWriteObjectElement(std::string_view key) = 0;
        [[nodiscard]] virtual ArchiveResult BeginReadObjectElement(std::string_view key, std::size_t index) = 0;

        //Document
        [[nodiscard]] ArchiveResult Encode(std::string& document) const;
        [[nodiscard]] ArchiveResult Decode(std::string_view document);

        //File
        [[nodiscard]] ArchiveResult SaveToFile(const std::filesystem::path& path) const;
        [[nodiscard]] ArchiveResult LoadFromFile(const std::filesystem::path& path);

        [[nodiscard]] virtual ArchiveResult GetMapSize(std::string_view key, std::size_t& size) const = 0;

    protected:
        virtual ArchiveResult EncodeDocument(std::string& document) const = 0;
        // 파싱 실패나 잘못된 루트 형식은 InvalidDocument로 반환한다.
        // 실패 시 기존 문서와 탐색 경로를 보존한다.
        virtual ArchiveResult DecodeDocument(std::string_view document) = 0;
        virtual ArchiveResult WriteValue(std::string_view key, const ArchiveScalarValue& value) = 0;        
        virtual ArchiveResult WriteArrayValues(std::string_view key, ArchiveScalarType elementType, const std::vector<ArchiveScalarValue>& values) = 0;
        virtual ArchiveResult WriteMapValues(std::string_view key, ArchiveScalarType keyType, ArchiveScalarType valueType, const std::vector<ArchiveScalarMapEntry>& entries) = 0;
        using ObjectWriter = std::function<ArchiveResult(IArchive&)>;
        virtual ArchiveResult WriteObjectValue(std::string_view key, const ObjectWriter& writeObject) = 0;
        using ObjectArrayWriter = std::function<ArchiveResult(IArchive&, std::size_t)>;
        virtual ArchiveResult WriteObjectArrayValues(std::string_view key, std::size_t size, const ObjectArrayWriter& writeObject) = 0;
        using ObjectMapKeyWriter = std::function<ArchiveResult(std::size_t, ArchiveScalarValue&)>;
        virtual ArchiveResult WriteObjectMapValues(std::string_view key, std::size_t size, ArchiveScalarType keyType,const ObjectMapKeyWriter& writeKey, const ObjectArrayWriter& writeValue) = 0;
        virtual ArchiveResult ReadValue(std::string_view key, ArchiveScalarType requestedType, ArchiveScalarValue& value) const = 0;
        virtual ArchiveResult ReadArrayValues(std::string_view key, ArchiveScalarType requestedType, std::vector<ArchiveScalarValue>& values) const = 0;
        virtual ArchiveResult ReadMapKey(std::string_view key, std::size_t index, ArchiveScalarType requestedType, ArchiveScalarValue& value) const = 0;
        virtual ArchiveResult ReadMapValue(std::string_view key, std::size_t index, ArchiveScalarType requestedType, ArchiveScalarValue& value) const = 0;
        using ObjectReader = std::function<ArchiveResult(IArchive&)>;
        virtual ArchiveResult ReadObjectValue(std::string_view key, const ObjectReader& readObject) const = 0;
        using ObjectArrayReader = std::function<ArchiveResult(IArchive&, std::size_t)>;
        virtual ArchiveResult ReadObjectArrayValues(std::string_view key, const ObjectArrayReader& readObject) const = 0;
        using ObjectMapKeyReader = std::function<ArchiveResult(std::size_t, const ArchiveScalarValue&)>;
        virtual ArchiveResult ReadObjectMapValues(std::string_view key, ArchiveScalarType keyType, const ObjectMapKeyReader& readKey, const ObjectArrayReader& readValue) const = 0;
        
    private:
        template<typename T>
        using IntegerStorage = std::conditional_t<std::is_signed_v<T>,
            std::conditional_t<sizeof(T) == 1, std::int8_t,
            std::conditional_t<sizeof(T) == 2, std::int16_t,
            std::conditional_t<sizeof(T) == 4, std::int32_t, std::int64_t>>>,
            std::conditional_t<sizeof(T) == 1, std::uint8_t,
            std::conditional_t<sizeof(T) == 2, std::uint16_t,
            std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>>>>;

        template<ArchiveScalar T>
        static constexpr ArchiveScalarType GetScalarType()
        {
            if constexpr (std::is_enum_v<T>)
            {
                using Underlying = std::underlying_type_t<T>;

                static_assert(sizeof(Underlying) == 1 || sizeof(Underlying) == 2 ||
                    sizeof(Underlying) == 4 || sizeof(Underlying) == 8, "Unsupported enum storage width.");

                return GetScalarType<IntegerStorage<Underlying>>();
            }
            else if constexpr (std::same_as<T, std::wstring>)
            {
                return ArchiveScalarType::String;
            }
            else if constexpr (std::same_as<T, bool>)
            {
                return ArchiveScalarType::Bool;
            }
            else if constexpr (std::same_as<T, std::string>)
            {
                return ArchiveScalarType::String;
            }
            else if constexpr (std::same_as<T, float>)
            {
                return ArchiveScalarType::Float;
            }
            else if constexpr (std::same_as<T, double>)
            {
                return ArchiveScalarType::Double;
            }
            else
            {
                static_assert(std::integral<T>);

                if constexpr (sizeof(T) == 1)
                {
                    return std::is_signed_v<T> ? ArchiveScalarType::Int8 : ArchiveScalarType::UInt8;
                }
                else if constexpr (sizeof(T) == 2)
                {
                    return std::is_signed_v<T> ? ArchiveScalarType::Int16 : ArchiveScalarType::UInt16;
                }
                else if constexpr (sizeof(T) == 4)
                {
                    return std::is_signed_v<T> ? ArchiveScalarType::Int32 : ArchiveScalarType::UInt32;
                }
                else
                {
                    static_assert(sizeof(T) == 8);
                    return std::is_signed_v<T> ? ArchiveScalarType::Int64 : ArchiveScalarType::UInt64;
                }
            }
        }
        template<ArchiveScalar T>
        static ArchiveResult MakeScalarValue(std::string_view key, const T& value, ArchiveScalarValue& destination)
        {
            if constexpr (std::is_enum_v<T>)
            {
                using Underlying = std::underlying_type_t<T>;

                static_assert(sizeof(Underlying) == 1 || sizeof(Underlying) == 2 ||
                    sizeof(Underlying) == 4 || sizeof(Underlying) == 8, "Unsupported enum storage width.");

                using Storage = IntegerStorage<Underlying>;
                return MakeScalarValue(key, static_cast<Storage>(value), destination);
            }
            else if constexpr (std::same_as<T, std::wstring>)
            {
                std::string utf8;

                if (!detail::WideToUtf8(value, utf8))
                {
                    return { ArchiveErrorCode::InvalidValue, std::string{ key } };
                }

                destination = std::move(utf8);
                return {};
            }
            else
            {
                if constexpr (std::floating_point<T>)
                {
                    if (!std::isfinite(value))
                    {
                        return { ArchiveErrorCode::InvalidValue, std::string{ key } };
                    }
                }

                destination.emplace<T>(value);
                return {};
            }
        }
        template<ArchiveScalar T>
        [[nodiscard]] static ArchiveResult ConvertScalarValue(std::string_view key, const ArchiveScalarValue& source, T& value)
        {
            if constexpr (std::is_enum_v<T>)
            {
                using Underlying = std::underlying_type_t<T>;

                static_assert(sizeof(Underlying) == 1 || sizeof(Underlying) == 2 ||
                    sizeof(Underlying) == 4 || sizeof(Underlying) == 8, "Unsupported enum storage width.");

                using Storage = IntegerStorage<Underlying>;
                Storage stored{};
                const auto result = ConvertScalarValue(key, source, stored);

                if (!result)
                {
                    return result;
                }

                const Storage minimum = static_cast<Storage>((std::numeric_limits<Underlying>::min)());
                const Storage maximum = static_cast<Storage>((std::numeric_limits<Underlying>::max)());

                if (stored < minimum || stored > maximum)
                {
                    return { ArchiveErrorCode::InvalidValue, std::string{ key } };
                }

                value = static_cast<T>(static_cast<Underlying>(stored));
                return {};
            }
            else if constexpr (std::same_as<T, std::wstring>)
            {
                std::string utf8;
                const auto result = ConvertScalarValue(key, source, utf8);

                if (!result)
                {
                    return result;
                }

                std::wstring candidate;

                if (!detail::Utf8ToWide(utf8, candidate))
                {
                    return { ArchiveErrorCode::InvalidValue, std::string{ key } };
                }

                value = std::move(candidate);
                return {};
            }
            else
            {
                T candidate{};
                const bool converted = std::visit([&candidate](const auto& stored)
                    {
                        using Source = std::remove_cvref_t<decltype(stored)>;

                        if constexpr (std::same_as<T, bool> || std::same_as<T, std::string>)
                        {
                            if constexpr (std::same_as<T, Source>)
                            {
                                candidate = stored;
                                return true;
                            }
                        }
                        else if constexpr (std::integral<T>)
                        {
                            if constexpr (std::integral<Source> && !std::same_as<Source, bool>)
                            {
                                if (std::in_range<T>(stored))
                                {
                                    candidate = static_cast<T>(stored);
                                    return true;
                                }
                            }
                        }
                        else
                        {
                            if constexpr ((std::integral<Source> || std::floating_point<Source>) &&
                                !std::same_as<Source, bool>)
                            {
                                const double number = static_cast<double>(stored);
                                const double limit = static_cast<double>((std::numeric_limits<T>::max)());

                                if (std::isfinite(number) && number >= -limit && number <= limit)
                                {
                                    candidate = static_cast<T>(number);
                                    return true;
                                }
                            }
                        }

                        return false;
                    }, source);

                if (!converted)
                {
                    return { ArchiveErrorCode::InvalidValue, std::string{ key } };
                }

                value = std::move(candidate);
                return {};
            }
        }
    };
}