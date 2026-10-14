#include "TestSupport.h"

#include <archive/JsonArchive.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <unordered_set>

namespace archive_tests
{
    int CheckCommonSets()
    {
        archive::JsonArchive setProbeStorage;

        if (!setProbeStorage.Parse(R"({"Values":[3,1,2],"Duplicates":[1,1],"Empty":[]})"))
        {
            return Fail("Cannot prepare common set checks.");
        }

        archive::IArchive& setProbeArchive = setProbeStorage;
        std::set<std::int32_t> orderedValues{ 9 };

        if (!setProbeArchive.ReadArray("Values", orderedValues) ||
            orderedValues != std::set<std::int32_t>{ 1, 2, 3 })
        {
            return Fail("Common ordered set reading failed.");
        }

        const auto originalOrderedValues = orderedValues;
        const auto duplicateSetResult = setProbeArchive.ReadArray("Duplicates", orderedValues);

        if (duplicateSetResult.Code != archive::ArchiveErrorCode::InvalidValue ||
            orderedValues != originalOrderedValues)
        {
            return Fail("Duplicate ordered set reading changed the destination.");
        }

        if (!setProbeArchive.ReadArray("Empty", orderedValues) || !orderedValues.empty())
        {
            return Fail("Empty ordered set reading failed.");
        }

        std::unordered_set<std::int32_t> hashedValues;
        hashedValues.max_load_factor(0.5f);

        if (!setProbeArchive.ReadArray("Values", hashedValues) || hashedValues.size() != 3 ||
            hashedValues.max_load_factor() != 0.5f)
        {
            return Fail("Unordered set reading changed the load factor policy.");
        }

        return 0;
    }

    int CheckTagSets()
    {
        const std::unordered_set<std::wstring> sourceTags{ L"플레이어", L"이동", L"\U0001F600" };
        archive::JsonArchive tagsWriter;

        if (!tagsWriter.WriteArray("Tags", sourceTags))
        {
            return Fail("Tag set writing failed.");
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
            const auto result = tagsFailureReader.ReadArray(key, unchangedTags);

            if (result.Code != archive::ArchiveErrorCode::InvalidValue || unchangedTags != originalTags)
            {
                return Fail("Failed tag set reading changed the destination.");
            }
        }

        const auto missingResult = tagsFailureReader.ReadArray("Missing", unchangedTags);

        if (missingResult.Code != archive::ArchiveErrorCode::MissingField || unchangedTags != originalTags)
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
        return 0;
    }
}