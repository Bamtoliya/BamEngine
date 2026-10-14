#include "TestSupport.h"

#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include "ProfileTestData.h"

#include <reflection/Registry.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace archive_reflection_tests
{
    namespace
    {
        struct SerializationSettings
        {
            std::int32_t Level = 3;
            double CachedSpeed = std::numeric_limits<double>::infinity();
            std::vector<std::int32_t> Scratch{ 1, 2, 3 };
        };

        bool SameSerializationSettings(const SerializationSettings& lhs, const SerializationSettings& rhs)
        {
            return lhs.Level == rhs.Level && lhs.CachedSpeed == rhs.CachedSpeed && lhs.Scratch == rhs.Scratch;
        }
    }

    int CheckSerializationPolicy(const std::filesystem::path& directory)
    {
        auto type = reflection::TypeInfo::For<SerializationSettings>("example::SerializationSettings");
        auto level = reflection::MakeProperty<&SerializationSettings::Level>("Level", "std::int32_t");
        auto cache = reflection::MakeProperty<&SerializationSettings::CachedSpeed>("CachedSpeed", "double");
        auto scratch = reflection::MakeProperty<&SerializationSettings::Scratch>(
            "Scratch", "std::vector<std::int32_t>");

        if (!level.Metadata.Add("Serialize", true) ||
            !cache.Metadata.Add("Serialize", false) || !scratch.Metadata.Add("Serialize", false))
        {
            return Fail("Serialization metadata setup failed.");
        }

        type.Properties.push_back(std::move(level));
        type.Properties.push_back(std::move(cache));
        type.Properties.push_back(std::move(scratch));

        const SerializationSettings source;
        archive::JsonArchive writer;

        if (!archive_reflection::WriteObject(writer, "Policy", type, reflection::ObjectView::From(source)))
        {
            return Fail("Excluded properties prevented writing.");
        }

        const auto path = directory / "reflection-policy.json";
        archive::JsonArchive reader;
        SerializationSettings restored;
        restored.Level = -1;
        restored.CachedSpeed = 42.0;
        restored.Scratch = { 9 };

        if (!writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Policy", type, restored) ||
            restored.Level != source.Level || restored.CachedSpeed != 42.0 ||
            restored.Scratch != std::vector<std::int32_t>{ 9 })
        {
            return Fail("Serialization policy round-trip failed.");
        }

        if (!reader.BeginReadObject("Policy"))
        {
            return Fail("Cannot inspect the policy document.");
        }

        double excludedCache{};
        std::vector<std::int32_t> excludedScratch;
        const auto cacheResult = reader.Read("CachedSpeed", excludedCache);
        const auto scratchResult = reader.ReadArray("Scratch", excludedScratch);
        const auto endResult = reader.EndObject();

        using Code = archive::ArchiveErrorCode;

        if (cacheResult.Code != Code::MissingField || scratchResult.Code != Code::MissingField || !endResult)
        {
            return Fail("Excluded properties were written.");
        }

        archive::JsonArchive ignoredReader;

        if (!ignoredReader.Parse(R"({"Policy":{"Level":99,"CachedSpeed":"bad","Scratch":17}})") ||
            !archive_reflection::ReadObject(ignoredReader, "Policy", type, restored) ||
            restored.Level != 99 || restored.CachedSpeed != 42.0 ||
            restored.Scratch != std::vector<std::int32_t>{ 9 })
        {
            return Fail("Excluded JSON properties were not ignored.");
        }

        auto invalidType = type;
        invalidType.Properties[1].Metadata = reflection::Metadata{};

        if (!invalidType.Properties[1].Metadata.Add("Serialize", std::string{ "false" }))
        {
            return Fail("Cannot prepare invalid serialization metadata.");
        }

        restored.Level = 77;
        const SerializationSettings beforeRead = restored;
        const auto readResult = archive_reflection::ReadObject(ignoredReader, "Policy", invalidType, restored);

        if (readResult.Code != Code::InvalidValue || readResult.Field != "CachedSpeed" ||
            !SameSerializationSettings(restored, beforeRead))
        {
            return Fail("Invalid serialization metadata changed the destination.");
        }

        std::string beforeWrite;

        if (!writer.ToJson(beforeWrite))
        {
            return Fail("Cannot capture the policy document.");
        }

        SerializationSettings changed = source;
        changed.Level = 99;
        const auto writeResult = archive_reflection::WriteObject(
            writer, "Policy", invalidType, reflection::ObjectView::From(changed));

        std::string afterWrite;

        if (writeResult.Code != Code::InvalidValue || writeResult.Field != "CachedSpeed" ||
            !writer.ToJson(afterWrite) || afterWrite != beforeWrite)
        {
            return Fail("Invalid serialization metadata changed the document.");
        }

        std::cout << beforeWrite << '\n';
        std::cout << "Reflection serialization policy checks passed.\n";
        std::cout << "Reflection serialization policy failure checks passed.\n";
        return 0;
    }

    int CheckOptionalFields(reflection::Registry& registry, const std::filesystem::path& directory)
    {
        auto type = reflection::TypeInfo::For<ProfileData>("example::ProfileData");
        auto options = reflection::MakeProperty<&ProfileData::Options>("Options", "Settings");
        auto level = reflection::MakeProperty<&ProfileData::Level>("Level", "std::int32_t");

        if (!options.Metadata.Add("Optional", true) || !level.Metadata.Add("Optional", true))
        {
            return Fail("Optional metadata setup failed.");
        }

        type.Properties.push_back(reflection::MakeProperty<&ProfileData::Name>("Name", "std::string"));
        type.Properties.push_back(std::move(options));
        type.Properties.push_back(std::move(level));

        const ProfileData source;
        ProfileData initial;
        initial.Name = "Before";
        initial.Options.Speed = -7.0f;
        initial.Options.Title = L"Before";
        initial.Level = -1;

        archive::JsonArchive writer;
        archive::JsonArchive reader;
        const auto path = directory / "reflection-optional.json";
        ProfileData restored = initial;

        if (!archive_reflection::WriteObject(
            writer, "Compatible", type, reflection::ObjectView::From(source), &registry) ||
            !writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Compatible", type, restored, &registry) ||
            !SameProfile(restored, source))
        {
            return Fail("Optional field round-trip failed.");
        }

        archive::JsonArchive legacyReader;

        if (!legacyReader.Parse(R"({"Compatible":{"Name":"Legacy"}})"))
        {
            return Fail("Cannot prepare the legacy document.");
        }

        restored = initial;
        ProfileData expected = initial;
        expected.Name = "Legacy";

        if (!archive_reflection::ReadObject(legacyReader, "Compatible", type, restored, &registry) ||
            !SameProfile(restored, expected))
        {
            return Fail("Missing optional fields did not preserve existing values.");
        }

        using Code = archive::ArchiveErrorCode;

        struct FailureCase
        {
            const char* Json;
            const char* Field;
            Code Expected;
        };

        const FailureCase failures[] = {
            {
                R"({"Compatible":{"Name":"Changed","Options":null}})",
                "Options", Code::InvalidValue
            },
            {
                R"({"Compatible":{"Name":"Changed","Options":{}}})",
                "Options.Enabled", Code::MissingField
            },
            {
                R"({"Compatible":{"Name":"Changed","Level":"bad"}})",
                "Level", Code::InvalidValue
            },
            {
                R"({"Compatible":{"Name":"Changed","Level":null}})",
                "Level", Code::InvalidValue
            },
            {
                R"({"Compatible":{"Level":9}})",
                "Name", Code::MissingField
            }
        };

        for (const auto& failure : failures)
        {
            archive::JsonArchive failureReader;

            if (!failureReader.Parse(failure.Json))
            {
                return Fail("Cannot prepare optional field failure.");
            }

            restored = initial;
            const auto result = archive_reflection::ReadObject(
                failureReader, "Compatible", type, restored, &registry);

            if (result.Code != failure.Expected || result.Field != failure.Field ||
                !SameProfile(restored, initial))
            {
                return Fail("Failed optional field reading changed the destination.");
            }
        }

        auto requiredType = type;
        requiredType.Properties[1].Metadata = reflection::Metadata{};
        restored = initial;

        const auto requiredResult = archive_reflection::ReadObject(
            legacyReader, "Compatible", requiredType, restored, &registry);

        if (requiredResult.Code != Code::MissingField || requiredResult.Field != "Options" ||
            !SameProfile(restored, initial))
        {
            return Fail("Unmarked fields were not required.");
        }

        auto invalidType = type;
        invalidType.Properties[1].Metadata = reflection::Metadata{};

        if (!invalidType.Properties[1].Metadata.Add("Optional", std::string{ "true" }))
        {
            return Fail("Cannot prepare invalid optional metadata.");
        }

        restored = initial;
        const auto invalidResult = archive_reflection::ReadObject(
            legacyReader, "Compatible", invalidType, restored, &registry);

        if (invalidResult.Code != Code::InvalidValue || invalidResult.Field != "Options" ||
            !SameProfile(restored, initial))
        {
            return Fail("Invalid optional metadata changed the destination.");
        }

        std::cout << "Reflection optional field checks passed.\n";
        std::cout << "Reflection optional field failure checks passed.\n";
        return 0;
    }
}