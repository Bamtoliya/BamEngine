#include "TestSupport.h"
#include "ObjectTestData.h"

#include <archive/JsonArchive.h>

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

namespace archive_tests
{
    int CheckObjectNavigation()
    {
        archive::JsonArchive writer;

        if (!writer.BeginWriteObject("Player") ||
            !writer.Write("Name", std::string{ "플레이어" }) ||
            !writer.BeginWriteObject("Movement") ||
            !writer.Write("Speed", 12.5f))
        {
            return Fail("Nested object writing failed.");
        }

        if (writer.GetObjectDepth() != 2)
        {
            return Fail("Nested object depth check failed.");
        }

        std::string rejectedJson = "unchanged";
        auto result = writer.ToJson(rejectedJson);

        if (result.Code != archive::ArchiveErrorCode::InvalidOperation || rejectedJson != "unchanged")
        {
            return Fail("Unclosed object encoding check failed.");
        }

        result = writer.Parse("{}");

        if (result.Code != archive::ArchiveErrorCode::InvalidOperation || writer.GetObjectDepth() != 2)
        {
            return Fail("Unclosed object parsing check failed.");
        }

        if (!writer.EndObject() || !writer.EndObject())
        {
            return Fail("Nested object closing failed.");
        }

        std::string json;

        if (!writer.ToJson(json))
        {
            return Fail("Nested object encoding failed.");
        }

        archive::JsonArchive reader;

        if (!reader.Parse(json) || !reader.BeginReadObject("Player"))
        {
            return Fail("Nested object parsing failed.");
        }

        std::string playerName;

        if (!reader.Read("Name", playerName) || playerName != "플레이어" ||
            !reader.BeginReadObject("Movement"))
        {
            return Fail("Nested player reading failed.");
        }

        float speed = 0.0f;

        if (!reader.Read("Speed", speed) || speed != 12.5f)
        {
            return Fail("Nested speed reading failed.");
        }

        result = reader.BeginReadObject("Missing");

        if (result.Code != archive::ArchiveErrorCode::MissingField || reader.GetObjectDepth() != 2)
        {
            return Fail("Missing object changed the current path.");
        }

        result = reader.BeginReadObject("Speed");

        if (result.Code != archive::ArchiveErrorCode::InvalidValue || reader.GetObjectDepth() != 2)
        {
            return Fail("Invalid object changed the current path.");
        }

        result = reader.BeginWriteObject("Speed");

        if (result.Code != archive::ArchiveErrorCode::InvalidValue || reader.GetObjectDepth() != 2)
        {
            return Fail("Object writing replaced a scalar field or changed the current path.");
        }

        if (!reader.EndObject() || !reader.EndObject())
        {
            return Fail("Nested reading scope closing failed.");
        }

        result = reader.EndObject();

        if (result.Code != archive::ArchiveErrorCode::InvalidOperation || reader.GetObjectDepth() != 0)
        {
            return Fail("Root object closing check failed.");
        }

        std::string restoredJson;

        if (!reader.ToJson(restoredJson) || restoredJson != json)
        {
            return Fail("Nested object document changed unexpectedly.");
        }

        std::cout << "JSON object navigation checks passed.\n";
        return 0;
    }

    int CheckSingleObjects()
    {
        archive::ArchiveResult result;
        const InventoryItem featuredItem{ "장검", 5 };
        archive::JsonArchive singleWriter;
        archive::IArchive& singleArchive = singleWriter;

        if (!singleArchive.WriteObject("Featured", featuredItem, WriteInventoryItem))
        {
            return Fail("Single object writing failed.");
        }

        std::string singleJson;

        if (!singleWriter.ToJson(singleJson))
        {
            return Fail("Single object encoding failed.");
        }

        archive::JsonArchive singleReader;
        InventoryItem restoredFeatured{ "old", 77 };
        const archive::IArchive& singleReadArchive = singleReader;

        if (!singleReader.Parse(singleJson) ||
            !singleReadArchive.ReadObject("Featured", restoredFeatured, ReadInventoryItem) ||
            restoredFeatured != featuredItem)
        {
            return Fail("Single object round-trip failed.");
        }

        std::string beforeSingleFailure;

        if (!singleWriter.ToJson(beforeSingleFailure))
        {
            return Fail("Cannot capture the single object document.");
        }

        const InventoryItem invalidFeatured{ "실패한 변경", -1 };
        result = singleArchive.WriteObject("Featured", invalidFeatured, WriteInventoryItem);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue || result.Field != "Count")
        {
            return Fail("Single object write failure check failed.");
        }

        std::string afterSingleFailure;

        if (!singleWriter.ToJson(afterSingleFailure) || afterSingleFailure != beforeSingleFailure)
        {
            return Fail("Failed single object writing changed the document.");
        }

        archive::JsonArchive singleFailureReader;
        const archive::IArchive& singleFailureArchive = singleFailureReader;

        if (!singleFailureReader.Parse(R"({"Featured":{"Name":"바뀜","Count":"bad"},"Scalar":12})"))
        {
            return Fail("Cannot prepare single object failure checks.");
        }

        const InventoryItem originalFeatured{ "보존", 77 };
        InventoryItem unchangedFeatured = originalFeatured;

        result = singleFailureArchive.ReadObject("Featured", unchangedFeatured, ReadInventoryItem);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue || result.Field != "Count" ||
            unchangedFeatured != originalFeatured)
        {
            return Fail("Failed single object reading changed the destination.");
        }

        result = singleFailureArchive.ReadObject("Missing", unchangedFeatured, ReadInventoryItem);

        if (result.Code != archive::ArchiveErrorCode::MissingField || unchangedFeatured != originalFeatured)
        {
            return Fail("Missing single object check failed.");
        }

        result = singleFailureArchive.ReadObject("Scalar", unchangedFeatured, ReadInventoryItem);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue || unchangedFeatured != originalFeatured)
        {
            return Fail("Invalid single object type check failed.");
        }

        const auto unclosedWriter =
            [](archive::IArchive& object, const InventoryItem&) -> archive::ArchiveResult
            {
                return object.BeginWriteObject("Unclosed");
            };

        result = singleArchive.WriteObject("Featured", featuredItem, unclosedWriter);

        if (result.Code != archive::ArchiveErrorCode::InvalidOperation)
        {
            return Fail("Unclosed callback object check failed.");
        }

        if (!singleWriter.ToJson(afterSingleFailure) || afterSingleFailure != beforeSingleFailure)
        {
            return Fail("Unclosed callback changed the document.");
        }

        archive::JsonArchive unclosedReadStorage;

        if (!unclosedReadStorage.Parse(R"({"Featured":{"Name":"바뀜","Nested":{}}})"))
        {
            return Fail("Cannot prepare unclosed object reading.");
        }

        const archive::IArchive& unclosedReadArchive = unclosedReadStorage;
        InventoryItem unclosedReadTarget = originalFeatured;

        const auto unclosedReader = [](archive::IArchive& object, InventoryItem& item) -> archive::ArchiveResult
            {
                const auto readResult = object.Read("Name", item.Name);

                if (!readResult)
                {
                    return readResult;
                }

                return object.BeginReadObject("Nested");
            };

        result = unclosedReadArchive.ReadObject("Featured", unclosedReadTarget, unclosedReader);

        if (result.Code != archive::ArchiveErrorCode::InvalidOperation || result.Field != "Featured" ||
            unclosedReadTarget != originalFeatured || unclosedReadArchive.GetObjectDepth() != 0)
        {
            return Fail("Unclosed object reading changed the destination or navigation state.");
        }

        std::cout << "JSON single object checks passed.\n";
        std::cout << "JSON single object failure checks passed.\n";
        return 0;
}

    int CheckObjectArrays()
    {
        archive::ArchiveResult result;
        const std::vector<InventoryItem> sourceItems{ { "검", 2 }, { "방패", 1 } };
        archive::JsonArchive itemWriter;
        archive::IArchive& itemArchive = itemWriter;

        if (!itemArchive.WriteObjectArray("Items", sourceItems, WriteInventoryItem))
        {
            return Fail("Object array writing failed.");
        }

        std::string itemJson;

        if (!itemWriter.ToJson(itemJson))
        {
            return Fail("Object array encoding failed.");
        }

        archive::JsonArchive itemReader;
        const archive::IArchive& itemReadArchive = itemReader;

        if (!itemReader.Parse(itemJson))
        {
            return Fail("Object array parsing failed.");
        }

        std::size_t itemCount = 0;

        if (!itemReader.GetArraySize("Items", itemCount) || itemCount != sourceItems.size())
        {
            return Fail("Object array size check failed.");
        }

        std::vector<InventoryItem> restoredItems;
        if (!itemReadArchive.ReadObjectArray("Items", restoredItems, ReadInventoryItem))
        {
            return Fail("Object array reading failed.");
        }

        if (restoredItems != sourceItems)
        {
            return Fail("Restored object array values differ.");
        }

        result = itemReader.BeginReadObjectElement("Items", itemCount);

        if (result.Code != archive::ArchiveErrorCode::IndexOutOfRange || itemReader.GetObjectDepth() != 0)
        {
            return Fail("Invalid object array index changed the current path.");
        }

        archive::JsonArchive invalidItemReader;

        if (!invalidItemReader.Parse(R"({"Items":[false]})"))
        {
            return Fail("Cannot prepare the invalid object array.");
        }

        result = invalidItemReader.BeginReadObjectElement("Items", 0);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue || invalidItemReader.GetObjectDepth() != 0)
        {
            return Fail("Invalid object array element changed the current path.");
        }

        std::size_t unchangedSize = 77;
        result = itemReader.GetArraySize("Missing", unchangedSize);

        if (result.Code != archive::ArchiveErrorCode::MissingField || unchangedSize != 77)
        {
            return Fail("Failed array size lookup changed the destination.");
        }

        std::string beforeCallbackFailure;

        if (!itemWriter.ToJson(beforeCallbackFailure))
        {
            return Fail("Cannot capture the object array document.");
        }

        const std::vector<InventoryItem> invalidItems{ { "새 아이템", 3 }, { "잘못된 아이템", -1 } };
        result = itemArchive.WriteObjectArray("Items", invalidItems, WriteInventoryItem);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue || result.Field != "Items[1].Count")
        {
            return Fail("Object array write callback failure check failed.");
        }

        std::string afterCallbackFailure;

        if (!itemWriter.ToJson(afterCallbackFailure) || afterCallbackFailure != beforeCallbackFailure)
        {
            return Fail("Failed object array writing changed the document.");
        }

        const auto unclosedArrayWriter =
            [](archive::IArchive& object, const InventoryItem& item) -> archive::ArchiveResult
            {
                const auto writeResult = WriteInventoryItem(object, item);

                if (!writeResult)
                {
                    return writeResult;
                }

                if (item.Count == 1)
                {
                    return object.BeginWriteObject("Unclosed");
                }

                return {};
            };

        result = itemArchive.WriteObjectArray("Items", sourceItems, unclosedArrayWriter);

        if (result.Code != archive::ArchiveErrorCode::InvalidOperation || result.Field != "Items[1]" ||
            itemArchive.GetObjectDepth() != 0 ||
            !itemWriter.ToJson(afterCallbackFailure) || afterCallbackFailure != beforeCallbackFailure)
        {
            return Fail("Unclosed object array writing changed the document or reported the wrong error.");
        }

        archive::JsonArchive callbackFailureReader;
        const archive::IArchive& callbackFailureArchive = callbackFailureReader;

        if (!callbackFailureReader.Parse(
            R"({"Items":[{"Name":"검","Count":2},{"Name":"방패","Count":"bad"}]})"))
        {
            return Fail("Cannot prepare the object array callback failure.");
        }

        std::vector<InventoryItem> unchangedItems{ { "보존", 77 } };
        result = callbackFailureArchive.ReadObjectArray("Items", unchangedItems, ReadInventoryItem);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue || result.Field != "Items[1].Count" ||
            unchangedItems != std::vector<InventoryItem>{ { "보존", 77 } })
        {
            return Fail("Failed object array reading changed the destination.");
        }

        archive::JsonArchive orderedFailureStorage;

        if (!orderedFailureStorage.Parse(R"({"Items":[{"Name":"바뀜","Count":"bad"},false]})"))
        {
            return Fail("Cannot prepare object array error ordering.");
        }

        const archive::IArchive& orderedFailureArchive = orderedFailureStorage;
        const std::vector<InventoryItem> originalOrderedItems{ { "보존", 77 } };
        auto orderedItems = originalOrderedItems;

        result = orderedFailureArchive.ReadObjectArray("Items", orderedItems, ReadInventoryItem);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue || result.Field != "Items[0].Count" ||
            orderedItems != originalOrderedItems)
        {
            return Fail("Object array reading changed the destination or reported a later error.");
        }

        const std::vector<InventoryItem> emptyItems;

        if (!itemArchive.WriteObjectArray("Items", emptyItems, WriteInventoryItem))
        {
            return Fail("Empty object array writing failed.");
        }

        std::size_t emptyCount = 77;

        if (!itemWriter.GetArraySize("Items", emptyCount) || emptyCount != 0 ||
            !itemArchive.ReadObjectArray("Items", unchangedItems, ReadInventoryItem) || !unchangedItems.empty())
        {
            return Fail("Empty object array restoration failed.");
        }

        std::cout << "JSON object array callback checks passed.\n";

        std::cout << "JSON object array checks passed.\n";
        std::cout << "JSON object array failure checks passed.\n";
        return 0;
    }
}