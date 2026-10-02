#include <reflection/Metadata.h>

#include <utility>

namespace reflection
{
    bool Metadata::Add(std::string key, MetadataValue value)
    {
        if (Find(key) != nullptr)
        {
            return false;
        }

        m_entries.push_back(
            MetadataEntry{
                std::move(key),
                std::move(value)
            }
        );

        return true;
    }

    const MetadataValue* Metadata::Find(std::string_view key) const
    {
        for (const MetadataEntry& entry : m_entries)
        {
            if (entry.Key == key)
            {
                return &entry.Value;
            }
        }

        return nullptr;
    }
}