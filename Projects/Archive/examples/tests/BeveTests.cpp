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
#include <archive/JsonArchive.h>
#include <algorithm>
#include <array>

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

        {
            std::vector<std::uint8_t> payload(65536);
            for (std::size_t i = 0; i < payload.size(); ++i)
                payload[i] = static_cast<std::uint8_t>(i % 256);

            archive::BeveArchive packedWriter;
            std::string packedDocument;

            if (!packedWriter.WriteArray("Bytes", payload) ||
                !packedWriter.WriteArray("Empty", std::vector<std::uint8_t>{}) ||
                !packedWriter.ToBeve(packedDocument) || packedDocument.size() > payload.size() + 1024)
            {
                return Fail("Packed byte writing or encoded size is incorrect.");
            }

            archive::BeveArchive packedReader;
            std::vector<std::uint8_t> restored;
            std::vector<std::uint8_t> empty{ 42 };
            std::vector<std::uint32_t> widened;
            std::size_t count = 0;

            if (!packedReader.Parse(packedDocument) ||
                !packedReader.ReadArray("Bytes", restored) || restored != payload ||
                !packedReader.ReadArray("Empty", empty) || !empty.empty() ||
                !packedReader.GetArraySize("Bytes", count) || count != payload.size() ||
                !packedReader.ReadArray("Bytes", widened) || widened.size() != payload.size() ||
                !std::equal(payload.begin(), payload.end(), widened.begin()))
            {
                return Fail("Packed byte reading is incorrect.");
            }

            count = 0;
            if (!packedReader.ReadArrayElements("Bytes",
                [&](archive::IArchive& entry, std::string_view key, std::size_t index) -> archive::ArchiveResult
                {
                    std::uint8_t value = 0;
                    const auto result = entry.Read(key, value);
                    if (!result)
                        return result;

                    if (value != payload[index])
                        return { Code::InvalidValue, std::string{ key } };

                    ++count;
                    return {};
                }) || count != payload.size())
            {
                return Fail("Packed byte element reading is incorrect.");
            }

            // 기존 generic_u64 형식으로 만든 BEVE를 새 구현에서 읽습니다.
            glz::generic_u64 legacy;
            legacy.data = glz::generic_u64::object_t{};
            auto& legacyField = legacy.get_object()["Bytes"];
            legacyField.data = glz::generic_u64::array_t{};

            const std::array<std::uint8_t, 4> sample{ 0, 127, 128, 255 };
            for (const auto byte : sample)
            {
                glz::generic_u64 element;
                element.data = static_cast<std::uint64_t>(byte);
                legacyField.get_array().push_back(std::move(element));
            }

            std::string legacyDocument;
            if (glz::write_beve(legacy, legacyDocument) ||
                !packedReader.Parse(legacyDocument) ||
                !packedReader.ReadArray("Bytes", restored) ||
                restored != std::vector<std::uint8_t>(sample.begin(), sample.end()))
            {
                return Fail("Legacy byte array compatibility failed.");
            }

            legacyField.get_array()[0].data = std::int64_t{ -1 };
            const auto beforeInvalidRead = restored;

            if (glz::write_beve(legacy, legacyDocument) ||
                !packedReader.Parse(legacyDocument) ||
                packedReader.ReadArray("Bytes", restored).Code != Code::InvalidValue ||
                restored != beforeInvalidRead)
            {
                return Fail("Invalid legacy byte data changed the destination.");
            }

            archive::JsonArchive jsonWriter;
            archive::JsonArchive jsonReader;
            std::string json;

            if (!jsonWriter.WriteArray("Bytes", payload) || !jsonWriter.ToJson(json) ||
                !jsonReader.Parse(json) || !jsonReader.ReadArray("Bytes", restored) || restored != payload)
            {
                return Fail("JSON byte array compatibility failed.");
            }
        }

        return 0;
    }
}