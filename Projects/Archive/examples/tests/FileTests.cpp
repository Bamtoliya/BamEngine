#include "TestSupport.h"
#include "EnumTestData.h"
#include "ObjectTestData.h"

#include <archive/JsonArchive.h>

#include <iostream>
#include <system_error>

namespace archive_tests
{
    int CheckTagSetFile()
    {
        const auto dataDirectory = std::filesystem::current_path() / "archive-example";
        std::error_code directoryError;
        std::filesystem::create_directories(dataDirectory, directoryError);

        if (directoryError)
        {
            return Fail("Cannot create the tag set file test directory.");
        }

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

        return 0;
    }

    int CheckScalarFile()
    {
        const auto dataDirectory = std::filesystem::current_path() / "archive-example";
        std::error_code directoryError;
        std::filesystem::create_directories(dataDirectory, directoryError);

        if (directoryError)
        {
            return Fail("Cannot create the file test directory.");
        }

        archive::JsonArchive writer;
        const auto minimum = (std::numeric_limits<std::int64_t>::min)();
        const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
        const std::string expectedName = "속도, \"빠름\"";

        if (!writer.Write("Enabled", true) || !writer.Write("Speed", 25.0f) ||
            !writer.Write("Minimum", minimum) || !writer.Write("Maximum", maximum) ||
            !writer.Write("Name", expectedName) || !writer.Write("Large", std::int16_t{ 300 }))
        {
            return Fail("Cannot prepare scalar file test data.");
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

        auto result = fileReader.LoadFromFile(dataDirectory / "missing.json");

        if (result.Code != archive::ArchiveErrorCode::FileOpenFailed)
        {
            return Fail("Missing file check failed.");
        }

        const auto invalidPath = dataDirectory / "invalid.json";
        std::ofstream invalidFile{ invalidPath, std::ios::binary | std::ios::trunc };
        invalidFile << R"({"Speed":99,"Name":)";
        invalidFile.close();

        if (!invalidFile)
        {
            return Fail("Cannot create the invalid JSON test file.");
        }

        result = fileReader.LoadFromFile(invalidPath);

        if (result.Code != archive::ArchiveErrorCode::InvalidDocument)
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

        std::cout << "JSON file checks passed.\n";
        return 0;
    }

    int CheckEnumFile()
    {
        const auto dataDirectory = std::filesystem::current_path() / "archive-example";
        std::error_code directoryError;
        std::filesystem::create_directories(dataDirectory, directoryError);

        if (directoryError)
        {
            return Fail("Cannot create the enum file test directory.");
        }

        archive::JsonArchive writer;
        const auto combinedPermission = Permission::Read | Permission::Write;
        const std::vector<MovementMode> sourceModes{ MovementMode::Walk, MovementMode::Run };

        if (!writer.Write("Mode", MovementMode::Run) || !writer.Write("Flags", combinedPermission) ||
            !writer.Write("Maximum", Permission::Maximum) || !writer.Write("State", SignedState::Disabled) ||
            !writer.WriteArray("Modes", sourceModes))
        {
            return Fail("Enum writing failed.");
        }

        const auto enumPath = dataDirectory / "enums.json";

        if (!writer.SaveToFile(enumPath))
        {
            return Fail("Enum file saving failed.");
        }

        archive::JsonArchive reader;
        MovementMode restoredMode = MovementMode::Walk;
        Permission restoredPermission = Permission::None;
        Permission restoredMaximum = Permission::None;
        SignedState restoredState = SignedState::Enabled;
        std::vector<MovementMode> restoredModes;

        if (!reader.LoadFromFile(enumPath) || !reader.Read("Mode", restoredMode) ||
            !reader.Read("Flags", restoredPermission) || !reader.Read("Maximum", restoredMaximum) ||
            !reader.Read("State", restoredState) || !reader.ReadArray("Modes", restoredModes))
        {
            return Fail("Enum file reading failed.");
        }

        if (restoredMode != MovementMode::Run || restoredPermission != combinedPermission ||
            restoredMaximum != Permission::Maximum || restoredState != SignedState::Disabled ||
            restoredModes != sourceModes)
        {
            return Fail("Restored enum values differ.");
        }

        return 0;
    }

    int CheckBitFlagFile()
    {
        const auto dataDirectory = std::filesystem::current_path() / "archive-example";
        std::error_code directoryError;
        std::filesystem::create_directories(dataDirectory, directoryError);

        if (directoryError)
        {
            return Fail("Cannot create the bit flag file test directory.");
        }

        const Permission sourceFlags = Permission::Read | Permission::Write | Permission::High;
        const std::vector<Permission> sourceSets{
            Permission::None, Permission::Read, Permission::Read | Permission::Write,
            Permission::High | Permission::Read
        };

        archive::JsonArchive writer;

        if (!writer.Write("Flags", sourceFlags) || !writer.WriteArray("FlagSets", sourceSets))
        {
            return Fail("Bit flag writing failed.");
        }

        const auto flagsPath = dataDirectory / "flags.json";

        if (!writer.SaveToFile(flagsPath))
        {
            return Fail("Bit flag file saving failed.");
        }

        archive::JsonArchive reader;
        Permission restoredFlags = Permission::None;
        std::vector<Permission> restoredSets;

        if (!reader.LoadFromFile(flagsPath) || !reader.Read("Flags", restoredFlags) ||
            !reader.ReadArray("FlagSets", restoredSets))
        {
            return Fail("Bit flag file reading failed.");
        }

        if (restoredFlags != sourceFlags || restoredSets != sourceSets)
        {
            return Fail("Restored bit flag values differ.");
        }

        if ((restoredFlags & Permission::Read) != Permission::Read ||
            (restoredFlags & Permission::Write) != Permission::Write ||
            (restoredFlags & Permission::High) != Permission::High)
        {
            return Fail("Restored bit flag membership check failed.");
        }

        return 0;
    }
    int CheckUnicodeFile()
    {
        const auto dataDirectory = std::filesystem::current_path() / "archive-example";
        std::error_code directoryError;
        std::filesystem::create_directories(dataDirectory, directoryError);

        if (directoryError)
        {
            return Fail("Cannot create the Unicode file test directory.");
        }

        const std::wstring wideName = L"한글 \U0001F600";
        const std::wstring embeddedWide{ L"A\0한글", 4 };
        const std::vector<std::wstring> wideLabels{ L"검", L"방패", L"\U0001F600", L"" };
        archive::JsonArchive writer;

        if (!writer.Write("Name", wideName) || !writer.Write("Embedded", embeddedWide) ||
            !writer.WriteArray("Labels", wideLabels))
        {
            return Fail("Wide string writing failed.");
        }

        const auto widePath = dataDirectory / "wide-strings.json";

        if (!writer.SaveToFile(widePath))
        {
            return Fail("Wide string file saving failed.");
        }

        archive::JsonArchive reader;
        std::wstring restoredWideName;
        std::wstring restoredEmbeddedWide;
        std::vector<std::wstring> restoredWideLabels;
        std::string restoredUtf8Name;

        if (!reader.LoadFromFile(widePath) ||
            !reader.Read("Name", restoredWideName) || !reader.Read("Embedded", restoredEmbeddedWide) ||
            !reader.ReadArray("Labels", restoredWideLabels) || !reader.Read("Name", restoredUtf8Name))
        {
            return Fail("Wide string file reading failed.");
        }

        if (restoredWideName != wideName || restoredEmbeddedWide != embeddedWide ||
            restoredWideLabels != wideLabels || restoredUtf8Name != std::string{ "한글 \U0001F600" })
        {
            return Fail("Restored wide string values differ.");
        }

        return 0;
    }

    int CheckDynamicArrayFile()
    {
        const auto dataDirectory = std::filesystem::current_path() / "archive-example";
        std::error_code directoryError;
        std::filesystem::create_directories(dataDirectory, directoryError);

        if (directoryError)
        {
            return Fail("Cannot create the array file test directory.");
        }

        archive::JsonArchive writer;

        if (!writer.BeginWriteObject("Inventory") ||
            !writer.WriteArray("Values", std::vector<std::int32_t>{ 1, 2, 3 }) ||
            !writer.WriteArray("Names", std::vector<std::string>{ "검", "방패" }) ||
            !writer.WriteArray("Enabled", std::vector<bool>{ true, false }) ||
            !writer.WriteArray("Empty", std::vector<float>{}) || !writer.EndObject())
        {
            return Fail("Array writing failed.");
        }

        const auto arrayPath = dataDirectory / "arrays.json";

        if (!writer.SaveToFile(arrayPath))
        {
            return Fail("Array file saving failed.");
        }

        archive::JsonArchive reader;

        if (!reader.LoadFromFile(arrayPath))
        {
            return Fail("Array file loading failed.");
        }

        archive::JsonArchive expected;
        const std::string expectedText =
            R"({"Inventory":{"Values":[1,2,3],"Names":["검","방패"],"Enabled":[true,false],"Empty":[]}})";
        std::string expectedJson;
        std::string loadedJson;

        if (!expected.Parse(expectedText) || !expected.ToJson(expectedJson) ||
            !reader.ToJson(loadedJson) || loadedJson != expectedJson)
        {
            return Fail("Array file contents differ.");
        }

        if (!reader.BeginReadObject("Inventory"))
        {
            return Fail("Cannot enter the loaded array object.");
        }

        std::vector<std::int32_t> restoredValues{ 99 };
        std::vector<float> restoredFloats{ -1.0f };
        std::vector<std::string> restoredNames{ "old" };
        std::vector<bool> restoredFlags{ false, true, true };
        std::vector<float> restoredEmpty{ 9.0f };

        if (!reader.ReadArray("Values", restoredValues) || !reader.ReadArray("Values", restoredFloats) ||
            !reader.ReadArray("Names", restoredNames) || !reader.ReadArray("Enabled", restoredFlags) ||
            !reader.ReadArray("Empty", restoredEmpty))
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

        if (!reader.EndObject())
        {
            return Fail("Cannot close the loaded array object.");
        }

        std::cout << "JSON array file checks passed.\n";
        return 0;
    }

    int CheckFixedArrayFile()
    {
        const auto dataDirectory = std::filesystem::current_path() / "archive-example";
        std::error_code directoryError;
        std::filesystem::create_directories(dataDirectory, directoryError);

        if (directoryError)
        {
            return Fail("Cannot create the fixed array file test directory.");
        }

        const std::array<float, 3> sourcePosition{ 1.0f, 2.0f, 3.0f };
        const std::array<float, 4> sourceRotation{ 0.0f, 0.0f, 0.0f, 1.0f };
        const std::array<float, 0> sourceEmpty{};
        archive::JsonArchive writer;

        if (!writer.WriteArray("Position", sourcePosition) || !writer.WriteArray("Rotation", sourceRotation) ||
            !writer.WriteArray("Empty", sourceEmpty))
        {
            return Fail("Fixed array writing failed.");
        }

        const auto fixedPath = dataDirectory / "fixed-arrays.json";

        if (!writer.SaveToFile(fixedPath))
        {
            return Fail("Fixed array file saving failed.");
        }

        archive::JsonArchive reader;
        std::array<float, 3> restoredPosition{};
        std::array<float, 4> restoredRotation{};
        std::array<float, 0> restoredEmpty{};

        if (!reader.LoadFromFile(fixedPath) || !reader.ReadArray("Position", restoredPosition) ||
            !reader.ReadArray("Rotation", restoredRotation) || !reader.ReadArray("Empty", restoredEmpty))
        {
            return Fail("Fixed array file reading failed.");
        }

        if (restoredPosition != sourcePosition || restoredRotation != sourceRotation)
        {
            return Fail("Restored fixed array values differ.");
        }

        return 0;
    }

    int CheckNestedObjectFile()
    {
        const auto directory = std::filesystem::current_path() / "archive-example";
        std::error_code error;
        std::filesystem::create_directories(directory, error);

        if (error)
        {
            return Fail("Cannot create the nested object file test directory.");
        }

        archive::JsonArchive writer;

        if (!writer.BeginWriteObject("Player") ||
            !writer.Write("Name", std::string{ "플레이어" }) ||
            !writer.BeginWriteObject("Movement") ||
            !writer.Write("Speed", 12.5f) ||
            !writer.EndObject() || !writer.EndObject())
        {
            return Fail("Cannot prepare the nested object file.");
        }

        const auto path = directory / "nested.json";
        std::string expectedJson;

        if (!writer.ToJson(expectedJson) || !writer.SaveToFile(path))
        {
            return Fail("Nested object file saving failed.");
        }

        archive::JsonArchive replacement;
        const auto replacementPath = directory / "nested-replacement.json";

        if (!replacement.Write("Replacement", 99) || !replacement.SaveToFile(replacementPath))
        {
            return Fail("Cannot prepare the replacement document file.");
        }

        archive::JsonArchive reader;

        if (!reader.LoadFromFile(path) || !reader.BeginReadObject("Player"))
        {
            return Fail("Nested object file loading failed.");
        }

        std::string name;
        float speed = 0.0f;

        if (!reader.Read("Name", name) || name != "플레이어" ||
            !reader.BeginReadObject("Movement") ||
            !reader.Read("Speed", speed) || speed != 12.5f)
        {
            return Fail("Nested object file values differ.");
        }

        const auto result = reader.LoadFromFile(replacementPath);

        if (result.Code != archive::ArchiveErrorCode::InvalidOperation || reader.GetObjectDepth() != 2)
        {
            return Fail("File loading replaced a document with an open object.");
        }

        if (!reader.EndObject() || !reader.EndObject())
        {
            return Fail("Nested object file reading scope closing failed.");
        }

        std::string restoredJson;

        if (!reader.ToJson(restoredJson) || restoredJson != expectedJson)
        {
            return Fail("Rejected file loading changed the nested document.");
        }

        std::cout << "JSON nested object file checks passed.\n";
        return 0;
    }

    int CheckObjectArrayFile()
    {
        const auto directory = std::filesystem::current_path() / "archive-example";
        std::error_code error;
        std::filesystem::create_directories(directory, error);

        if (error)
        {
            return Fail("Cannot create the object array file test directory.");
        }

        const std::vector<InventoryItem> source{ { "검", 2 }, { "방패", 1 } };
        archive::JsonArchive writer;
        archive::IArchive& writeArchive = writer;
        const auto path = directory / "items.json";

        if (!writeArchive.WriteObjectArray("Items", source, WriteInventoryItem) || !writer.SaveToFile(path))
        {
            return Fail("Object array file saving failed.");
        }

        archive::JsonArchive reader;
        const archive::IArchive& readArchive = reader;

        if (!reader.LoadFromFile(path))
        {
            return Fail("Object array file loading failed.");
        }

        std::size_t count = 0;
        std::vector<InventoryItem> restored;

        if (!reader.GetArraySize("Items", count) || count != source.size() ||
            !readArchive.ReadObjectArray("Items", restored, ReadInventoryItem) || restored != source)
        {
            return Fail("Object array file round-trip failed.");
        }

        std::cout << "JSON object array file checks passed.\n";
        return 0;
    }

    int CheckSingleObjectFile()
    {
        const auto directory = std::filesystem::current_path() / "archive-example";
        std::error_code error;
        std::filesystem::create_directories(directory, error);

        if (error)
        {
            return Fail("Cannot create the single object file test directory.");
        }

        const InventoryItem source{ "장검", 5 };
        archive::JsonArchive writer;
        archive::IArchive& writeArchive = writer;
        const auto path = directory / "single-item.json";

        if (!writeArchive.WriteObject("Featured", source, WriteInventoryItem) || !writer.SaveToFile(path))
        {
            return Fail("Single object file saving failed.");
        }

        archive::JsonArchive reader;
        const archive::IArchive& readArchive = reader;
        InventoryItem restored{ "old", 77 };

        if (!reader.LoadFromFile(path) ||
            !readArchive.ReadObject("Featured", restored, ReadInventoryItem) || restored != source)
        {
            return Fail("Single object file round-trip failed.");
        }

        std::cout << "JSON single object file checks passed.\n";
        return 0;
    }
}