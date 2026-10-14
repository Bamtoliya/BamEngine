#include "TestSupport.h"
#include "ObjectTestData.h"

#include <archive/JsonArchive.h>

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <limits>

namespace archive_tests
{
    int CheckCommonScalarMaps()
    {
        archive::JsonArchive scalarMapWriter;
        archive::IArchive& scalarMapArchive = scalarMapWriter;
        const std::map<std::int16_t, std::wstring> scalarMapSource{ { -1, L"음수" }, { 7, L"한글" } };

        if (!scalarMapArchive.WriteMap("Map", scalarMapSource))
        {
            return Fail("Common map writing failed.");
        }

        std::map<std::int16_t, std::wstring> scalarMapRestored;

        if (!scalarMapArchive.ReadMap("Map", scalarMapRestored) || scalarMapRestored != scalarMapSource)
        {
            return Fail("Common scalar map round-trip failed.");
        }

        std::string scalarMapBefore;

        if (!scalarMapWriter.ToJson(scalarMapBefore))
        {
            return Fail("Cannot encode the map before failure.");
        }

        const std::map<std::int16_t, double> scalarMapInvalid{
            { 1, 1.0 }, { 2, std::numeric_limits<double>::infinity() }
        };
        const auto scalarMapFailure = scalarMapArchive.WriteMap("Map", scalarMapInvalid);
        std::string scalarMapAfter;

        if (scalarMapFailure.Code != archive::ArchiveErrorCode::InvalidValue ||
            scalarMapFailure.Field != "Map[1].Value" ||
            !scalarMapWriter.ToJson(scalarMapAfter) || scalarMapAfter != scalarMapBefore)
        {
            return Fail("Failed common map writing changed the archive.");
        }

        struct MapReadFailureCase
        {
            const char* Json;
            archive::ArchiveErrorCode Code;
            const char* Field;
        };

        const MapReadFailureCase mapReadFailures[]{
            { R"({"Map":[{"Value":"a"}]})", archive::ArchiveErrorCode::MissingField, "Map[0].Key" },
            { R"({"Map":[{"Key":40000}]})", archive::ArchiveErrorCode::MissingField, "Map[0].Value" },
            { R"({"Map":[{"Key":40000,"Value":{}}]})", archive::ArchiveErrorCode::InvalidValue, "Map[0].Key" },
            { R"({"Map":[{"Key":1,"Value":42}]})", archive::ArchiveErrorCode::InvalidValue, "Map[0].Value" },
            {
                R"({"Map":[{"Key":1,"Value":"a"},{"Key":1,"Value":"b"},42]})",
                archive::ArchiveErrorCode::InvalidValue, "Map[1].Key"
            }
        };

        for (const auto& failure : mapReadFailures)
        {
            archive::JsonArchive mapReadStorage;

            if (!mapReadStorage.Parse(failure.Json))
            {
                return Fail("Cannot prepare common map reading failure.");
            }

            archive::IArchive& mapReadArchive = mapReadStorage;
            auto unchangedMap = scalarMapSource;
            const auto readResult = mapReadArchive.ReadMap("Map", unchangedMap);

            if (readResult.Code != failure.Code || readResult.Field != failure.Field ||
                unchangedMap != scalarMapSource)
            {
                return Fail("Common map reading changed the destination or reported the wrong error.");
            }
        }

        return 0;
    }
    int CheckCommonObjectMaps()
    {
        using ItemMap = std::map<std::int16_t, InventoryItem>;
        using NamedMap = std::unordered_map<std::wstring, InventoryItem>;
        using Code = archive::ArchiveErrorCode;

        archive::JsonArchive storage;
        archive::IArchive& ar = storage;
        const ItemMap source{ { 1, { "검", 2 } }, { 2, { "방패", 1 } } };
        const NamedMap namedSource{ { L"플레이어", { "검", 2 } } };
        ItemMap restored{ { 99, { "old", 77 } } };
        NamedMap namedRestored;
        namedRestored.max_load_factor(0.75f);

        if (!ar.WriteObjectMap("Items", source, WriteInventoryItem) ||
            !ar.WriteObjectMap("Named", namedSource, WriteInventoryItem) ||
            !ar.ReadObjectMap("Items", restored, ReadInventoryItem) ||
            !ar.ReadObjectMap("Named", namedRestored, ReadInventoryItem) ||
            restored != source || namedRestored != namedSource || namedRestored.max_load_factor() != 0.75f)
        {
            return Fail("Common object map round-trip failed.");
        }

        std::string before;

        if (!storage.ToJson(before))
        {
            return Fail("Cannot capture the common object map document.");
        }

        auto invalid = source;
        invalid.at(2).Count = -1;
        const auto writeFailure = ar.WriteObjectMap("Items", invalid, WriteInventoryItem);
        std::string after;

        if (writeFailure.Code != Code::InvalidValue || writeFailure.Field != "Items[1].Value.Count" ||
            !storage.ToJson(after) || after != before)
        {
            return Fail("Failed common object map writing changed the document.");
        }

        struct ReadFailureCase
        {
            const char* Json;
            Code Expected;
            const char* Field;
        };

        const ReadFailureCase failures[]{
            { R"({"Items":[false]})", Code::InvalidValue, "Items[0]" },
            { R"({"Items":[{"Key":40000}]})", Code::MissingField, "Items[0].Value" },
            { R"({"Items":[{"Key":40000,"Value":7}]})", Code::InvalidValue, "Items[0].Value" },
            {
                R"({"Items":[{"Key":40000,"Value":{"Name":"a","Count":1}}]})",
                Code::InvalidValue, "Items[0].Key"
            },
            {
                R"({"Items":[{"Key":1,"Value":{"Name":"a","Count":1}},)"
                R"({"Key":1,"Value":{"Name":"b","Count":2}},false]})",
                Code::InvalidValue, "Items[1].Key"
            },
            {
                R"({"Items":[{"Key":1,"Value":{"Name":"a","Count":1}},)"
                R"({"Key":1,"Value":{"Name":"b","Count":"bad"}}]})",
                Code::InvalidValue, "Items[1].Value.Count"
            }
        };

        for (const auto& failure : failures)
        {
            archive::JsonArchive reader;

            if (!reader.Parse(failure.Json))
            {
                return Fail("Cannot prepare common object map reading failure.");
            }

            const archive::IArchive& input = reader;
            auto unchanged = source;
            const auto readFailure = input.ReadObjectMap("Items", unchanged, ReadInventoryItem);

            if (readFailure.Code != failure.Expected || readFailure.Field != failure.Field || unchanged != source)
            {
                return Fail("Common object map reading changed the destination or reported the wrong error.");
            }
        }

        const ItemMap empty;

        if (!ar.WriteObjectMap("Items", empty, WriteInventoryItem) ||
            !ar.ReadObjectMap("Items", restored, ReadInventoryItem) || !restored.empty())
        {
            return Fail("Empty common object map restoration failed.");
        }

        return 0;
    }
}