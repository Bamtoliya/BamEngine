#include "TestSupport.h"
#include "ObjectTestData.h"

#include <archive/BeveArchive.h>
#include <glaze/beve.hpp>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <string>
#include <system_error>
#include <vector>

namespace archive_tests
{
    int CheckBeveArchive()
    {
        using Code = archive::ArchiveErrorCode;
        using ItemMap = std::map<std::int16_t, InventoryItem>;

        const auto minimum = (std::numeric_limits<std::int64_t>::min)();
        const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
        const std::string embedded{ "A\0B", 3 };
        const std::wstring label = L"한글 \U0001F600";
        const std::vector<std::int16_t> numbers{ -1, 2, 300 };
        const ItemMap items{ { 1, { "검", 2 } }, { 2, { "방패", 1 } } };

        archive::BeveArchive writer;
        archive::IArchive& output = writer;

        if (!output.Write("Minimum", minimum) || !output.Write("Maximum", maximum) ||
            !output.Write("Embedded", embedded) || !output.Write("Label", label) ||
            !output.WriteArray("Numbers", numbers) ||
            !output.WriteObjectMap("Items", items, WriteInventoryItem))
        {
            return Fail("BEVE writing failed.");
        }

        std::string bytes;

        if (!writer.ToBeve(bytes) || bytes.empty())
        {
            return Fail("BEVE encoding failed.");
        }

        archive::BeveArchive reader;

        if (!reader.Parse(bytes))
        {
            return Fail("BEVE parsing failed.");
        }

        const archive::IArchive& input = reader;
        std::int64_t restoredMinimum{};
        std::uint64_t restoredMaximum{};
        std::string restoredEmbedded;
        std::wstring restoredLabel;
        std::vector<std::int16_t> restoredNumbers;
        ItemMap restoredItems;

        if (!input.Read("Minimum", restoredMinimum) || !input.Read("Maximum", restoredMaximum) ||
            !input.Read("Embedded", restoredEmbedded) || !input.Read("Label", restoredLabel) ||
            !input.ReadArray("Numbers", restoredNumbers) ||
            !input.ReadObjectMap("Items", restoredItems, ReadInventoryItem) ||
            restoredMinimum != minimum || restoredMaximum != maximum ||
            restoredEmbedded != embedded || restoredLabel != label ||
            restoredNumbers != numbers || restoredItems != items)
        {
            return Fail("BEVE round-trip values differ.");
        }

        std::string before;

        if (!reader.ToBeve(before))
        {
            return Fail("Cannot capture the BEVE document.");
        }

        glz::generic_u64 scalarRoot;
        scalarRoot.data = std::int64_t{ 7 };
        std::string scalarBytes;

        if (glz::write_beve(scalarRoot, scalarBytes))
        {
            return Fail("Cannot prepare the scalar BEVE root.");
        }

        auto trailingBytes = bytes;
        trailingBytes.push_back('\0');

        const std::vector<std::string> invalidInputs{
            "", bytes.substr(0, bytes.size() - 1), trailingBytes, scalarBytes
        };

        for (const auto& invalid : invalidInputs)
        {
            const auto result = reader.Parse(invalid);
            std::string after;

            if (result.Code != Code::InvalidDocument || !reader.ToBeve(after) || after != before)
            {
                return Fail("Failed BEVE parsing changed the document.");
            }
        }

        if (!reader.BeginReadObjectElement("Items", 0))
        {
            return Fail("Cannot open the BEVE object element.");
        }

        std::string unchangedBytes = "keep";

        if (reader.ToBeve(unchangedBytes).Code != Code::InvalidOperation || unchangedBytes != "keep" ||
            reader.Parse(bytes).Code != Code::InvalidOperation || reader.GetObjectDepth() != 1 ||
            !reader.EndObject())
        {
            return Fail("BEVE document operations changed an open object.");
        }

        const auto directory = std::filesystem::current_path() / "archive-example";
        std::error_code directoryError;
        std::filesystem::create_directories(directory, directoryError);

        if (directoryError)
        {
            return Fail("Cannot create the BEVE test directory.");
        }

        const auto path = directory / "round-trip.beve";
        archive::BeveArchive fileReader;
        std::string fileEmbedded;
        ItemMap fileItems;

        if (!writer.SaveToFile(path) || !fileReader.LoadFromFile(path) ||
            !fileReader.Read("Embedded", fileEmbedded) ||
            !fileReader.ReadObjectMap("Items", fileItems, ReadInventoryItem) ||
            fileEmbedded != embedded || fileItems != items)
        {
            return Fail("BEVE binary file round-trip failed.");
        }

        return 0;
    }
}