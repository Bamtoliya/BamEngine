#include "TestSupport.h"

#include <archive/JsonArchive.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace archive_tests
{
    int CheckDynamicArrays()
    {
        archive::JsonArchive writer;

        if (!writer.BeginWriteObject("Inventory") ||
            !writer.WriteArray("Values", std::vector<std::int32_t>{ 9, 8, 7, 6 }) ||
            !writer.WriteArray("Values", std::vector<std::int32_t>{ 1, 2, 3 }) ||
            !writer.WriteArray("Names", std::vector<std::string>{ "검", "방패" }) ||
            !writer.WriteArray("Enabled", std::vector<bool>{ true, false }) ||
            !writer.WriteArray("Empty", std::vector<float>{}) || !writer.EndObject())
        {
            return Fail("Array writing failed.");
        }

        std::string json;

        if (!writer.ToJson(json))
        {
            return Fail("Array encoding failed.");
        }

        archive::JsonArchive expected;
        const std::string expectedText =
            R"({"Inventory":{"Values":[1,2,3],"Names":["검","방패"],"Enabled":[true,false],"Empty":[]}})";
        std::string expectedJson;

        if (!expected.Parse(expectedText) || !expected.ToJson(expectedJson) || json != expectedJson)
        {
            return Fail("Array contents differ.");
        }

        if (!writer.BeginReadObject("Inventory"))
        {
            return Fail("Cannot enter the array test object.");
        }

        const std::vector<double> invalidValues{ 10.0, std::numeric_limits<double>::infinity() };
        const auto writeFailure = writer.WriteArray("Values", invalidValues);

        if (writeFailure.Code != archive::ArchiveErrorCode::InvalidValue || writeFailure.Field != "Values")
        {
            return Fail("Invalid array element check failed.");
        }

        if (!writer.EndObject())
        {
            return Fail("Cannot close the array test object.");
        }

        std::string afterFailure;

        if (!writer.ToJson(afterFailure) || afterFailure != json)
        {
            return Fail("Failed array writing changed the document.");
        }

        archive::JsonArchive reader;

        if (!reader.Parse(json) || !reader.BeginReadObject("Inventory"))
        {
            return Fail("Cannot prepare array restoration.");
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
            return Fail("Cannot close the array test object.");
        }

        archive::JsonArchive failureReader;

        if (!failureReader.Parse(R"({"Large":[1,300],"Mixed":[1,"bad"],"Scalar":4,"Fraction":[1,2.5]})"))
        {
            return Fail("Cannot prepare array failure checks.");
        }

        const std::vector<std::int8_t> original{ 7, 8 };
        auto unchanged = original;

        for (const char* key : { "Large", "Mixed", "Scalar", "Fraction" })
        {
            const auto result = failureReader.ReadArray(key, unchanged);

            if (result.Code != archive::ArchiveErrorCode::InvalidValue ||
                result.Field != key || unchanged != original)
            {
                return Fail("Failed array reading changed the destination.");
            }
        }

        const auto missingResult = failureReader.ReadArray("Missing", unchanged);

        if (missingResult.Code != archive::ArchiveErrorCode::MissingField || unchanged != original)
        {
            return Fail("Missing array check failed.");
        }

        std::cout << json << '\n';
        std::cout << "JSON array write checks passed.\n";
        std::cout << "JSON array read checks passed.\n";
        std::cout << "JSON array read failure checks passed.\n";
        return 0;
    }

    int CheckFixedArrays()
    {
        const std::array<float, 3> sourcePosition{ 1.0f, 2.0f, 3.0f };
        const std::array<float, 4> sourceRotation{ 0.0f, 0.0f, 0.0f, 1.0f };
        const std::array<float, 0> sourceEmpty{};

        archive::JsonArchive storage;
        archive::IArchive& ar = storage;

        if (!ar.WriteArray("Position", sourcePosition) || !ar.WriteArray("Rotation", sourceRotation) ||
            !ar.WriteArray("Empty", sourceEmpty))
        {
            return Fail("Fixed array writing failed.");
        }

        std::array<float, 3> restoredPosition{};
        std::array<float, 4> restoredRotation{};
        std::array<float, 0> restoredEmpty{};

        if (!ar.ReadArray("Position", restoredPosition) || !ar.ReadArray("Rotation", restoredRotation) ||
            !ar.ReadArray("Empty", restoredEmpty))
        {
            return Fail("Fixed array reading failed.");
        }

        if (restoredPosition != sourcePosition || restoredRotation != sourceRotation)
        {
            return Fail("Restored fixed array values differ.");
        }

        archive::JsonArchive failureReader;

        if (!failureReader.Parse(
            R"({"Short":[1,2],"Long":[1,2,3,4],"Mixed":[1,"bad",3],"Range":[1,1e40,3],"Scalar":1})"))
        {
            return Fail("Cannot prepare fixed array failure checks.");
        }

        const std::array<float, 3> original{ 7.0f, 8.0f, 9.0f };
        auto unchanged = original;

        for (const char* key : { "Short", "Long", "Mixed", "Range", "Scalar" })
        {
            const auto result = failureReader.ReadArray(key, unchanged);

            if (result.Code != archive::ArchiveErrorCode::InvalidValue || unchanged != original)
            {
                return Fail("Failed fixed array reading changed the destination.");
            }
        }

        const auto missingResult = failureReader.ReadArray("Missing", unchanged);

        if (missingResult.Code != archive::ArchiveErrorCode::MissingField || unchanged != original)
        {
            return Fail("Missing fixed array check failed.");
        }

        std::cout << "JSON fixed array checks passed.\n";
        std::cout << "JSON fixed array failure checks passed.\n";
        return 0;
    }
}