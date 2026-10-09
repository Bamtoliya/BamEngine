#include <archive/JsonArchive.h>

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>
#include <unordered_set>
#include <array>

namespace
{
    enum class MovementMode : std::uint8_t
    {
        Walk = 0,
        Run = 1
    };

    enum class Permission : std::uint64_t
    {
        None = 0,
        Read = 1,
        Write = 2,
        High = std::uint64_t{ 1 } << 63,
        Maximum = (std::numeric_limits<std::uint64_t>::max)()
    };

    constexpr Permission operator|(Permission lhs, Permission rhs)
    {
        return static_cast<Permission>(static_cast<std::uint64_t>(lhs) | static_cast<std::uint64_t>(rhs));
    }

    constexpr Permission operator&(Permission lhs, Permission rhs)
    {
        return static_cast<Permission>(static_cast<std::uint64_t>(lhs) & static_cast<std::uint64_t>(rhs));
    }

    enum class SignedState : std::int8_t
    {
        Disabled = -1,
        Enabled = 1
    };

    struct InventoryItem
    {
        std::string Name;
        std::int32_t Count = 0;

        bool operator==(const InventoryItem&) const = default;
    };

    archive::ArchiveResult WriteInventoryItem(archive::GlazeArchiveBase& object, const InventoryItem& item)
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

    archive::ArchiveResult ReadInventoryItem(archive::GlazeArchiveBase& object, InventoryItem& item)
    {
        const auto result = object.Read("Name", item.Name);

        if (!result)
        {
            return result;
        }

        return object.Read("Count", item.Count);
    }

    int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }
}

int main()
{
    archive::JsonArchive writer;

    const auto minimum = (std::numeric_limits<std::int64_t>::min)();
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    const std::string expectedName = "속도, \"빠름\"";

    if (!writer.Write("Enabled", true) || !writer.Write("Speed", 25.0f) ||
        !writer.Write("Minimum", minimum) || !writer.Write("Maximum", maximum) ||
        !writer.Write("Name", expectedName) || !writer.Write("Large", std::int16_t{ 300 }))
    {
        return Fail("JSON write checks failed.");
    }

    std::string json;

    if (!writer.ToJson(json))
    {
        return Fail("JSON encoding failed.");
    }

    archive::JsonArchive reader;

    if (!reader.Parse(json))
    {
        return Fail("JSON parsing failed.");
    }

    bool enabled = false;
    float speed = 0.0f;
    std::int64_t restoredMinimum = 0;
    std::uint64_t restoredMaximum = 0;
    std::string name;

    if (!reader.Read("Enabled", enabled) || !reader.Read("Speed", speed) ||
        !reader.Read("Minimum", restoredMinimum) || !reader.Read("Maximum", restoredMaximum) ||
        !reader.Read("Name", name))
    {
        return Fail("JSON read checks failed.");
    }

    if (!enabled || speed != 25.0f || restoredMinimum != minimum ||
        restoredMaximum != maximum || name != expectedName)
    {
        return Fail("JSON round-trip values differ.");
    }

    std::int32_t unchanged = 77;
    auto result = reader.Read("Missing", unchanged);

    if (result.Code != archive::ArchiveErrorCode::MissingField || unchanged != 77)
    {
        return Fail("Missing field check failed.");
    }

    result = reader.Read("Name", unchanged);

    if (result.Code != archive::ArchiveErrorCode::InvalidValue || unchanged != 77)
    {
        return Fail("Type mismatch check failed.");
    }

    std::int8_t small = 7;
    result = reader.Read("Large", small);

    if (result.Code != archive::ArchiveErrorCode::InvalidValue || small != 7)
    {
        return Fail("Integer range check failed.");
    }

    result = reader.Parse(R"({"Speed": 1} trailing)");

    if (result.Code != archive::ArchiveErrorCode::InvalidJson)
    {
        return Fail("Invalid JSON check failed.");
    }

    speed = 0.0f;

    if (!reader.Read("Speed", speed) || speed != 25.0f)
    {
        return Fail("Failed parsing changed the document.");
    }

    result = writer.Write("InvalidNumber", std::numeric_limits<double>::infinity());

    if (result.Code != archive::ArchiveErrorCode::InvalidValue)
    {
        return Fail("Non-finite number check failed.");
    }

#pragma region File 
    const auto dataDirectory = std::filesystem::current_path() / "archive-example";
    std::error_code directoryError;
    std::filesystem::create_directories(dataDirectory, directoryError);

    if (directoryError)
    {
        return Fail("Cannot create the file test directory.");
    }

    const auto filePath = dataDirectory / L"왕복.json";

    if (!writer.SaveToFile(filePath))
    {
        return Fail("JSON file saving failed.");
    }

    archive::JsonArchive fileReader;

    if (!fileReader.LoadFromFile(filePath))
    {
        return Fail("JSON file loading failed.");
    }

    std::string fileName;
    std::int64_t fileMinimum = 0;
    std::uint64_t fileMaximum = 0;

    if (!fileReader.Read("Name", fileName) || !fileReader.Read("Minimum", fileMinimum) ||
        !fileReader.Read("Maximum", fileMaximum))
    {
        return Fail("JSON file value reading failed.");
    }

    if (fileName != expectedName || fileMinimum != minimum || fileMaximum != maximum)
    {
        return Fail("JSON file round-trip values differ.");
    }

    std::string beforeFailure;

    if (!fileReader.ToJson(beforeFailure))
    {
        return Fail("Cannot capture the loaded document.");
    }

    result = fileReader.LoadFromFile(dataDirectory / "missing.json");

    if (result.Code != archive::ArchiveErrorCode::FileOpenFailed)
    {
        return Fail("Missing file check failed.");
    }

    const auto invalidPath = dataDirectory / "invalid.json";
    std::ofstream invalidFile{ invalidPath, std::ios::binary | std::ios::trunc };
    invalidFile << R"({"Speed": 99, "Name":)";
    invalidFile.close();

    if (!invalidFile)
    {
        return Fail("Cannot create the invalid JSON test file.");
    }

    result = fileReader.LoadFromFile(invalidPath);

    if (result.Code != archive::ArchiveErrorCode::InvalidJson)
    {
        return Fail("Invalid JSON file check failed.");
    }

    std::string afterFailure;

    if (!fileReader.ToJson(afterFailure) || afterFailure != beforeFailure)
    {
        return Fail("Failed file loading changed the document.");
    }

    result = writer.SaveToFile(dataDirectory / "missing-directory" / "output.json");

    if (result.Code != archive::ArchiveErrorCode::FileOpenFailed)
    {
        return Fail("Invalid output directory check failed.");
    }
#pragma endregion

#pragma region nested json
    archive::JsonArchive nestedWriter;

    if (!nestedWriter.BeginWriteObject("Player") ||
        !nestedWriter.Write("Name", std::string{ "플레이어" }) ||
        !nestedWriter.BeginWriteObject("Movement") ||
        !nestedWriter.Write("Speed", 12.5f))
    {
        return Fail("Nested object writing failed.");
    }

    if (nestedWriter.GetObjectDepth() != 2)
    {
        return Fail("Nested object depth check failed.");
    }

    std::string rejectedJson = "unchanged";
    result = nestedWriter.ToJson(rejectedJson);

    if (result.Code != archive::ArchiveErrorCode::InvalidOperation || rejectedJson != "unchanged")
    {
        return Fail("Unclosed object encoding check failed.");
    }

    result = nestedWriter.Parse("{}");

    if (result.Code != archive::ArchiveErrorCode::InvalidOperation || nestedWriter.GetObjectDepth() != 2)
    {
        return Fail("Unclosed object parsing check failed.");
    }

    if (!nestedWriter.EndObject() || !nestedWriter.EndObject())
    {
        return Fail("Nested object closing failed.");
    }

    std::string nestedJson;
    const auto nestedPath = dataDirectory / "nested.json";

    if (!nestedWriter.ToJson(nestedJson) || !nestedWriter.SaveToFile(nestedPath))
    {
        return Fail("Nested object saving failed.");
    }

    archive::JsonArchive nestedReader;

    if (!nestedReader.LoadFromFile(nestedPath) || !nestedReader.BeginReadObject("Player"))
    {
        return Fail("Nested object loading failed.");
    }

    std::string playerName;

    if (!nestedReader.Read("Name", playerName) || playerName != "플레이어" ||
        !nestedReader.BeginReadObject("Movement"))
    {
        return Fail("Nested player reading failed.");
    }

    float nestedSpeed = 0.0f;

    if (!nestedReader.Read("Speed", nestedSpeed) || nestedSpeed != 12.5f)
    {
        return Fail("Nested speed reading failed.");
    }

    result = nestedReader.BeginReadObject("Missing");

    if (result.Code != archive::ArchiveErrorCode::MissingField || nestedReader.GetObjectDepth() != 2)
    {
        return Fail("Missing object changed the current path.");
    }

    result = nestedReader.BeginReadObject("Speed");

    if (result.Code != archive::ArchiveErrorCode::InvalidValue || nestedReader.GetObjectDepth() != 2)
    {
        return Fail("Invalid object changed the current path.");
    }

    result = nestedReader.BeginWriteObject("Speed");

    if (result.Code != archive::ArchiveErrorCode::InvalidValue)
    {
        return Fail("Object writing replaced a scalar field.");
    }

    result = nestedReader.LoadFromFile(filePath);

    if (result.Code != archive::ArchiveErrorCode::InvalidOperation || nestedReader.GetObjectDepth() != 2)
    {
        return Fail("File loading replaced a document with an open object.");
    }

    if (!nestedReader.EndObject() || !nestedReader.EndObject())
    {
        return Fail("Nested reading scope closing failed.");
    }

    result = nestedReader.EndObject();

    if (result.Code != archive::ArchiveErrorCode::InvalidOperation || nestedReader.GetObjectDepth() != 0)
    {
        return Fail("Root object closing check failed.");
    }

    std::string restoredNestedJson;

    if (!nestedReader.ToJson(restoredNestedJson) || restoredNestedJson != nestedJson)
    {
        return Fail("Nested object document changed unexpectedly.");
    }

    std::cout << nestedJson << '\n';
#pragma endregion

#pragma region Array 
    archive::JsonArchive arrayWriter;

    if (!arrayWriter.BeginWriteObject("Inventory") ||
        !arrayWriter.WriteArray("Values", std::vector<std::int32_t>{ 9, 8, 7, 6 }) ||
        !arrayWriter.WriteArray("Values", std::vector<std::int32_t>{ 1, 2, 3 }) ||
        !arrayWriter.WriteArray("Names", std::vector<std::string>{ "검", "방패" }) ||
        !arrayWriter.WriteArray("Enabled", std::vector<bool>{ true, false }) ||
        !arrayWriter.WriteArray("Empty", std::vector<float>{}) ||
        !arrayWriter.EndObject())
    {
        return Fail("Array writing failed.");
    }

    std::string arrayJson;

    if (!arrayWriter.ToJson(arrayJson))
    {
        return Fail("Array encoding failed.");
    }

    archive::JsonArchive expectedArray;
    const std::string expectedArrayText =
        R"({"Inventory":{"Values":[1,2,3],"Names":["검","방패"],"Enabled":[true,false],"Empty":[]}})";

    std::string expectedArrayJson;

    if (!expectedArray.Parse(expectedArrayText) || !expectedArray.ToJson(expectedArrayJson) ||
        arrayJson != expectedArrayJson)
    {
        return Fail("Array contents differ.");
    }

    if (!arrayWriter.BeginReadObject("Inventory"))
    {
        return Fail("Cannot enter the array test object.");
    }

    result = arrayWriter.WriteArray("Values", std::vector<double>{
        10.0, std::numeric_limits<double>::infinity()
    });

    if (result.Code != archive::ArchiveErrorCode::InvalidValue || result.Field != "Values")
    {
        return Fail("Invalid array element check failed.");
    }

    if (!arrayWriter.EndObject())
    {
        return Fail("Cannot close the array test object.");
    }

    std::string arrayAfterFailure;

    if (!arrayWriter.ToJson(arrayAfterFailure) || arrayAfterFailure != arrayJson)
    {
        return Fail("Failed array writing changed the document.");
    }

    const auto arrayPath = dataDirectory / "arrays.json";

    if (!arrayWriter.SaveToFile(arrayPath))
    {
        return Fail("Array file saving failed.");
    }

    archive::JsonArchive arrayFileReader;

    if (!arrayFileReader.LoadFromFile(arrayPath))
    {
        return Fail("Array file loading failed.");
    }

    std::string loadedArrayJson;

    if (!arrayFileReader.ToJson(loadedArrayJson) || loadedArrayJson != expectedArrayJson)
    {
        return Fail("Array file contents differ.");
    }

    std::cout << "JSON array file checks passed.\n";

    if (!arrayFileReader.BeginReadObject("Inventory"))
    {
        return Fail("Cannot enter the loaded array object.");
    }

    std::vector<std::int32_t> restoredValues{ 99 };
    std::vector<float> restoredFloats{ -1.0f };
    std::vector<std::string> restoredNames{ "old" };
    std::vector<bool> restoredFlags{ false, true, true };
    std::vector<float> restoredEmpty{ 9.0f };

    if (!arrayFileReader.ReadArray("Values", restoredValues) ||
        !arrayFileReader.ReadArray("Values", restoredFloats) ||
        !arrayFileReader.ReadArray("Names", restoredNames) ||
        !arrayFileReader.ReadArray("Enabled", restoredFlags) ||
        !arrayFileReader.ReadArray("Empty", restoredEmpty))
    {
        return Fail("Array restoration failed.");
    }

    if (restoredValues != std::vector<std::int32_t>{ 1, 2, 3 } ||
        restoredFloats != std::vector<float>{ 1.0f, 2.0f, 3.0f } ||
        restoredNames != std::vector<std::string>{ "검", "방패" } ||
        restoredFlags != std::vector<bool>{ true, false } || !restoredEmpty.empty())
    {
        return Fail("Restored array values differ.");
    }

    if (!arrayFileReader.EndObject())
    {
        return Fail("Cannot close the loaded array object.");
    }

    archive::JsonArchive arrayFailureReader;

    if (!arrayFailureReader.Parse(
        R"({"Large":[1,300],"Mixed":[1,"bad"],"Scalar":4,"Fraction":[1,2.5]})"))
    {
        return Fail("Cannot prepare array failure checks.");
    }

    std::vector<std::int8_t> unchangedArray{ 7, 8 };

    for (const char* key : { "Large", "Mixed", "Scalar", "Fraction" })
    {
        result = arrayFailureReader.ReadArray(key, unchangedArray);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue ||
            result.Field != key || unchangedArray != std::vector<std::int8_t>{ 7, 8 })
        {
            return Fail("Failed array reading changed the destination.");
        }
    }

    result = arrayFailureReader.ReadArray("Missing", unchangedArray);

    if (result.Code != archive::ArchiveErrorCode::MissingField ||
        unchangedArray != std::vector<std::int8_t>{ 7, 8 })
    {
        return Fail("Missing array check failed.");
    }

    std::cout << "JSON array read checks passed.\n";
    std::cout << "JSON array read failure checks passed.\n";

    std::cout << arrayJson << '\n';
#pragma endregion

#pragma region Object Array
    const std::vector<InventoryItem> sourceItems{ { "검", 2 }, { "방패", 1 } };
    archive::JsonArchive itemWriter;

    if (!itemWriter.WriteObjectArray("Items", sourceItems, WriteInventoryItem))
    {
        return Fail("Object array writing failed.");
    }

    const auto itemPath = dataDirectory / "items.json";

    if (!itemWriter.SaveToFile(itemPath))
    {
        return Fail("Object array saving failed.");
    }

    archive::JsonArchive itemReader;

    if (!itemReader.LoadFromFile(itemPath))
    {
        return Fail("Object array loading failed.");
    }

    std::size_t itemCount = 0;

    if (!itemReader.GetArraySize("Items", itemCount) || itemCount != sourceItems.size())
    {
        return Fail("Object array size check failed.");
    }

    std::vector<InventoryItem> restoredItems;
    if (!itemReader.ReadObjectArray("Items", restoredItems, ReadInventoryItem))
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
    result = itemWriter.WriteObjectArray("Items", invalidItems, WriteInventoryItem);

    if (result.Code != archive::ArchiveErrorCode::InvalidValue || result.Field != "Items[1].Count")
    {
        return Fail("Object array write callback failure check failed.");
    }

    std::string afterCallbackFailure;

    if (!itemWriter.ToJson(afterCallbackFailure) || afterCallbackFailure != beforeCallbackFailure)
    {
        return Fail("Failed object array writing changed the document.");
    }

    archive::JsonArchive callbackFailureReader;

    if (!callbackFailureReader.Parse(
        R"({"Items":[{"Name":"검","Count":2},{"Name":"방패","Count":"bad"}]})"))
    {
        return Fail("Cannot prepare the object array callback failure.");
    }

    std::vector<InventoryItem> unchangedItems{ { "보존", 77 } };
    result = callbackFailureReader.ReadObjectArray("Items", unchangedItems, ReadInventoryItem);

    if (result.Code != archive::ArchiveErrorCode::InvalidValue || result.Field != "Items[1].Count" ||
        unchangedItems != std::vector<InventoryItem>{ { "보존", 77 } })
    {
        return Fail("Failed object array reading changed the destination.");
    }

    const std::vector<InventoryItem> emptyItems;

    if (!itemWriter.WriteObjectArray("Items", emptyItems, WriteInventoryItem))
    {
        return Fail("Empty object array writing failed.");
    }

    std::size_t emptyCount = 77;

    if (!itemWriter.GetArraySize("Items", emptyCount) || emptyCount != 0 ||
        !itemWriter.ReadObjectArray("Items", unchangedItems, ReadInventoryItem) || !unchangedItems.empty())
    {
        return Fail("Empty object array restoration failed.");
    }

    std::cout << "JSON object array callback checks passed.\n";

    std::cout << "JSON object array checks passed.\n";
    std::cout << "JSON object array failure checks passed.\n";
#pragma endregion

#pragma region Object
    const InventoryItem featuredItem{ "장검", 5 };
    archive::JsonArchive singleWriter;

    if (!singleWriter.WriteObject("Featured", featuredItem, WriteInventoryItem))
    {
        return Fail("Single object writing failed.");
    }

    const auto singlePath = dataDirectory / "single-item.json";

    if (!singleWriter.SaveToFile(singlePath))
    {
        return Fail("Single object saving failed.");
    }

    archive::JsonArchive singleReader;
    InventoryItem restoredFeatured{ "old", 77 };

    if (!singleReader.LoadFromFile(singlePath) ||
        !singleReader.ReadObject("Featured", restoredFeatured, ReadInventoryItem) ||
        restoredFeatured != featuredItem)
    {
        return Fail("Single object file round-trip failed.");
    }

    std::string beforeSingleFailure;

    if (!singleWriter.ToJson(beforeSingleFailure))
    {
        return Fail("Cannot capture the single object document.");
    }

    const InventoryItem invalidFeatured{ "실패한 변경", -1 };
    result = singleWriter.WriteObject("Featured", invalidFeatured, WriteInventoryItem);

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

    if (!singleFailureReader.Parse(R"({"Featured":{"Name":"바뀜","Count":"bad"},"Scalar":12})"))
    {
        return Fail("Cannot prepare single object failure checks.");
    }

    const InventoryItem originalFeatured{ "보존", 77 };
    InventoryItem unchangedFeatured = originalFeatured;

    result = singleFailureReader.ReadObject("Featured", unchangedFeatured, ReadInventoryItem);

    if (result.Code != archive::ArchiveErrorCode::InvalidValue || result.Field != "Count" ||
        unchangedFeatured != originalFeatured)
    {
        return Fail("Failed single object reading changed the destination.");
    }

    result = singleFailureReader.ReadObject("Missing", unchangedFeatured, ReadInventoryItem);

    if (result.Code != archive::ArchiveErrorCode::MissingField || unchangedFeatured != originalFeatured)
    {
        return Fail("Missing single object check failed.");
    }

    result = singleFailureReader.ReadObject("Scalar", unchangedFeatured, ReadInventoryItem);

    if (result.Code != archive::ArchiveErrorCode::InvalidValue || unchangedFeatured != originalFeatured)
    {
        return Fail("Invalid single object type check failed.");
    }

    const auto unclosedWriter =
        [](archive::GlazeArchiveBase& object, const InventoryItem&) -> archive::ArchiveResult
        {
            return object.BeginWriteObject("Unclosed");
        };

    result = singleWriter.WriteObject("Featured", featuredItem, unclosedWriter);

    if (result.Code != archive::ArchiveErrorCode::InvalidOperation)
    {
        return Fail("Unclosed callback object check failed.");
    }

    if (!singleWriter.ToJson(afterSingleFailure) || afterSingleFailure != beforeSingleFailure)
    {
        return Fail("Unclosed callback changed the document.");
    }

    std::cout << "JSON single object checks passed.\n";
    std::cout << "JSON single object failure checks passed.\n";
#pragma endregion

#pragma region Enum
    archive::JsonArchive enumWriter;
    const auto combinedPermission = Permission::Read | Permission::Write;
    const std::vector<MovementMode> sourceModes{ MovementMode::Walk, MovementMode::Run };

    if (!enumWriter.Write("Mode", MovementMode::Run) ||
        !enumWriter.Write("Flags", combinedPermission) ||
        !enumWriter.Write("Maximum", Permission::Maximum) ||
        !enumWriter.Write("State", SignedState::Disabled) ||
        !enumWriter.WriteArray("Modes", sourceModes))
    {
        return Fail("Enum writing failed.");
    }

    const auto enumPath = dataDirectory / "enums.json";

    if (!enumWriter.SaveToFile(enumPath))
    {
        return Fail("Enum file saving failed.");
    }

    archive::JsonArchive enumReader;
    MovementMode restoredMode = MovementMode::Walk;
    Permission restoredPermission = Permission::None;
    Permission restoredEnumMaximum = Permission::None;
    SignedState restoredState = SignedState::Enabled;
    std::vector<MovementMode> restoredModes;

    if (!enumReader.LoadFromFile(enumPath) ||
        !enumReader.Read("Mode", restoredMode) || !enumReader.Read("Flags", restoredPermission) ||
        !enumReader.Read("Maximum", restoredEnumMaximum) || !enumReader.Read("State", restoredState) ||
        !enumReader.ReadArray("Modes", restoredModes))
    {
        return Fail("Enum file reading failed.");
    }

    if (restoredMode != MovementMode::Run || restoredPermission != combinedPermission ||
        restoredEnumMaximum != Permission::Maximum || restoredState != SignedState::Disabled ||
        restoredModes != sourceModes)
    {
        return Fail("Restored enum values differ.");
    }

    archive::JsonArchive enumFailureReader;

    if (!enumFailureReader.Parse(
        R"({"Large":300,"Negative":-1,"Fraction":1.5,"Text":"Run","Boolean":true,"Modes":[0,300]})"))
    {
        return Fail("Cannot prepare enum failure checks.");
    }

    MovementMode unchangedMode = MovementMode::Run;

    for (const char* key : { "Large", "Negative", "Fraction", "Text", "Boolean" })
    {
        result = enumFailureReader.Read(key, unchangedMode);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue || unchangedMode != MovementMode::Run)
        {
            return Fail("Failed enum reading changed the destination.");
        }
    }

    result = enumFailureReader.Read("Missing", unchangedMode);

    if (result.Code != archive::ArchiveErrorCode::MissingField || unchangedMode != MovementMode::Run)
    {
        return Fail("Missing enum check failed.");
    }

    std::vector<MovementMode> unchangedModes{ MovementMode::Run };
    result = enumFailureReader.ReadArray("Modes", unchangedModes);

    if (result.Code != archive::ArchiveErrorCode::InvalidValue ||
        unchangedModes != std::vector<MovementMode>{ MovementMode::Run })
    {
        return Fail("Failed enum array reading changed the destination.");
    }

    std::cout << "JSON enum checks passed.\n";
    std::cout << "JSON enum failure checks passed.\n";

    const Permission sourceBitFlags = Permission::Read | Permission::Write | Permission::High;
    const std::vector<Permission> sourceFlagSets{
        Permission::None, Permission::Read, Permission::Read | Permission::Write,
        Permission::High | Permission::Read
    };

    archive::JsonArchive flagsWriter;

    if (!flagsWriter.Write("Flags", sourceBitFlags) || !flagsWriter.WriteArray("FlagSets", sourceFlagSets))
    {
        return Fail("Bit flag writing failed.");
    }

    const auto flagsPath = dataDirectory / "flags.json";

    if (!flagsWriter.SaveToFile(flagsPath))
    {
        return Fail("Bit flag file saving failed.");
    }

    archive::JsonArchive flagsReader;
    Permission restoredBitFlags = Permission::None;
    std::vector<Permission> restoredFlagSets;

    if (!flagsReader.LoadFromFile(flagsPath) ||
        !flagsReader.Read("Flags", restoredBitFlags) ||
        !flagsReader.ReadArray("FlagSets", restoredFlagSets))
    {
        return Fail("Bit flag file reading failed.");
    }

    if (restoredBitFlags != sourceBitFlags || restoredFlagSets != sourceFlagSets)
    {
        return Fail("Restored bit flag values differ.");
    }

    if ((restoredBitFlags & Permission::Read) != Permission::Read ||
        (restoredBitFlags & Permission::Write) != Permission::Write ||
        (restoredBitFlags & Permission::High) != Permission::High)
    {
        return Fail("Restored bit flag membership check failed.");
    }

    archive::JsonArchive flagsFailureReader;

    if (!flagsFailureReader.Parse(R"({"Negative":-1,"Fraction":1.5,"Sets":[1,-1]})"))
    {
        return Fail("Cannot prepare bit flag failure checks.");
    }

    Permission unchangedBitFlags = sourceBitFlags;

    for (const char* key : { "Negative", "Fraction" })
    {
        result = flagsFailureReader.Read(key, unchangedBitFlags);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue || unchangedBitFlags != sourceBitFlags)
        {
            return Fail("Failed bit flag reading changed the destination.");
        }
    }

    std::vector<Permission> unchangedFlagSets = sourceFlagSets;
    result = flagsFailureReader.ReadArray("Sets", unchangedFlagSets);

    if (result.Code != archive::ArchiveErrorCode::InvalidValue || unchangedFlagSets != sourceFlagSets)
    {
        return Fail("Failed bit flag array reading changed the destination.");
    }

    std::cout << "JSON bit flag checks passed.\n";
    std::cout << "JSON bit flag failure checks passed.\n";
#pragma endregion

#pragma region Wstring Unicode
    const std::wstring wideName = L"한글 \U0001F600";
    const std::wstring embeddedWide{ L"A\0한글", 4 };
    const std::vector<std::wstring> wideLabels{ L"검", L"방패", L"\U0001F600", L"" };

    archive::JsonArchive wideWriter;

    if (!wideWriter.Write("Name", wideName) || !wideWriter.Write("Embedded", embeddedWide) ||
        !wideWriter.WriteArray("Labels", wideLabels))
    {
        return Fail("Wide string writing failed.");
    }

    std::string wideJson;

    if (!wideWriter.ToJson(wideJson) || wideJson.find('\0') != std::string::npos ||
        wideJson.find(R"(A\u0000한글)") == std::string::npos)
    {
        return Fail("JSON control character escaping failed.");
    }

    const auto widePath = dataDirectory / "wide-strings.json";

    if (!wideWriter.SaveToFile(widePath))
    {
        return Fail("Wide string file saving failed.");
    }

    archive::JsonArchive wideReader;
    std::wstring restoredWideName;
    std::wstring restoredEmbeddedWide;
    std::vector<std::wstring> restoredWideLabels;
    std::string restoredUtf8Name;

    if (!wideReader.LoadFromFile(widePath) ||
        !wideReader.Read("Name", restoredWideName) || !wideReader.Read("Embedded", restoredEmbeddedWide) ||
        !wideReader.ReadArray("Labels", restoredWideLabels) || !wideReader.Read("Name", restoredUtf8Name))
    {
        return Fail("Wide string file reading failed.");
    }

    if (restoredWideName != wideName || restoredEmbeddedWide != embeddedWide ||
        restoredWideLabels != wideLabels || restoredUtf8Name != std::string{ "한글 \U0001F600" })
    {
        return Fail("Restored wide string values differ.");
    }

    std::string beforeWideFailure;

    if (!wideWriter.ToJson(beforeWideFailure))
    {
        return Fail("Cannot capture the wide string document.");
    }

    const std::wstring invalidWide(1, static_cast<wchar_t>(0xD800));
    result = wideWriter.Write("Name", invalidWide);

    if (result.Code != archive::ArchiveErrorCode::InvalidValue)
    {
        return Fail("Invalid wide string check failed.");
    }

    std::string afterWideFailure;

    if (!wideWriter.ToJson(afterWideFailure) || afterWideFailure != beforeWideFailure)
    {
        return Fail("Failed wide string writing changed the document.");
    }

    const std::vector<std::string> invalidUtf8Inputs{
        std::string{ "\xC0\xAF", 2 }, std::string{ "\xE2\x82", 2 },
        std::string{ "\xED\xA0\x80", 3 }, std::string{ "\xF4\x90\x80\x80", 4 }
    };

    archive::JsonArchive invalidWideReader;
    std::wstring unchangedWide = L"보존";

    for (const auto& invalidUtf8 : invalidUtf8Inputs)
    {
        if (!invalidWideReader.Write("Text", invalidUtf8))
        {
            return Fail("Cannot prepare the invalid UTF-8 input.");
        }

        result = invalidWideReader.Read("Text", unchangedWide);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue || unchangedWide != L"보존")
        {
            return Fail("Failed UTF-8 conversion changed the destination.");
        }
    }

    std::cout << "JSON wide string checks passed.\n";
    std::cout << "JSON wide string failure checks passed.\n";
#pragma endregion

#pragma region Set
    const std::unordered_set<std::wstring> sourceTags{ L"플레이어", L"이동", L"\U0001F600" };
    archive::JsonArchive tagsWriter;

    if (!tagsWriter.WriteArray("Tags", sourceTags))
    {
        return Fail("Tag set writing failed.");
    }

    const auto tagsPath = dataDirectory / "tags.json";

    if (!tagsWriter.SaveToFile(tagsPath))
    {
        return Fail("Tag set file saving failed.");
    }

    archive::JsonArchive tagsReader;
    std::unordered_set<std::wstring> restoredTags{ L"old" };

    if (!tagsReader.LoadFromFile(tagsPath) || !tagsReader.ReadArray("Tags", restoredTags) ||
        restoredTags != sourceTags)
    {
        return Fail("Tag set file round-trip failed.");
    }

    archive::JsonArchive tagsFailureReader;

    if (!tagsFailureReader.Parse(
        R"({"Duplicates":["이동","이동"],"Mixed":["이동",7],"Scalar":1,"Empty":[]})"))
    {
        return Fail("Cannot prepare tag set failure checks.");
    }

    std::unordered_set<std::wstring> unchangedTags{ L"보존" };
    const auto originalTags = unchangedTags;

    for (const char* key : { "Duplicates", "Mixed", "Scalar" })
    {
        result = tagsFailureReader.ReadArray(key, unchangedTags);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue || unchangedTags != originalTags)
        {
            return Fail("Failed tag set reading changed the destination.");
        }
    }

    result = tagsFailureReader.ReadArray("Missing", unchangedTags);

    if (result.Code != archive::ArchiveErrorCode::MissingField || unchangedTags != originalTags)
    {
        return Fail("Missing tag set check failed.");
    }

    if (!tagsFailureReader.ReadArray("Empty", unchangedTags) || !unchangedTags.empty())
    {
        return Fail("Empty tag set reading failed.");
    }

    const std::unordered_set<std::wstring> emptyTags;

    if (!tagsWriter.WriteArray("Tags", emptyTags))
    {
        return Fail("Empty tag set writing failed.");
    }

    std::size_t tagCount = 77;

    if (!tagsWriter.GetArraySize("Tags", tagCount) || tagCount != 0)
    {
        return Fail("Empty tag set size check failed.");
    }

    std::cout << "JSON unordered set checks passed.\n";
    std::cout << "JSON unordered set failure checks passed.\n";
#pragma endregion

#pragma region Array
    const std::array<float, 3> sourcePosition{ 1.0f, 2.0f, 3.0f };
    const std::array<float, 4> sourceRotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    const std::array<float, 0> sourceEmptyFixed{};

    archive::JsonArchive fixedWriter;

    if (!fixedWriter.WriteArray("Position", sourcePosition) ||
        !fixedWriter.WriteArray("Rotation", sourceRotation) ||
        !fixedWriter.WriteArray("Empty", sourceEmptyFixed))
    {
        return Fail("Fixed array writing failed.");
    }

    const auto fixedPath = dataDirectory / "fixed-arrays.json";

    if (!fixedWriter.SaveToFile(fixedPath))
    {
        return Fail("Fixed array file saving failed.");
    }

    archive::JsonArchive fixedReader;
    std::array<float, 3> restoredPosition{};
    std::array<float, 4> restoredRotation{};
    std::array<float, 0> restoredEmptyFixed{};

    if (!fixedReader.LoadFromFile(fixedPath) ||
        !fixedReader.ReadArray("Position", restoredPosition) ||
        !fixedReader.ReadArray("Rotation", restoredRotation) ||
        !fixedReader.ReadArray("Empty", restoredEmptyFixed))
    {
        return Fail("Fixed array file reading failed.");
    }

    if (restoredPosition != sourcePosition || restoredRotation != sourceRotation)
    {
        return Fail("Restored fixed array values differ.");
    }

    archive::JsonArchive fixedFailureReader;

    if (!fixedFailureReader.Parse(
        R"({"Short":[1,2],"Long":[1,2,3,4],"Mixed":[1,"bad",3],"Range":[1,1e40,3],"Scalar":1})"))
    {
        return Fail("Cannot prepare fixed array failure checks.");
    }

    std::array<float, 3> unchangedPosition{ 7.0f, 8.0f, 9.0f };
    const auto originalPosition = unchangedPosition;

    for (const char* key : { "Short", "Long", "Mixed", "Range", "Scalar" })
    {
        result = fixedFailureReader.ReadArray(key, unchangedPosition);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue || unchangedPosition != originalPosition)
        {
            return Fail("Failed fixed array reading changed the destination.");
        }
    }

    result = fixedFailureReader.ReadArray("Missing", unchangedPosition);

    if (result.Code != archive::ArchiveErrorCode::MissingField || unchangedPosition != originalPosition)
    {
        return Fail("Missing fixed array check failed.");
    }

    std::cout << "JSON fixed array checks passed.\n";
    std::cout << "JSON fixed array failure checks passed.\n";
#pragma endregion




    std::cout << json << '\n';
    std::cout << "JSON round-trip checks passed.\n";
    std::cout << "JSON failure checks passed.\n";
    std::cout << "JSON file checks passed.\n";
    std::cout << "JSON object checks passed.\n";
    std::cout << "JSON array write checks passed.\n";
    return 0;
}