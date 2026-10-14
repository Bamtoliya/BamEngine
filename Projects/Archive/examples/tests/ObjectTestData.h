#pragma once

#include <archive/ArchiveInterface.h>

#include <cstdint>
#include <string>

namespace archive_tests
{
    struct InventoryItem
    {
        std::string Name;
        std::int32_t Count = 0;

        bool operator==(const InventoryItem&) const = default;
    };

    inline archive::ArchiveResult WriteInventoryItem(archive::IArchive& object, const InventoryItem& item)
    {
        const auto result = object.Write("Name", item.Name);

        if (!result)
        {
            return result;
        }

        if (item.Count < 0)
        {
            return { archive::ArchiveErrorCode::InvalidValue, "Count" };
        }

        return object.Write("Count", item.Count);
    }

    inline archive::ArchiveResult ReadInventoryItem(archive::IArchive& object, InventoryItem& item)
    {
        const auto result = object.Read("Name", item.Name);

        if (!result)
        {
            return result;
        }

        return object.Read("Count", item.Count);
    }
}