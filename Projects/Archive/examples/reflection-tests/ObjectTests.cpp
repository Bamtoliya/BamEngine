#include "TestSupport.h"

#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include "SettingsTestData.h"
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
        struct UnsupportedSettings
        {
            float Speed = 99.0f;
            std::vector<long double> Values{ 1.0L, 2.0L, 3.0L };
        };
    }

    int CheckSettingsArchive(reflection::Registry& registry, const std::filesystem::path& directory)
    {
        const auto* type = registry.FindType<Settings>();

        if (type == nullptr)
        {
            return Fail("Settings type lookup failed.");
        }

        const Settings source;
        archive::JsonArchive writer;

        if (!archive_reflection::WriteObject(writer, "Settings", *type, reflection::ObjectView::From(source)))
        {
            return Fail("Reflected object writing failed.");
        }

        const auto path = directory / "reflection-settings.json";

        if (!writer.SaveToFile(path))
        {
            return Fail("Reflected object file saving failed.");
        }

        archive::JsonArchive reader;
        Settings restored;
        restored.Enabled = false;
        restored.Speed = -123.0f;
        restored.Minimum = 0;
        restored.Maximum = 0;
        restored.Name = "Before";
        restored.Title = L"Before";

        if (!reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Settings", *type, restored))
        {
            return Fail("Reflected object file reading failed.");
        }

        if (restored.Enabled != source.Enabled || restored.Speed != source.Speed ||
            restored.Minimum != source.Minimum || restored.Maximum != source.Maximum ||
            restored.Name != source.Name || restored.Title != source.Title)
        {
            return Fail("Saved reflected values differ.");
        }

        std::string beforeFailure;

        if (!writer.ToJson(beforeFailure))
        {
            return Fail("Cannot capture the reflected document.");
        }

        auto unsupportedType = reflection::TypeInfo::For<UnsupportedSettings>("example::UnsupportedSettings");
        unsupportedType.Properties.push_back(reflection::MakeProperty<&UnsupportedSettings::Speed>("Speed", "float"));
        unsupportedType.Properties.push_back(
            reflection::MakeProperty<&UnsupportedSettings::Values>("Values", "std::vector<long double>"));

        const UnsupportedSettings unsupported;
        auto result = archive_reflection::WriteObject(
            writer, "Settings", unsupportedType, reflection::ObjectView::From(unsupported));

        if (result.Code != archive::ArchiveErrorCode::UnsupportedType || result.Field != "Values")
        {
            return Fail("Unsupported reflected property check failed.");
        }

        std::string afterFailure;

        if (!writer.ToJson(afterFailure) || afterFailure != beforeFailure)
        {
            return Fail("Unsupported property writing changed the document.");
        }

        result = archive_reflection::WriteObject(
            writer, "Settings", *type, reflection::ObjectView::From(unsupported));

        if (result.Code != archive::ArchiveErrorCode::InvalidOperation ||
            !writer.ToJson(afterFailure) || afterFailure != beforeFailure)
        {
            return Fail("Mismatched reflected owner check failed.");
        }

        Settings invalid = source;
        invalid.Enabled = false;
        invalid.Speed = std::numeric_limits<float>::infinity();
        result = archive_reflection::WriteObject(writer, "Settings", *type, reflection::ObjectView::From(invalid));

        if (result.Code != archive::ArchiveErrorCode::InvalidValue || result.Field != "Speed" ||
            !writer.ToJson(afterFailure) || afterFailure != beforeFailure)
        {
            return Fail("Invalid reflected value writing changed the document.");
        }

        archive::JsonArchive failureReader;
        const std::string invalidSettingsJson =
            R"({"Settings":{"Enabled":false,"Speed":99,"Minimum":-1,"Maximum":1,)"
            R"("Name":"Changed","Title":42}})";

        if (!failureReader.Parse(invalidSettingsJson))
        {
            return Fail("Cannot prepare reflected reading failure.");
        }

        Settings unchanged = source;
        auto readResult = archive_reflection::ReadObject(failureReader, "Settings", *type, unchanged);

        if (readResult.Code != archive::ArchiveErrorCode::InvalidValue ||
            readResult.Field != "Title" || !SameSettings(unchanged, source))
        {
            return Fail("Failed reflected reading changed the destination.");
        }

        const std::string missingSettingsJson =
            R"({"Settings":{"Enabled":false,"Speed":99,"Minimum":-1,"Maximum":1,)"
            R"("Name":"Changed"}})";

        if (!failureReader.Parse(missingSettingsJson))
        {
            return Fail("Cannot prepare missing reflected field.");
        }

        readResult = archive_reflection::ReadObject(failureReader, "Settings", *type, unchanged);

        if (readResult.Code != archive::ArchiveErrorCode::MissingField ||
            readResult.Field != "Title" || !SameSettings(unchanged, source))
        {
            return Fail("Missing reflected field changed the destination.");
        }

        UnsupportedSettings unsupportedDestination;
        unsupportedDestination.Speed = -10.0f;
        unsupportedDestination.Values = { 9, 8 };
        const UnsupportedSettings beforeUnsupportedRead = unsupportedDestination;

        readResult = archive_reflection::ReadObject(reader, "Settings", unsupportedType, unsupportedDestination);

        if (readResult.Code != archive::ArchiveErrorCode::UnsupportedType ||
            readResult.Field != "Values" ||
            unsupportedDestination.Speed != beforeUnsupportedRead.Speed ||
            unsupportedDestination.Values != beforeUnsupportedRead.Values)
        {
            return Fail("Unsupported reflected reading changed the destination.");
        }

        readResult = archive_reflection::ReadObject(reader, "Settings", *type, unsupportedDestination);

        if (readResult.Code != archive::ArchiveErrorCode::InvalidOperation ||
            unsupportedDestination.Speed != beforeUnsupportedRead.Speed ||
            unsupportedDestination.Values != beforeUnsupportedRead.Values)
        {
            return Fail("Mismatched reflected reading changed the destination.");
        }

        std::cout << "Reflection archive reading checks passed.\n";
        std::cout << "Reflection archive reading failure checks passed.\n";

        std::cout << beforeFailure << '\n';
        std::cout << "Reflection archive writing checks passed.\n";
        std::cout << "Reflection archive failure checks passed.\n";
        return 0;
    }

    int CheckNestedArchive(reflection::Registry& registry, const std::filesystem::path& directory)
    {
        auto type = reflection::TypeInfo::For<ProfileData>("example::ProfileData");
        type.Properties.push_back(reflection::MakeProperty<&ProfileData::Name>("Name", "std::string"));
        type.Properties.push_back(reflection::MakeProperty<&ProfileData::Options>("Options", "Settings"));
        type.Properties.push_back(reflection::MakeProperty<&ProfileData::Level>("Level", "std::int32_t"));

        if (!registry.Register(std::move(type)))
        {
            return Fail("Profile registration failed.");
        }

        const auto* profileType = registry.FindType<ProfileData>();

        if (profileType == nullptr)
        {
            return Fail("Profile type lookup failed.");
        }

        const ProfileData source;
        archive::JsonArchive writer;

        if (!archive_reflection::WriteObject(
            writer, "Profile", *profileType, reflection::ObjectView::From(source), &registry))
        {
            return Fail("Nested reflected writing failed.");
        }

        const auto path = directory / "reflection-profile.json";
        archive::JsonArchive reader;
        ProfileData restored;
        restored.Name = "Before";
        restored.Options.Enabled = false;
        restored.Options.Speed = -123.0f;
        restored.Options.Minimum = 0;
        restored.Options.Maximum = 0;
        restored.Options.Name = "Before";
        restored.Options.Title = L"Before";
        restored.Level = -1;

        if (!writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Profile", *profileType, restored, &registry) ||
            !SameProfile(restored, source))
        {
            return Fail("Nested reflected round-trip failed.");
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
                R"({"Profile":{"Name":"Changed","Options":{"Enabled":false,"Speed":99,)"
                R"("Minimum":-1,"Maximum":1,"Name":"Changed","Title":42},"Level":9}})",
                "Options.Title", Code::InvalidValue
            },
            {
                R"({"Profile":{"Name":"Changed","Options":7,"Level":9}})",
                "Options", Code::InvalidValue
            },
            {
                R"({"Profile":{"Name":"Changed","Level":9}})",
                "Options", Code::MissingField
            },
            {
                R"({"Profile":{"Name":"Changed","Options":{"Enabled":false,"Speed":99,)"
                R"("Minimum":-1,"Maximum":1,"Name":"Changed","Title":"Changed"},"Level":"bad"}})",
                "Level", Code::InvalidValue
            }
        };

        for (const auto& failure : failures)
        {
            archive::JsonArchive failureReader;

            if (!failureReader.Parse(failure.Json))
            {
                return Fail("Cannot prepare nested reflected failure.");
            }

            ProfileData unchanged = source;
            const auto result =
                archive_reflection::ReadObject(failureReader, "Profile", *profileType, unchanged, &registry);

            if (result.Code != failure.Expected || result.Field != failure.Field ||
                !SameProfile(unchanged, source))
            {
                return Fail("Failed nested reading changed the destination.");
            }
        }

        std::string beforeFailure;

        if (!writer.ToJson(beforeFailure))
        {
            return Fail("Cannot capture the nested document.");
        }

        ProfileData invalid = source;
        invalid.Name = "Changed";
        invalid.Options.Enabled = false;
        invalid.Options.Speed = std::numeric_limits<float>::infinity();

        auto result = archive_reflection::WriteObject(
            writer, "Profile", *profileType, reflection::ObjectView::From(invalid), &registry);

        std::string afterFailure;

        if (result.Code != Code::InvalidValue || result.Field != "Options.Speed" ||
            !writer.ToJson(afterFailure) || afterFailure != beforeFailure)
        {
            return Fail("Failed nested writing changed the document.");
        }

        reflection::Registry emptyRegistry;
        result = archive_reflection::WriteObject(
            writer, "Profile", *profileType, reflection::ObjectView::From(source), &emptyRegistry);

        if (result.Code != Code::UnsupportedType || result.Field != "Options" ||
            !writer.ToJson(afterFailure) || afterFailure != beforeFailure)
        {
            return Fail("Unregistered nested type check failed.");
        }

        std::cout << beforeFailure << '\n';
        std::cout << "Reflection nested archive checks passed.\n";
        std::cout << "Reflection nested archive failure checks passed.\n";
        return 0;
    }
}