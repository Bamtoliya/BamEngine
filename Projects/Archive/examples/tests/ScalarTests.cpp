#include "TestSupport.h"
#include "EnumTestData.h"

#include <archive/JsonArchive.h>

#include <iostream>

namespace archive_tests
{
    int CheckCommonScalarReads()
    {
        archive::JsonArchive storage;

        if (!storage.Parse(R"({"Range":1e40,"Boolean":true})"))
        {
            return Fail("Cannot prepare common scalar read checks.");
        }

        archive::IArchive& ar = storage;
        float unchangedFloat = 7.0f;
        std::int32_t unchangedInteger = 7;

        if (ar.Read("Range", unchangedFloat).Code != archive::ArchiveErrorCode::InvalidValue ||
            unchangedFloat != 7.0f)
        {
            return Fail("Failed common float reading changed the destination.");
        }

        if (ar.Read("Boolean", unchangedInteger).Code != archive::ArchiveErrorCode::InvalidValue ||
            unchangedInteger != 7)
        {
            return Fail("Common scalar reading converted bool to an integer.");
        }

        return 0;
    }

    int CheckScalarRoundTrip()
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

        result = writer.Write("InvalidNumber", std::numeric_limits<double>::infinity());

        if (result.Code != archive::ArchiveErrorCode::InvalidValue)
        {
            return Fail("Non-finite number check failed.");
        }

        std::cout << json << '\n';
        std::cout << "JSON round-trip checks passed.\n";
        return 0;
    }
    int CheckEnumValues()
    {
        using Code = archive::ArchiveErrorCode;

        archive::JsonArchive storage;
        archive::IArchive& ar = storage;
        const auto combinedPermission = Permission::Read | Permission::Write;
        const std::vector<MovementMode> sourceModes{ MovementMode::Walk, MovementMode::Run };

        if (!ar.Write("Mode", MovementMode::Run) || !ar.Write("Flags", combinedPermission) ||
            !ar.Write("Maximum", Permission::Maximum) || !ar.Write("State", SignedState::Disabled) ||
            !ar.WriteArray("Modes", sourceModes))
        {
            return Fail("Enum writing failed.");
        }

        MovementMode restoredMode = MovementMode::Walk;
        Permission restoredPermission = Permission::None;
        Permission restoredMaximum = Permission::None;
        SignedState restoredState = SignedState::Enabled;
        std::vector<MovementMode> restoredModes;

        if (!ar.Read("Mode", restoredMode) || !ar.Read("Flags", restoredPermission) ||
            !ar.Read("Maximum", restoredMaximum) || !ar.Read("State", restoredState) ||
            !ar.ReadArray("Modes", restoredModes))
        {
            return Fail("Enum reading failed.");
        }

        if (restoredMode != MovementMode::Run || restoredPermission != combinedPermission ||
            restoredMaximum != Permission::Maximum || restoredState != SignedState::Disabled ||
            restoredModes != sourceModes)
        {
            return Fail("Restored enum values differ.");
        }

        archive::JsonArchive failureReader;

        if (!failureReader.Parse(
            R"({"Large":300,"Negative":-1,"Fraction":1.5,"Text":"Run","Boolean":true,"Modes":[0,300]})"))
        {
            return Fail("Cannot prepare enum failure checks.");
        }

        MovementMode unchangedMode = MovementMode::Run;

        for (const char* key : { "Large", "Negative", "Fraction", "Text", "Boolean" })
        {
            const auto result = failureReader.Read(key, unchangedMode);

            if (result.Code != Code::InvalidValue || unchangedMode != MovementMode::Run)
            {
                return Fail("Failed enum reading changed the destination.");
            }
        }

        const auto missingResult = failureReader.Read("Missing", unchangedMode);

        if (missingResult.Code != Code::MissingField || unchangedMode != MovementMode::Run)
        {
            return Fail("Missing enum check failed.");
        }

        std::vector<MovementMode> unchangedModes{ MovementMode::Run };
        const auto arrayResult = failureReader.ReadArray("Modes", unchangedModes);

        if (arrayResult.Code != Code::InvalidValue ||
            unchangedModes != std::vector<MovementMode>{ MovementMode::Run })
        {
            return Fail("Failed enum array reading changed the destination.");
        }

        std::cout << "JSON enum checks passed.\n";
        std::cout << "JSON enum failure checks passed.\n";
        return 0;
    }

    int CheckBitFlags()
    {
        using Code = archive::ArchiveErrorCode;

        const Permission sourceFlags = Permission::Read | Permission::Write | Permission::High;
        const std::vector<Permission> sourceSets{
            Permission::None, Permission::Read, Permission::Read | Permission::Write,
            Permission::High | Permission::Read
        };

        archive::JsonArchive storage;
        archive::IArchive& ar = storage;

        if (!ar.Write("Flags", sourceFlags) || !ar.WriteArray("FlagSets", sourceSets))
        {
            return Fail("Bit flag writing failed.");
        }

        Permission restoredFlags = Permission::None;
        std::vector<Permission> restoredSets;

        if (!ar.Read("Flags", restoredFlags) || !ar.ReadArray("FlagSets", restoredSets) ||
            restoredFlags != sourceFlags || restoredSets != sourceSets)
        {
            return Fail("Restored bit flag values differ.");
        }

        archive::JsonArchive failureReader;

        if (!failureReader.Parse(R"({"Negative":-1,"Fraction":1.5,"Sets":[1,-1]})"))
        {
            return Fail("Cannot prepare bit flag failure checks.");
        }

        Permission unchangedFlags = sourceFlags;

        for (const char* key : { "Negative", "Fraction" })
        {
            const auto result = failureReader.Read(key, unchangedFlags);

            if (result.Code != Code::InvalidValue || unchangedFlags != sourceFlags)
            {
                return Fail("Failed bit flag reading changed the destination.");
            }
        }

        auto unchangedSets = sourceSets;
        const auto arrayResult = failureReader.ReadArray("Sets", unchangedSets);

        if (arrayResult.Code != Code::InvalidValue || unchangedSets != sourceSets)
        {
            return Fail("Failed bit flag array reading changed the destination.");
        }

        std::cout << "JSON bit flag checks passed.\n";
        std::cout << "JSON bit flag failure checks passed.\n";
        return 0;
    }
}