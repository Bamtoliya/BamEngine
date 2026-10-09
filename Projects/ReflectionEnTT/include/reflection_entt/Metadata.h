#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace reflection_entt
{
    using MetadataValue = std::variant<bool, std::int64_t, std::uint64_t, double, std::string>;

    struct MetadataEntry
    {
        std::string Key;
        MetadataValue Value;
    };

    class Metadata
    {
    public:
        [[nodiscard]] bool Add(std::string key, MetadataValue value)
        {
            if (Find(key) != nullptr)
            {
                return false;
            }
            m_entries.push_back({std::move(key), std::move(value)});
            return true;
        }

        [[nodiscard]] const MetadataValue* Find(std::string_view key) const
        {
            for (const auto& entry : m_entries)
            {
                if (entry.Key == key)
                {
                    return &entry.Value;
                }
            }
            return nullptr;
        }

        template<typename T>
        [[nodiscard]] const T* FindAs(std::string_view key) const
        {
            const auto* value = Find(key);
            return value ? std::get_if<T>(value) : nullptr;
        }

        [[nodiscard]] const std::vector<MetadataEntry>& Entries() const noexcept { return m_entries; }

    private:
        std::vector<MetadataEntry> m_entries;
    };

    // Metadata owns its strings; it is attached once per meta object through custom().
    template<typename MetaObject>
    [[nodiscard]] const Metadata* GetMetadata(const MetaObject& object)
    {
        return object ? static_cast<const Metadata*>(object.custom()) : nullptr;
    }
}
