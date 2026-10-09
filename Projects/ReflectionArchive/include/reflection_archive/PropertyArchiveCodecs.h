#pragma once

#include <archive/GlazeArchiveBase.h>
#include <reflection/PropertyInfo.h>

#include <concepts>
#include <functional>
#include <optional>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace archive_reflection
{
    class PropertyCodecs
    {
    public:
        template<typename T>
            requires (std::is_object_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>&&
        std::default_initializable<T>&& reflection::detail::SupportsPropertyCopyConstruction<T>::value)
            [[nodiscard]] bool Register(
                std::function<archive::ArchiveResult(archive::GlazeArchiveBase&, std::string_view, const T&)> write,
                std::function<archive::ArchiveResult(const archive::GlazeArchiveBase&, std::string_view, T&)> read)
        {
            if (!write || !read)
            {
                return false;
            }

            Codec codec;

            codec.Write = [write = std::move(write)](archive::GlazeArchiveBase& ar, std::string_view key,
                const reflection::ValueView& value) -> archive::ArchiveResult
                {
                    const auto* typedValue = value.Get<T>();
                    if (typedValue == nullptr)
                    {
                        return { archive::ArchiveErrorCode::InvalidOperation, std::string{ key } };
                    }

                    return write(ar, key, *typedValue);
                };

            codec.Read = [read = std::move(read)](const archive::GlazeArchiveBase& ar,
                const reflection::PropertyInfo& property, const reflection::ObjectView& object) -> archive::ArchiveResult
                {
                    const auto* current = property.ReadFrom<T>(object);
                    if (current == nullptr)
                    {
                        return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
                    }

                    T value = *current;
                    const auto result = read(ar, property.Name, value);

                    if (!result)
                    {
                        return result;
                    }

                    if (!property.WriteTo<T>(object, value))
                    {
                        return { archive::ArchiveErrorCode::InvalidOperation, property.Name };
                    }

                    return {};
                };

            return m_codecs.emplace(std::type_index{ typeid(T) }, std::move(codec)).second;
        }

        template<typename T>
            requires (std::is_object_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>&&
        std::default_initializable<T>&&
            requires (archive::GlazeArchiveBase& writer, const archive::GlazeArchiveBase& reader,
        std::string_view key, const T& source, T& destination)
        {
            { writer.WriteArray(key, source) } -> std::same_as<archive::ArchiveResult>;
            { reader.ReadArray(key, destination) } -> std::same_as<archive::ArchiveResult>;
        })
            [[nodiscard]] bool RegisterArray()
        {
            return Register<T>(
                [](archive::GlazeArchiveBase& ar, std::string_view key, const T& value) -> archive::ArchiveResult
                {
                    return ar.WriteArray(key, value);
                },
                [](const archive::GlazeArchiveBase& ar, std::string_view key, T& value) -> archive::ArchiveResult
                {
                    return ar.ReadArray(key, value);
                });
        }

        template<archive::ArchiveMap T>
            requires std::default_initializable<T>
        [[nodiscard]] bool RegisterMap()
        {
            return Register<T>(
                [](archive::GlazeArchiveBase& ar, std::string_view key, const T& value) -> archive::ArchiveResult
                {
                    return ar.WriteMap(key, value);
                },
                [](const archive::GlazeArchiveBase& ar, std::string_view key, T& value) -> archive::ArchiveResult
                {
                    return ar.ReadMap(key, value);
                });
        }

        [[nodiscard]] std::optional<archive::ArchiveResult> WriteValue(archive::GlazeArchiveBase& ar,
            std::string_view key, const reflection::ValueView& value) const
        {
            const auto found = m_codecs.find(value.GetCppType());

            if (found == m_codecs.end())
            {
                return std::nullopt;
            }

            return found->second.Write(ar, key, value);
        }

        [[nodiscard]] std::optional<archive::ArchiveResult> ReadProperty(const archive::GlazeArchiveBase& ar,
            const reflection::PropertyInfo& property, const reflection::ObjectView& object) const
        {
            const auto current = property.TryReadValue(object);

            if (!current)
            {
                return archive::ArchiveResult{ archive::ArchiveErrorCode::InvalidOperation, property.Name };
            }

            const auto found = m_codecs.find(current.Value.GetCppType());

            if (found == m_codecs.end())
            {
                return std::nullopt;
            }

            return found->second.Read(ar, property, object);
        }

    private:
        struct Codec
        {
            std::function<archive::ArchiveResult(archive::GlazeArchiveBase&,
                std::string_view, const reflection::ValueView&)> Write;

            std::function<archive::ArchiveResult(const archive::GlazeArchiveBase&,
                const reflection::PropertyInfo&, const reflection::ObjectView&)> Read;
        };

        std::unordered_map<std::type_index, Codec> m_codecs;
    };
}