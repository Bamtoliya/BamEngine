#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include <reflection/Registry.h>

#if defined(ARCHIVE_EXAMPLE_GLM)
#include "Adapters/GlmArchiveAdapter.h"
#endif

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include <array>
#include <unordered_set>
#include <set>
#include <map>
#include <unordered_map>
#include <memory>
#include <array>

namespace
{
    struct Settings
    {
        bool Enabled = true;
        float Speed = 25.0f;
        std::int64_t Minimum = std::numeric_limits<std::int64_t>::min();
        std::uint64_t Maximum = std::numeric_limits<std::uint64_t>::max();
        std::string Name = "Movement";
        std::wstring Title = L"이동 설정 \U0001F600";
    };

    struct UnsupportedSettings
    {
        float Speed = 99.0f;
        std::vector<std::int32_t> Values{ 1, 2, 3 };
    };

    int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }

    reflection::TypeInfo MakeSettingsType()
    {
        auto type = reflection::TypeInfo::For<Settings>("example::Settings");
        type.Properties.push_back(reflection::MakeProperty<&Settings::Enabled>("Enabled", "bool"));
        type.Properties.push_back(reflection::MakeProperty<&Settings::Speed>("Speed", "float"));
        type.Properties.push_back(reflection::MakeProperty<&Settings::Minimum>("Minimum", "std::int64_t"));
        type.Properties.push_back(reflection::MakeProperty<&Settings::Maximum>("Maximum", "std::uint64_t"));
        type.Properties.push_back(reflection::MakeProperty<&Settings::Name>("Name", "std::string"));
        type.Properties.push_back(reflection::MakeProperty<&Settings::Title>("Title", "std::wstring"));
        return type;
    }

    bool SameSettings(const Settings& lhs, const Settings& rhs)
    {
        return lhs.Enabled == rhs.Enabled && lhs.Speed == rhs.Speed &&
            lhs.Minimum == rhs.Minimum && lhs.Maximum == rhs.Maximum &&
            lhs.Name == rhs.Name && lhs.Title == rhs.Title;
    }

    struct ProfileData
    {
        std::string Name = "Player";
        Settings Options;
        std::int32_t Level = 3;
    };

    bool SameProfile(const ProfileData& lhs, const ProfileData& rhs)
    {
        return lhs.Name == rhs.Name && SameSettings(lhs.Options, rhs.Options) && lhs.Level == rhs.Level;
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

    struct ContainerSettings
    {
        std::int32_t Level = 3;
        std::vector<std::int32_t> Values{ 1, -2, 300 };
        std::vector<bool> Flags{ true, false, true };
        std::array<double, 2> Samples{ 0.25, 0.75 };
        std::unordered_set<std::wstring> Tags{ L"플레이어", L"이동 \U0001F600" };
    };

    bool SameContainers(const ContainerSettings& lhs, const ContainerSettings& rhs)
    {
        return lhs.Level == rhs.Level && lhs.Values == rhs.Values && lhs.Flags == rhs.Flags &&
            lhs.Samples == rhs.Samples && lhs.Tags == rhs.Tags;
    }

    int CheckContainerArchive(const std::filesystem::path& directory)
    {
        archive_reflection::PropertyCodecs codecs;

        if (!codecs.RegisterArray<std::vector<std::int32_t>>() ||
            !codecs.RegisterArray<std::vector<bool>>() ||
            !codecs.RegisterArray<std::array<double, 2>>() ||
            !codecs.RegisterArray<std::unordered_set<std::wstring>>())
        {
            return Fail("Container codec registration failed.");
        }

        auto type = reflection::TypeInfo::For<ContainerSettings>("example::ContainerSettings");
        type.Properties.push_back(reflection::MakeProperty<&ContainerSettings::Level>("Level", "std::int32_t"));
        type.Properties.push_back(
            reflection::MakeProperty<&ContainerSettings::Values>("Values", "std::vector<std::int32_t>"));
        type.Properties.push_back(
            reflection::MakeProperty<&ContainerSettings::Flags>("Flags", "std::vector<bool>"));
        type.Properties.push_back(
            reflection::MakeProperty<&ContainerSettings::Samples>("Samples", "std::array<double, 2>"));
        type.Properties.push_back(
            reflection::MakeProperty<&ContainerSettings::Tags>("Tags", "std::unordered_set<std::wstring>"));

        const ContainerSettings source;
        archive::JsonArchive writer;

        if (!archive_reflection::WriteObject(
            writer, "Containers", type, reflection::ObjectView::From(source), nullptr, &codecs))
        {
            return Fail("Reflected container writing failed.");
        }

        const auto path = directory / "reflection-containers.json";
        archive::JsonArchive reader;
        ContainerSettings restored;
        restored.Level = -1;
        restored.Values = { 9 };
        restored.Flags = { false };
        restored.Samples = { -1.0, -2.0 };
        restored.Tags = { L"Before" };

        if (!writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Containers", type, restored, nullptr, &codecs) ||
            !SameContainers(restored, source))
        {
            return Fail("Reflected container round-trip failed.");
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
                R"({"Containers":{"Level":99,"Values":[1,2147483648]}})",
                "Values", Code::InvalidValue
            },
            {
                R"({"Containers":{"Level":99,"Values":[9],"Flags":[true,1]}})",
                "Flags", Code::InvalidValue
            },
            {
                R"({"Containers":{"Level":99,"Values":[9],"Flags":[false],"Samples":[1]}})",
                "Samples", Code::InvalidValue
            },
            {
                R"({"Containers":{"Level":99,"Values":[9],"Flags":[false],)"
                R"("Samples":[1,2],"Tags":["same","same"]}})",
                "Tags", Code::InvalidValue
            },
            {
                R"({"Containers":{"Level":99,"Values":[9],"Flags":[false],"Samples":[1,2]}})",
                "Tags", Code::MissingField
            }
        };

        for (const auto& failure : failures)
        {
            archive::JsonArchive failureReader;

            if (!failureReader.Parse(failure.Json))
            {
                return Fail("Cannot prepare reflected container failure.");
            }

            ContainerSettings unchanged = source;
            const auto result = archive_reflection::ReadObject(
                failureReader, "Containers", type, unchanged, nullptr, &codecs);

            if (result.Code != failure.Expected || result.Field != failure.Field ||
                !SameContainers(unchanged, source))
            {
                return Fail("Failed container reading changed the destination.");
            }
        }

        std::string beforeFailure;

        if (!writer.ToJson(beforeFailure))
        {
            return Fail("Cannot capture the container document.");
        }

        ContainerSettings invalid = source;
        invalid.Level = 99;
        invalid.Values = { 9 };
        invalid.Flags = { false };
        invalid.Samples[1] = std::numeric_limits<double>::infinity();

        const auto result = archive_reflection::WriteObject(
            writer, "Containers", type, reflection::ObjectView::From(invalid), nullptr, &codecs);

        std::string afterFailure;

        if (result.Code != Code::InvalidValue || result.Field != "Samples" ||
            !writer.ToJson(afterFailure) || afterFailure != beforeFailure)
        {
            return Fail("Failed container writing changed the document.");
        }

        std::cout << beforeFailure << '\n';
        std::cout << "Reflection container archive checks passed.\n";
        std::cout << "Reflection container archive failure checks passed.\n";
        return 0;
    }

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

    struct SetSettings
    {
        std::set<std::int32_t> Sorted{ 3, 1, 2 };
        std::unordered_set<std::wstring> Tags{ L"플레이어", L"이동" };
    };

    int CheckSetContainers(const std::filesystem::path& directory)
    {
        auto sorted = reflection::MakeProperty<&SetSettings::Sorted>("Sorted", "std::set<std::int32_t>");
        auto tags = reflection::MakeProperty<&SetSettings::Tags>("Tags", "std::unordered_set<std::wstring>");
        const SetSettings source;
        const auto view = reflection::ObjectView::From(source);

        const auto sortedSize = sorted.TryGetSize(view);
        const auto tagSize = tags.TryGetSize(view);
        const auto first = sorted.TryReadElement(view, 0);
        const auto* firstValue = first ? first.Value.Get<std::int32_t>() : nullptr;

        if (sorted.GetValueKind() != reflection::PropertyValueKind::Set ||
            tags.GetValueKind() != reflection::PropertyValueKind::UnorderedSet ||
            !sortedSize || sortedSize.Size != 3 || !tagSize || tagSize.Size != 2 ||
            firstValue == nullptr || *firstValue != 1 ||
            sorted.CanWriteElement() || tags.CanWriteElement() ||
            sorted.CanResize() || tags.CanEditElementObject())
        {
            return Fail("Reflection set capabilities failed.");
        }

        const auto tag = tags.TryReadElement(view, 0);
        const auto* tagValue = tag ? tag.Value.Get<std::wstring>() : nullptr;

        if (tagValue == nullptr || !source.Tags.contains(*tagValue) ||
            sorted.TryReadElement(view, 3).Error != reflection::PropertyAccessError::IndexOutOfRange ||
            sorted.TryClear(view) != reflection::PropertyAccessError::ReadOnlyObject)
        {
            return Fail("Reflection set access checks failed.");
        }

        SetSettings editable = source;

        if (sorted.TryClear(reflection::ObjectView::From(editable)) != reflection::PropertyAccessError::None ||
            !editable.Sorted.empty() || editable.Tags != source.Tags)
        {
            return Fail("Reflection set clearing failed.");
        }

        auto type = reflection::TypeInfo::For<SetSettings>("example::SetSettings");
        type.Properties.push_back(sorted);
        type.Properties.push_back(tags);

        archive_reflection::PropertyCodecs codecs;

        if (!codecs.RegisterArray<std::set<std::int32_t>>() ||
            !codecs.RegisterArray<std::unordered_set<std::wstring>>())
        {
            return Fail("Set codec registration failed.");
        }

        archive::JsonArchive writer;
        archive::JsonArchive reader;
        SetSettings restored;
        restored.Sorted = { 99 };
        restored.Tags = { L"Before" };

        if (!archive_reflection::WriteObject(
            writer, "Sets", type, reflection::ObjectView::From(source), nullptr, &codecs) ||
            !writer.SaveToFile(directory / "reflection-sets.json") ||
            !reader.LoadFromFile(directory / "reflection-sets.json") ||
            !archive_reflection::ReadObject(reader, "Sets", type, restored, nullptr, &codecs) ||
            restored.Sorted != source.Sorted || restored.Tags != source.Tags)
        {
            return Fail("Reflected set round-trip failed.");
        }

        struct FailureCase
        {
            const char* Json;
            const char* Field;
        };

        const FailureCase failures[] = {
            { R"({"Sets":{"Sorted":[1,1],"Tags":[]}})", "Sorted" },
            { R"({"Sets":{"Sorted":[1,"bad"],"Tags":[]}})", "Sorted" },
            { R"({"Sets":{"Sorted":[9],"Tags":["same","same"]}})", "Tags" }
        };

        for (const auto& failure : failures)
        {
            archive::JsonArchive failureReader;

            if (!failureReader.Parse(failure.Json))
            {
                return Fail("Cannot prepare set reading failure.");
            }

            SetSettings unchanged = source;
            const auto result = archive_reflection::ReadObject(
                failureReader, "Sets", type, unchanged, nullptr, &codecs);

            if (result.Code != archive::ArchiveErrorCode::InvalidValue || result.Field != failure.Field ||
                unchanged.Sorted != source.Sorted || unchanged.Tags != source.Tags)
            {
                return Fail("Failed set reading changed the destination.");
            }
        }

        std::cout << "Reflection set container checks passed.\n";
        std::cout << "Reflection set archive checks passed.\n";
        std::cout << "Reflection set archive failure checks passed.\n";
        return 0;
    }

    int CheckSetMutation(const std::filesystem::path& directory)
    {
        using Error = reflection::PropertyAccessError;
        using Value = reflection::ValueView;

        auto sorted = reflection::MakeProperty<&SetSettings::Sorted>("Sorted", "Sorted");
        auto tags = reflection::MakeProperty<&SetSettings::Tags>("Tags", "Tags");

        const SetSettings initial;
        const auto initialObject = reflection::ObjectView::From(initial);
        const std::int32_t existingNumber = 1;
        const std::wstring existingTag = L"플레이어";

        const auto number = sorted.TryFindSetElement(initialObject, Value::From(existingNumber));
        const auto tag = tags.TryFindSetElement(initialObject, Value::From(existingTag));
        const auto* typedNumber = number ? number.Value.Get<std::int32_t>() : nullptr;
        const auto* typedTag = tag ? tag.Value.Get<std::wstring>() : nullptr;

        if (!sorted.IsSet() || !tags.IsSet() ||
            typedNumber == nullptr || *typedNumber != existingNumber ||
            typedTag == nullptr || *typedTag != existingTag)
        {
            return Fail("Set value lookup failed.");
        }

        SetSettings settings = initial;
        const auto object = reflection::ObjectView::From(settings);
        const std::int32_t newNumber = 4;
        const std::wstring newTag = L"적";
        const auto numberView = Value::From(newNumber);
        const auto tagView = Value::From(newTag);
        const auto removedNumberView = Value::From(existingNumber);
        const auto removedTagView = Value::From(existingTag);

        if (!sorted.CanInsertSetElement() || !sorted.CanEraseSetElement() ||
            !tags.CanInsertSetElement() || !tags.CanEraseSetElement() ||
            sorted.CanWriteElement() || tags.CanEditElementObject())
        {
            return Fail("Set mutation capabilities were incorrect.");
        }

        if (sorted.TryInsertSetElement(object, numberView) != Error::None ||
            tags.TryInsertSetElement(object, tagView) != Error::None ||
            settings.Sorted.size() != 4 || settings.Tags.size() != 3 ||
            !settings.Sorted.contains(newNumber) || !settings.Tags.contains(newTag))
        {
            return Fail("Set insertion failed.");
        }

        if (sorted.TryEraseSetElement(object, removedNumberView) != Error::None ||
            tags.TryEraseSetElement(object, removedTagView) != Error::None ||
            settings.Sorted.size() != 3 || settings.Tags.size() != 2 ||
            settings.Sorted.contains(existingNumber) || settings.Tags.contains(existingTag))
        {
            return Fail("Set erasing failed.");
        }

        const SetSettings beforeFailure = settings;
        const auto readOnlyObject = reflection::ObjectView::From(beforeFailure);
        const std::int64_t wrongType = 4;
        const auto wrongView = Value::From(wrongType);

        Settings other;
        const auto otherObject = reflection::ObjectView::From(other);
        auto speed = reflection::MakeProperty<&Settings::Speed>("Speed", "float");

        struct ReadOnlySets
        {
            const std::set<std::int32_t> Sorted;
            const std::unordered_set<std::wstring> Tags;
        };

        ReadOnlySets fixed;
        const auto fixedObject = reflection::ObjectView::From(fixed);
        auto fixedSorted = reflection::MakeProperty<&ReadOnlySets::Sorted>("Sorted", "Sorted");
        auto fixedTags = reflection::MakeProperty<&ReadOnlySets::Tags>("Tags", "Tags");

        if (fixedSorted.CanInsertSetElement() || fixedSorted.CanEraseSetElement() ||
            fixedTags.CanInsertSetElement() || fixedTags.CanEraseSetElement())
        {
            return Fail("Const set mutation capabilities were bound.");
        }

        struct FailureCase
        {
            Error Actual;
            Error Expected;
        };

        const FailureCase failures[] = {
            { sorted.TryInsertSetElement(object, numberView), Error::ElementAlreadyExists },
            { tags.TryInsertSetElement(object, tagView), Error::ElementAlreadyExists },
            { sorted.TryFindSetElement(object, removedNumberView).Error, Error::ElementNotFound },
            { tags.TryFindSetElement(object, removedTagView).Error, Error::ElementNotFound },
            { sorted.TryEraseSetElement(object, removedNumberView), Error::ElementNotFound },
            { tags.TryEraseSetElement(object, removedTagView), Error::ElementNotFound },
            { sorted.TryFindSetElement(object, wrongView).Error, Error::ValueTypeMismatch },
            { sorted.TryInsertSetElement(object, wrongView), Error::ValueTypeMismatch },
            { tags.TryEraseSetElement(object, wrongView), Error::ValueTypeMismatch },
            { sorted.TryFindSetElement(object, {}).Error, Error::InvalidValue },
            { sorted.TryInsertSetElement(object, {}), Error::InvalidValue },
            { tags.TryEraseSetElement(object, {}), Error::InvalidValue },
            { sorted.TryFindSetElement({}, numberView).Error, Error::InvalidObject },
            { sorted.TryInsertSetElement({}, numberView), Error::InvalidObject },
            { tags.TryEraseSetElement({}, tagView), Error::InvalidObject },
            { sorted.TryInsertSetElement(readOnlyObject, numberView), Error::ReadOnlyObject },
            { tags.TryEraseSetElement(readOnlyObject, tagView), Error::ReadOnlyObject },
            { fixedSorted.TryInsertSetElement(fixedObject, numberView), Error::ReadOnlyProperty },
            { fixedSorted.TryEraseSetElement(fixedObject, numberView), Error::ReadOnlyProperty },
            { fixedTags.TryInsertSetElement(fixedObject, tagView), Error::ReadOnlyProperty },
            { fixedTags.TryEraseSetElement(fixedObject, tagView), Error::ReadOnlyProperty },
            { sorted.TryFindSetElement(otherObject, numberView).Error, Error::OwnerTypeMismatch },
            { sorted.TryInsertSetElement(otherObject, numberView), Error::OwnerTypeMismatch },
            { tags.TryEraseSetElement(otherObject, tagView), Error::OwnerTypeMismatch },
            { speed.TryFindSetElement(otherObject, numberView).Error, Error::NotSet },
            { speed.TryInsertSetElement(otherObject, numberView), Error::NotSet },
            { speed.TryEraseSetElement(otherObject, numberView), Error::NotSet }
        };

        for (const auto& failure : failures)
        {
            if (failure.Actual != failure.Expected)
            {
                std::cerr << "Expected " << reflection::ToString(failure.Expected)
                    << ", got " << reflection::ToString(failure.Actual) << '\n';
                return Fail("Set mutation failure checks failed.");
            }
        }

        if (settings.Sorted != beforeFailure.Sorted || settings.Tags != beforeFailure.Tags ||
            !fixed.Sorted.empty() || !fixed.Tags.empty())
        {
            return Fail("Failed set mutation changed the destination.");
        }

        struct CompareTens
        {
            bool operator()(std::int32_t left, std::int32_t right) const
            {
                return left / 10 < right / 10;
            }
        };

        struct ComparedSets
        {
            std::set<std::int32_t, CompareTens> Values{ 11 };
        };

        ComparedSets compared;
        auto comparedProperty = reflection::MakeProperty<&ComparedSets::Values>("Values", "Values");
        const auto comparedObject = reflection::ObjectView::From(compared);
        const std::int32_t equivalent = 19;
        const auto equivalentView = Value::From(equivalent);
        const auto found = comparedProperty.TryFindSetElement(comparedObject, equivalentView);
        const auto* stored = found ? found.Value.Get<std::int32_t>() : nullptr;

        if (stored == nullptr || *stored != 11 ||
            comparedProperty.TryInsertSetElement(comparedObject, equivalentView) != Error::ElementAlreadyExists ||
            comparedProperty.TryEraseSetElement(comparedObject, equivalentView) != Error::None ||
            !compared.Values.empty())
        {
            return Fail("Set comparison policy was not preserved.");
        }

        struct MoveOnlySets
        {
            std::set<std::unique_ptr<int>> Values;
        };

        MoveOnlySets moveOnly;
        moveOnly.Values.insert(std::make_unique<int>(7));
        auto moveProperty = reflection::MakeProperty<&MoveOnlySets::Values>("Values", "Values");
        const auto moveObject = reflection::ObjectView::From(moveOnly);
        const auto entry = moveProperty.TryReadElement(moveObject, 0);

        if (moveProperty.CanInsertSetElement() || !moveProperty.CanEraseSetElement() || !entry ||
            moveProperty.TryInsertSetElement(moveObject, entry.Value) != Error::InsertUnavailable ||
            moveOnly.Values.size() != 1 ||
            moveProperty.TryEraseSetElement(moveObject, entry.Value) != Error::None ||
            !moveOnly.Values.empty())
        {
            return Fail("Move-only set capabilities were incorrect.");
        }

        auto type = reflection::TypeInfo::For<SetSettings>("example::SetSettings");
        type.Properties.push_back(sorted);
        type.Properties.push_back(tags);

        archive_reflection::PropertyCodecs codecs;
        if (!codecs.RegisterArray<std::set<std::int32_t>>() ||
            !codecs.RegisterArray<std::unordered_set<std::wstring>>())
        {
            return Fail("Edited set codec registration failed.");
        }

        archive::JsonArchive writer;
        archive::JsonArchive reader;
        SetSettings restored;

        if (!archive_reflection::WriteObject(writer, "Sets", type, object, nullptr, &codecs) ||
            !writer.SaveToFile(directory / "reflection-set-editing.json") ||
            !reader.LoadFromFile(directory / "reflection-set-editing.json") ||
            !archive_reflection::ReadObject(reader, "Sets", type, restored, nullptr, &codecs) ||
            restored.Sorted != settings.Sorted || restored.Tags != settings.Tags)
        {
            return Fail("Edited set archive round-trip failed.");
        }

        std::cout << "Reflection set lookup and mutation checks passed.\n";
        std::cout << "Reflection set mutation failure checks passed.\n";
        std::cout << "Edited set archive checks passed.\n";
        return 0;
    }

    struct MapSettings
    {
        std::map<std::int64_t, std::wstring> Numbers{ { -1, L"음수" }, { 10, L"한글" } };
        std::unordered_map<std::wstring, std::uint64_t> Counts{
            { L"플레이어", 3 }, { L"최대", std::numeric_limits<std::uint64_t>::max() }
        };
    };

    int CheckMapContainers(const std::filesystem::path& directory)
    {
        auto numbers = reflection::MakeProperty<&MapSettings::Numbers>(
            "Numbers", "std::map<std::int64_t, std::wstring>");
        auto counts = reflection::MakeProperty<&MapSettings::Counts>(
            "Counts", "std::unordered_map<std::wstring, std::uint64_t>");

        const MapSettings source;
        const auto view = reflection::ObjectView::From(source);
        const auto size = numbers.TryGetSize(view);
        const auto key = numbers.TryReadMapKey(view, 0);
        const auto value = numbers.TryReadMapValue(view, 0);
        const auto* typedKey = key ? key.Value.Get<std::int64_t>() : nullptr;
        const auto* typedValue = value ? value.Value.Get<std::wstring>() : nullptr;

        if (numbers.GetValueKind() != reflection::PropertyValueKind::Map ||
            counts.GetValueKind() != reflection::PropertyValueKind::UnorderedMap ||
            !numbers.IsMap() || !counts.IsMap() || !size || size.Size != 2 ||
            typedKey == nullptr || *typedKey != -1 ||
            typedValue == nullptr || *typedValue != L"음수" ||
            numbers.CanWriteElement() || numbers.CanResize() ||
            numbers.TryReadMapKey(view, 2).Error != reflection::PropertyAccessError::IndexOutOfRange)
        {
            return Fail("Reflection map access checks failed.");
        }

        const auto unorderedKey = counts.TryReadMapKey(view, 0);
        const auto unorderedValue = counts.TryReadMapValue(view, 0);
        const auto* name = unorderedKey ? unorderedKey.Value.Get<std::wstring>() : nullptr;
        const auto* count = unorderedValue ? unorderedValue.Value.Get<std::uint64_t>() : nullptr;

        if (name == nullptr || count == nullptr ||
            !source.Counts.contains(*name) || source.Counts.at(*name) != *count)
        {
            return Fail("Reflection unordered map access failed.");
        }

        MapSettings editable = source;

        if (numbers.TryClear(reflection::ObjectView::From(editable)) != reflection::PropertyAccessError::None ||
            !editable.Numbers.empty() || editable.Counts != source.Counts ||
            numbers.TryClear(view) != reflection::PropertyAccessError::ReadOnlyObject)
        {
            return Fail("Reflection map clearing failed.");
        }

        auto type = reflection::TypeInfo::For<MapSettings>("example::MapSettings");
        type.Properties.push_back(numbers);
        type.Properties.push_back(counts);
        archive_reflection::PropertyCodecs codecs;

        if (!codecs.RegisterMap<std::map<std::int64_t, std::wstring>>() ||
            !codecs.RegisterMap<std::unordered_map<std::wstring, std::uint64_t>>())
        {
            return Fail("Map codec registration failed.");
        }

        archive::JsonArchive writer;
        archive::JsonArchive reader;
        MapSettings restored;
        restored.Numbers = { { 99, L"Before" } };
        restored.Counts = { { L"Before", 99 } };

        if (!archive_reflection::WriteObject(
            writer, "Maps", type, reflection::ObjectView::From(source), nullptr, &codecs) ||
            !writer.SaveToFile(directory / "reflection-maps.json") ||
            !reader.LoadFromFile(directory / "reflection-maps.json") ||
            !archive_reflection::ReadObject(reader, "Maps", type, restored, nullptr, &codecs) ||
            restored.Numbers != source.Numbers || restored.Counts != source.Counts)
        {
            return Fail("Reflected map round-trip failed.");
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
                R"({"Maps":{"Numbers":[{"Key":1,"Value":"a"},{"Key":1,"Value":"b"}]}})",
                "Numbers[1].Key", Code::InvalidValue
            },
            {
                R"({"Maps":{"Numbers":[{"Value":"a"}]}})",
                "Numbers[0].Key", Code::MissingField
            },
            {
                R"({"Maps":{"Numbers":[{"Key":1,"Value":42}]}})",
                "Numbers[0].Value", Code::InvalidValue
            },
            {
                R"({"Maps":{"Numbers":[42]}})",
                "Numbers[0]", Code::InvalidValue
            },
            {
                R"({"Maps":{"Numbers":[],"Counts":[{"Key":"a","Value":-1}]}})",
                "Counts[0].Value", Code::InvalidValue
            },
            {
                R"({"Maps":{"Numbers":[],"Counts":[{"Key":"a","Value":1},{"Key":"a","Value":2}]}})",
                "Counts[1].Key", Code::InvalidValue
            }
        };

        for (const auto& failure : failures)
        {
            archive::JsonArchive failureReader;

            if (!failureReader.Parse(failure.Json))
            {
                return Fail("Cannot prepare map reading failure.");
            }

            MapSettings unchanged = source;
            const auto result = archive_reflection::ReadObject(
                failureReader, "Maps", type, unchanged, nullptr, &codecs);

            if (result.Code != failure.Expected || result.Field != failure.Field ||
                unchanged.Numbers != source.Numbers || unchanged.Counts != source.Counts)
            {
                return Fail("Failed map reading changed the destination.");
            }
        }

        std::string beforeFailure;

        if (!writer.ToJson(beforeFailure))
        {
            return Fail("Cannot capture the map document.");
        }

        MapSettings invalid = source;
        invalid.Numbers.at(10).assign(1, static_cast<wchar_t>(0xD800));

        const auto result = archive_reflection::WriteObject(
            writer, "Maps", type, reflection::ObjectView::From(invalid), nullptr, &codecs);
        std::string afterFailure;

        if (result.Code != Code::InvalidValue || result.Field != "Numbers[1].Value" ||
            !writer.ToJson(afterFailure) || afterFailure != beforeFailure)
        {
            return Fail("Failed map writing changed the document.");
        }

        std::cout << "Reflection map container checks passed.\n";
        std::cout << "Reflection map archive checks passed.\n";
        std::cout << "Reflection map archive failure checks passed.\n";
        return 0;
    }

    int CheckMapKeyAccess()
    {
        using Error = reflection::PropertyAccessError;
        using Value = reflection::ValueView;

        auto numbers = reflection::MakeProperty<&MapSettings::Numbers>("Numbers", "Numbers");
        auto counts = reflection::MakeProperty<&MapSettings::Counts>("Counts", "Counts");

        MapSettings settings;
        const auto object = reflection::ObjectView::From(settings);
        const std::int64_t numberKey = -1;
        const std::wstring countKey = L"플레이어";

        const auto number = numbers.TryFindMapValue(object, Value::From(numberKey));
        const auto count = counts.TryFindMapValue(object, Value::From(countKey));
        const auto* text = number ? number.Value.Get<std::wstring>() : nullptr;
        const auto* amount = count ? count.Value.Get<std::uint64_t>() : nullptr;

        if (!numbers.CanWriteMapValue() || !counts.CanWriteMapValue() ||
            text == nullptr || *text != L"음수" || amount == nullptr || *amount != 3)
        {
            return Fail("Reflection map key lookup failed.");
        }

        const std::wstring replacement = L"수정된 값";
        const std::uint64_t replacementCount = 25;

        if (numbers.TryWriteMapValue(object, Value::From(numberKey), Value::From(replacement)) != Error::None ||
            counts.TryWriteMapValue(object, Value::From(countKey), Value::From(replacementCount)) != Error::None ||
            settings.Numbers.at(numberKey) != replacement || settings.Counts.at(countKey) != replacementCount ||
            settings.Numbers.size() != 2 || settings.Counts.size() != 2)
        {
            return Fail("Reflection map value update failed.");
        }

        const MapSettings beforeFailure = settings;
        const auto readOnlyObject = reflection::ObjectView::From(beforeFailure);
        const std::int64_t missingNumber = 999;
        const std::wstring missingName = L"없는 키";
        const std::int32_t wrongKey = -1;
        const double wrongValue = 1.0;

        if (numbers.TryFindMapValue(object, Value::From(missingNumber)).Error != Error::KeyNotFound ||
            counts.TryFindMapValue(object, Value::From(missingName)).Error != Error::KeyNotFound ||
            numbers.TryFindMapValue(object, Value::From(wrongKey)).Error != Error::KeyTypeMismatch ||
            numbers.TryFindMapValue(object, {}).Error != Error::InvalidValue ||
            numbers.TryFindMapValue({}, Value::From(numberKey)).Error != Error::InvalidObject)
        {
            return Fail("Reflection map lookup failure checks failed.");
        }

        if (numbers.TryWriteMapValue(object, Value::From(missingNumber), Value::From(replacement)) !=
            Error::KeyNotFound)
        {
            return Fail("Writing a missing map key did not fail.");
        }

        if (counts.TryWriteMapValue(object, Value::From(missingName), Value::From(replacementCount)) !=
            Error::KeyNotFound)
        {
            return Fail("Writing a missing unordered map key did not fail.");
        }

        if (numbers.TryWriteMapValue(object, Value::From(wrongKey), Value::From(replacement)) !=
            Error::KeyTypeMismatch)
        {
            return Fail("An incorrect map key type was accepted.");
        }

        if (numbers.TryWriteMapValue(object, Value::From(numberKey), Value::From(wrongValue)) !=
            Error::ValueTypeMismatch)
        {
            return Fail("An incorrect mapped value type was accepted.");
        }

        if (numbers.TryWriteMapValue(readOnlyObject, Value::From(numberKey), Value::From(replacement)) !=
            Error::ReadOnlyObject)
        {
            return Fail("A read-only map owner was accepted.");
        }

        if (numbers.TryWriteMapValue(object, Value::From(numberKey), {}) != Error::InvalidValue ||
            settings.Numbers != beforeFailure.Numbers || settings.Counts != beforeFailure.Counts)
        {
            return Fail("Failed map access changed the destination.");
        }

        struct ReadOnlyMaps
        {
            const std::map<std::int64_t, std::wstring> Values{ { -1, L"고정" } };
        };

        ReadOnlyMaps fixed;
        auto fixedProperty = reflection::MakeProperty<&ReadOnlyMaps::Values>("Values", "Values");
        const auto fixedObject = reflection::ObjectView::From(fixed);
        const auto fixedValue = fixedProperty.TryFindMapValue(fixedObject, Value::From(numberKey));
        const auto* fixedText = fixedValue ? fixedValue.Value.Get<std::wstring>() : nullptr;

        if (fixedProperty.CanWriteMapValue() || fixedText == nullptr || *fixedText != L"고정")
        {
            return Fail("Const map property lookup failed.");
        }

        if (fixedProperty.TryWriteMapValue(fixedObject, Value::From(numberKey), Value::From(replacement)) !=
            Error::ReadOnlyProperty)
        {
            return Fail("A const map property was accepted for writing.");
        }

        Settings other;
        auto speed = reflection::MakeProperty<&Settings::Speed>("Speed", "float");
        const auto otherObject = reflection::ObjectView::From(other);

        if (numbers.TryFindMapValue(otherObject, Value::From(numberKey)).Error != Error::OwnerTypeMismatch ||
            speed.TryFindMapValue(otherObject, Value::From(numberKey)).Error != Error::NotMap)
        {
            return Fail("Map owner and property validation failed.");
        }

        std::cout << "Reflection map key lookup checks passed.\n";
        std::cout << "Reflection map value update checks passed.\n";
        return 0;
    }

    int CheckMapMutation(const std::filesystem::path& directory)
    {
        using Error = reflection::PropertyAccessError;
        using Value = reflection::ValueView;

        auto numbers = reflection::MakeProperty<&MapSettings::Numbers>("Numbers", "Numbers");
        auto counts = reflection::MakeProperty<&MapSettings::Counts>("Counts", "Counts");

        MapSettings settings;
        const auto object = reflection::ObjectView::From(settings);

        const std::int64_t numberKey = 99;
        const std::wstring numberValue = L"추가된 항목";
        const std::wstring countKey = L"적";
        const std::uint64_t countValue = 25;
        const std::int64_t removedNumber = -1;
        const std::wstring removedName = L"플레이어";

        const auto numberKeyView = Value::From(numberKey);
        const auto numberValueView = Value::From(numberValue);
        const auto countKeyView = Value::From(countKey);
        const auto countValueView = Value::From(countValue);
        const auto removedNumberView = Value::From(removedNumber);
        const auto removedNameView = Value::From(removedName);

        if (!numbers.CanInsertMapEntry() || !numbers.CanEraseMapEntry() ||
            !counts.CanInsertMapEntry() || !counts.CanEraseMapEntry())
        {
            return Fail("Map mutation capabilities were not bound.");
        }

        if (numbers.TryInsertMapEntry(object, numberKeyView, numberValueView) != Error::None ||
            counts.TryInsertMapEntry(object, countKeyView, countValueView) != Error::None ||
            settings.Numbers.size() != 3 || settings.Counts.size() != 3 ||
            settings.Numbers.at(numberKey) != numberValue || settings.Counts.at(countKey) != countValue)
        {
            return Fail("Map insertion failed.");
        }

        if (numbers.TryEraseMapEntry(object, removedNumberView) != Error::None ||
            counts.TryEraseMapEntry(object, removedNameView) != Error::None ||
            settings.Numbers.size() != 2 || settings.Counts.size() != 2 ||
            settings.Numbers.contains(removedNumber) || settings.Counts.contains(removedName))
        {
            return Fail("Map erasing failed.");
        }

        const MapSettings beforeFailure = settings;
        const auto readOnlyObject = reflection::ObjectView::From(beforeFailure);
        const std::int32_t wrongKey = 99;
        const double wrongValue = 1.0;
        const std::wstring duplicateValue = L"덮어쓰면 안 됨";
        const std::uint64_t duplicateCount = 999;

        Settings other;
        const auto otherObject = reflection::ObjectView::From(other);
        auto speed = reflection::MakeProperty<&Settings::Speed>("Speed", "float");

        struct ReadOnlyMaps
        {
            const std::map<std::int64_t, std::wstring> Numbers;
            const std::unordered_map<std::wstring, std::uint64_t> Counts;
        };

        ReadOnlyMaps fixed;
        const auto fixedObject = reflection::ObjectView::From(fixed);
        auto fixedNumbers = reflection::MakeProperty<&ReadOnlyMaps::Numbers>("Numbers", "Numbers");
        auto fixedCounts = reflection::MakeProperty<&ReadOnlyMaps::Counts>("Counts", "Counts");

        if (fixedNumbers.CanInsertMapEntry() || fixedNumbers.CanEraseMapEntry() ||
            fixedCounts.CanInsertMapEntry() || fixedCounts.CanEraseMapEntry())
        {
            return Fail("Const map mutation capabilities were bound.");
        }

        struct FailureCase
        {
            Error Actual;
            Error Expected;
        };

        const FailureCase failures[] = {
            {
                numbers.TryInsertMapEntry(object, numberKeyView, Value::From(duplicateValue)),
                Error::KeyAlreadyExists
            },
            {
                counts.TryInsertMapEntry(object, countKeyView, Value::From(duplicateCount)),
                Error::KeyAlreadyExists
            },
            { numbers.TryEraseMapEntry(object, removedNumberView), Error::KeyNotFound },
            { counts.TryEraseMapEntry(object, removedNameView), Error::KeyNotFound },
            {
                numbers.TryInsertMapEntry(object, Value::From(wrongKey), numberValueView),
                Error::KeyTypeMismatch
            },
            { numbers.TryEraseMapEntry(object, Value::From(wrongKey)), Error::KeyTypeMismatch },
            {
                counts.TryInsertMapEntry(object, countKeyView, Value::From(wrongValue)),
                Error::ValueTypeMismatch
            },
            { numbers.TryInsertMapEntry(object, {}, numberValueView), Error::InvalidValue },
            { counts.TryInsertMapEntry(object, countKeyView, {}), Error::InvalidValue },
            { numbers.TryEraseMapEntry(object, {}), Error::InvalidValue },
            { numbers.TryInsertMapEntry({}, numberKeyView, numberValueView), Error::InvalidObject },
            { numbers.TryEraseMapEntry({}, numberKeyView), Error::InvalidObject },
            {
                numbers.TryInsertMapEntry(readOnlyObject, numberKeyView, numberValueView),
                Error::ReadOnlyObject
            },
            { counts.TryEraseMapEntry(readOnlyObject, countKeyView), Error::ReadOnlyObject },
            {
                numbers.TryInsertMapEntry(otherObject, numberKeyView, numberValueView),
                Error::OwnerTypeMismatch
            },
            { numbers.TryEraseMapEntry(otherObject, numberKeyView), Error::OwnerTypeMismatch },
            { speed.TryInsertMapEntry(otherObject, numberKeyView, numberValueView), Error::NotMap },
            { speed.TryEraseMapEntry(otherObject, numberKeyView), Error::NotMap },
            {
                fixedNumbers.TryInsertMapEntry(fixedObject, numberKeyView, numberValueView),
                Error::ReadOnlyProperty
            },
            { fixedNumbers.TryEraseMapEntry(fixedObject, numberKeyView), Error::ReadOnlyProperty },
            {
                fixedCounts.TryInsertMapEntry(fixedObject, countKeyView, countValueView),
                Error::ReadOnlyProperty
            },
            { fixedCounts.TryEraseMapEntry(fixedObject, countKeyView), Error::ReadOnlyProperty }
        };

        for (const auto& failure : failures)
        {
            if (failure.Actual != failure.Expected)
            {
                std::cerr << "Expected " << reflection::ToString(failure.Expected)
                    << ", got " << reflection::ToString(failure.Actual) << '\n';
                return Fail("Map mutation failure checks failed.");
            }
        }

        if (settings.Numbers != beforeFailure.Numbers || settings.Counts != beforeFailure.Counts ||
            !fixed.Numbers.empty() || !fixed.Counts.empty())
        {
            return Fail("Failed map mutation changed the destination.");
        }

        struct MoveOnlyMaps
        {
            std::map<std::int64_t, std::unique_ptr<int>> Values;
        };

        MoveOnlyMaps moveOnly;
        moveOnly.Values.emplace(numberKey, std::make_unique<int>(7));
        auto moveProperty = reflection::MakeProperty<&MoveOnlyMaps::Values>("Values", "Values");
        const auto moveObject = reflection::ObjectView::From(moveOnly);
        auto incoming = std::make_unique<int>(9);

        if (moveProperty.CanInsertMapEntry() || !moveProperty.CanEraseMapEntry() ||
            moveProperty.TryInsertMapEntry(moveObject, numberKeyView, Value::From(incoming)) !=
            Error::InsertUnavailable)
        {
            return Fail("Move-only map insertion capabilities were incorrect.");
        }

        if (moveOnly.Values.size() != 1 || *moveOnly.Values.at(numberKey) != 7 ||
            incoming == nullptr || *incoming != 9 ||
            moveProperty.TryEraseMapEntry(moveObject, numberKeyView) != Error::None ||
            !moveOnly.Values.empty())
        {
            return Fail("Move-only map erasing failed.");
        }

        auto type = reflection::TypeInfo::For<MapSettings>("example::MapSettings");
        type.Properties.push_back(numbers);
        type.Properties.push_back(counts);

        archive_reflection::PropertyCodecs codecs;
        if (!codecs.RegisterMap<std::map<std::int64_t, std::wstring>>() ||
            !codecs.RegisterMap<std::unordered_map<std::wstring, std::uint64_t>>())
        {
            return Fail("Edited map codec registration failed.");
        }

        archive::JsonArchive writer;
        archive::JsonArchive reader;
        MapSettings restored;

        if (!archive_reflection::WriteObject(writer, "Maps", type, object, nullptr, &codecs) ||
            !writer.SaveToFile(directory / "reflection-map-editing.json") ||
            !reader.LoadFromFile(directory / "reflection-map-editing.json") ||
            !archive_reflection::ReadObject(reader, "Maps", type, restored, nullptr, &codecs) ||
            restored.Numbers != settings.Numbers || restored.Counts != settings.Counts)
        {
            return Fail("Edited map archive round-trip failed.");
        }

        std::cout << "Reflection map insertion and erase checks passed.\n";
        std::cout << "Reflection map mutation failure checks passed.\n";
        std::cout << "Edited map archive checks passed.\n";
        return 0;
    }

    struct ObjectMapSettings
    {
        std::map<std::int32_t, Settings> Ordered{ { 1, Settings{} } };
        std::unordered_map<std::wstring, Settings> Unordered{ { L"플레이어", Settings{} } };
    };

    int CheckObjectMapAccess()
    {
        using Error = reflection::PropertyAccessError;
        using Value = reflection::ValueView;

        reflection::Registry registry;
        if (!registry.Register(MakeSettingsType()))
        {
            return Fail("Object map value type registration failed.");
        }

        auto ordered = reflection::MakeProperty<&ObjectMapSettings::Ordered>("Ordered", "Ordered");
        auto unordered = reflection::MakeProperty<&ObjectMapSettings::Unordered>("Unordered", "Unordered");

        ObjectMapSettings settings;
        const auto object = reflection::ObjectView::From(settings);
        const std::int32_t numberKey = 1;
        const std::wstring nameKey = L"플레이어";
        const auto numberKeyView = Value::From(numberKey);
        const auto nameKeyView = Value::From(nameKey);

        if (!ordered.CanEditMapValueObject() || !unordered.CanEditMapValueObject())
        {
            return Fail("Object map editing callbacks were not bound.");
        }

        const auto read = ordered.TryFindMapValue(object, numberKeyView);
        const auto* valueType = read ? registry.FindType(read.Value) : nullptr;

        if (valueType == nullptr)
        {
            return Fail("Object map value type lookup failed.");
        }

        const auto* speed = valueType->FindProperty("Speed");
        const auto* title = valueType->FindProperty("Title");
        if (speed == nullptr || title == nullptr)
        {
            return Fail("Object map value properties were not found.");
        }

        const auto readObject = valueType->AsObject(read.Value);
        const auto* readSpeed = speed->ReadFrom<float>(readObject);
        const float newSpeed = 80.0f;

        if (!readObject.IsValid() || !readObject.IsReadOnly() ||
            readSpeed == nullptr || *readSpeed != settings.Ordered.at(numberKey).Speed ||
            speed->TryWriteValue(readObject, Value::From(newSpeed)) != Error::ReadOnlyObject)
        {
            return Fail("Object map reading did not preserve read-only access.");
        }

        const auto orderedObject = ordered.TryEditMapValueObject(object, numberKeyView);
        const auto unorderedObject = unordered.TryEditMapValueObject(object, nameKeyView);

        if (!orderedObject || !unorderedObject ||
            orderedObject.Object.IsReadOnly() || unorderedObject.Object.IsReadOnly() ||
            registry.FindType(orderedObject.Object) != valueType ||
            registry.FindType(unorderedObject.Object) != valueType)
        {
            return Fail("Object map editing views were incorrect.");
        }

        const std::wstring newTitle = L"수정된 설정";
        if (speed->TryWriteValue(orderedObject.Object, Value::From(newSpeed)) != Error::None ||
            title->TryWriteValue(unorderedObject.Object, Value::From(newTitle)) != Error::None ||
            settings.Ordered.at(numberKey).Speed != newSpeed ||
            settings.Unordered.at(nameKey).Title != newTitle ||
            settings.Ordered.size() != 1 || settings.Unordered.size() != 1)
        {
            return Fail("Editing a mapped object did not update its stored value.");
        }

        const ObjectMapSettings beforeFailure = settings;
        const auto readOnlyObject = reflection::ObjectView::From(beforeFailure);
        const std::int32_t missingNumber = 99;
        const std::wstring missingName = L"없는 키";
        const std::int64_t wrongKey = 1;

        Settings other;
        const auto otherObject = reflection::ObjectView::From(other);

        struct ReadOnlyObjectMaps
        {
            const std::map<std::int32_t, Settings> Ordered{ { 1, Settings{} } };
            const std::unordered_map<std::wstring, Settings> Unordered{ { L"플레이어", Settings{} } };
        };

        ReadOnlyObjectMaps fixed;
        const auto fixedObject = reflection::ObjectView::From(fixed);
        auto fixedOrdered = reflection::MakeProperty<&ReadOnlyObjectMaps::Ordered>("Ordered", "Ordered");
        auto fixedUnordered = reflection::MakeProperty<&ReadOnlyObjectMaps::Unordered>("Unordered", "Unordered");

        MapSettings scalarMaps;
        const auto scalarObject = reflection::ObjectView::From(scalarMaps);
        auto scalarCounts = reflection::MakeProperty<&MapSettings::Counts>("Counts", "Counts");

        if (fixedOrdered.CanEditMapValueObject() || fixedUnordered.CanEditMapValueObject() ||
            scalarCounts.CanEditMapValueObject())
        {
            return Fail("Unsupported mapped object editing was enabled.");
        }

        struct FailureCase
        {
            Error Actual;
            Error Expected;
        };

        const FailureCase failures[] = {
            {
                ordered.TryEditMapValueObject(object, Value::From(missingNumber)).Error,
                Error::KeyNotFound
            },
            {
                unordered.TryEditMapValueObject(object, Value::From(missingName)).Error,
                Error::KeyNotFound
            },
            { ordered.TryEditMapValueObject(object, Value::From(wrongKey)).Error, Error::KeyTypeMismatch },
            { ordered.TryEditMapValueObject(object, {}).Error, Error::InvalidValue },
            { ordered.TryEditMapValueObject({}, numberKeyView).Error, Error::InvalidObject },
            { ordered.TryEditMapValueObject(otherObject, numberKeyView).Error, Error::OwnerTypeMismatch },
            { ordered.TryEditMapValueObject(readOnlyObject, numberKeyView).Error, Error::ReadOnlyObject },
            { unordered.TryEditMapValueObject(readOnlyObject, nameKeyView).Error, Error::ReadOnlyObject },
            { fixedOrdered.TryEditMapValueObject(fixedObject, numberKeyView).Error, Error::ReadOnlyProperty },
            { fixedUnordered.TryEditMapValueObject(fixedObject, nameKeyView).Error, Error::ReadOnlyProperty },
            { scalarCounts.TryEditMapValueObject(scalarObject, nameKeyView).Error, Error::ObjectUnavailable },
            { speed->TryEditMapValueObject(otherObject, numberKeyView).Error, Error::NotMap }
        };

        for (const auto& failure : failures)
        {
            if (failure.Actual != failure.Expected)
            {
                std::cerr << "Expected " << reflection::ToString(failure.Expected)
                    << ", got " << reflection::ToString(failure.Actual) << '\n';
                return Fail("Mapped object editing failure checks failed.");
            }
        }

        if (settings.Ordered.size() != 1 || settings.Unordered.size() != 1 ||
            !settings.Ordered.contains(numberKey) || !settings.Unordered.contains(nameKey) ||
            !SameSettings(settings.Ordered.at(numberKey), beforeFailure.Ordered.at(numberKey)) ||
            !SameSettings(settings.Unordered.at(nameKey), beforeFailure.Unordered.at(nameKey)) ||
            scalarMaps.Counts.size() != 2 || scalarMaps.Counts.at(nameKey) != 3)
        {
            return Fail("Failed mapped object editing changed the destination.");
        }

        struct NonCopyValue
        {
            float Speed = 1.0f;

            NonCopyValue() = default;
            NonCopyValue(const NonCopyValue&) = delete;
            NonCopyValue& operator=(const NonCopyValue&) = delete;
        };

        struct NonCopyMaps
        {
            std::map<std::int32_t, NonCopyValue> Values;
        };

        NonCopyMaps nonCopy;
        nonCopy.Values.try_emplace(numberKey);
        auto nonCopyMap = reflection::MakeProperty<&NonCopyMaps::Values>("Values", "Values");
        auto nonCopySpeed = reflection::MakeProperty<&NonCopyValue::Speed>("Speed", "float");
        const auto nonCopyObject = reflection::ObjectView::From(nonCopy);
        const auto editable = nonCopyMap.TryEditMapValueObject(nonCopyObject, numberKeyView);

        if (nonCopyMap.CanWriteMapValue() || !nonCopyMap.CanEditMapValueObject() || !editable ||
            nonCopySpeed.TryWriteValue(editable.Object, Value::From(newSpeed)) != Error::None ||
            nonCopy.Values.at(numberKey).Speed != newSpeed)
        {
            return Fail("Non-copyable mapped object editing failed.");
        }

        std::cout << "Reflection mapped object read checks passed.\n";
        std::cout << "Reflection mapped object edit checks passed.\n";
        std::cout << "Reflection mapped object failure checks passed.\n";
        return 0;
    }

    bool SameObjectMapSettings(const ObjectMapSettings& lhs, const ObjectMapSettings& rhs)
    {
        if (lhs.Ordered.size() != rhs.Ordered.size() || lhs.Unordered.size() != rhs.Unordered.size())
        {
            return false;
        }

        for (const auto& [key, value] : lhs.Ordered)
        {
            const auto found = rhs.Ordered.find(key);
            if (found == rhs.Ordered.end() || !SameSettings(value, found->second))
            {
                return false;
            }
        }

        for (const auto& [key, value] : lhs.Unordered)
        {
            const auto found = rhs.Unordered.find(key);
            if (found == rhs.Unordered.end() || !SameSettings(value, found->second))
            {
                return false;
            }
        }

        return true;
    }

    int CheckObjectMapArchive(const std::filesystem::path& directory)
    {
        using OrderedMap = decltype(ObjectMapSettings::Ordered);
        using UnorderedMap = decltype(ObjectMapSettings::Unordered);
        using Code = archive::ArchiveErrorCode;

        auto type = reflection::TypeInfo::For<ObjectMapSettings>("example::ObjectMapSettings");
        type.Properties.push_back(reflection::MakeProperty<&ObjectMapSettings::Ordered>("Ordered", "Ordered"));
        type.Properties.push_back(reflection::MakeProperty<&ObjectMapSettings::Unordered>("Unordered", "Unordered"));

        archive_reflection::PropertyCodecs codecs;
        if (!archive_reflection::RegisterObjectMap<OrderedMap>(codecs, MakeSettingsType()) ||
            !archive_reflection::RegisterObjectMap<UnorderedMap>(codecs, MakeSettingsType()) ||
            archive_reflection::RegisterObjectMap<OrderedMap>(codecs, MakeSettingsType()))
        {
            return Fail("Object map codec registration failed.");
        }

        archive_reflection::PropertyCodecs wrongCodecs;
        if (archive_reflection::RegisterObjectMap<OrderedMap>(
            wrongCodecs, reflection::TypeInfo::For<ProfileData>("example::ProfileData")))
        {
            return Fail("An incorrect object map value type was accepted.");
        }

        ObjectMapSettings source;
        source.Ordered.at(1).Speed = 10.0f;
        source.Ordered.try_emplace(2).first->second.Speed = 50.0f;
        source.Unordered.at(L"플레이어").Title = L"플레이어 설정 \U0001F600";
        source.Unordered.try_emplace(L"적").first->second.Speed = 75.0f;

        archive::JsonArchive writer;
        archive::JsonArchive reader;
        ObjectMapSettings restored;
        restored.Ordered = { { 99, Settings{} } };
        restored.Unordered = { { L"Before", Settings{} } };
        restored.Unordered.max_load_factor(0.75f);

        if (!archive_reflection::WriteObject(
            writer, "Maps", type, reflection::ObjectView::From(source), nullptr, &codecs) ||
            !writer.SaveToFile(directory / "reflection-object-maps.json") ||
            !reader.LoadFromFile(directory / "reflection-object-maps.json") ||
            !archive_reflection::ReadObject(reader, "Maps", type, restored, nullptr, &codecs) ||
            !SameObjectMapSettings(restored, source) || restored.Unordered.max_load_factor() != 0.75f)
        {
            return Fail("Object map archive round-trip failed.");
        }

        const std::string validValue =
            R"({"Enabled":true,"Speed":25,"Minimum":0,"Maximum":0,"Name":"value","Title":"한글"})";

        struct FailureCase
        {
            std::string Json;
            const char* Field;
            Code Expected;
        };

        const FailureCase failures[] = {
            { R"({"Maps":{"Ordered":[]}})", "Unordered", Code::MissingField },
            { R"({"Maps":{"Ordered":{},"Unordered":[]}})", "Ordered", Code::InvalidValue },
            { R"({"Maps":{"Ordered":[42]}})", "Ordered[0]", Code::InvalidValue },
            { R"({"Maps":{"Ordered":[{"Value":{}}]}})", "Ordered[0].Key", Code::MissingField },
            { R"({"Maps":{"Ordered":[{"Key":1}]}})", "Ordered[0].Value", Code::MissingField },
            { R"({"Maps":{"Ordered":[{"Key":1,"Value":7}]}})", "Ordered[0].Value", Code::InvalidValue },
            { R"({"Maps":{"Ordered":[{"Key":"bad","Value":{}}]}})", "Ordered[0].Key", Code::InvalidValue },
            { R"({"Maps":{"Ordered":[{"Key":2147483648,"Value":{}}]}})", "Ordered[0].Key", Code::InvalidValue },
            {
                R"({"Maps":{"Ordered":[{"Key":1,"Value":)" + validValue +
                    R"(},{"Key":1,"Value":)" + validValue + R"(}],"Unordered":[]}})",
                "Ordered[1].Key", Code::InvalidValue
            },
            {
                R"({"Maps":{"Ordered":[{"Key":1,"Value":)" + validValue +
                    R"(},{"Key":2,"Value":{"Enabled":true,"Speed":"bad"}}]}})",
                "Ordered[1].Value.Speed", Code::InvalidValue
            },
            {
                R"({"Maps":{"Ordered":[],"Unordered":[{"Key":"플레이어","Value":)" + validValue +
                    R"(},{"Key":"플레이어","Value":)" + validValue + R"(}]}})",
                "Unordered[1].Key", Code::InvalidValue
            },
            {
                R"({"Maps":{"Ordered":[],"Unordered":[{"Key":42,"Value":{}}]}})",
                "Unordered[0].Key", Code::InvalidValue
            }
        };

        for (const auto& failure : failures)
        {
            archive::JsonArchive failureReader;
            if (!failureReader.Parse(failure.Json))
            {
                return Fail("Cannot prepare object map reading failure.");
            }

            ObjectMapSettings unchanged = source;
            const auto result = archive_reflection::ReadObject(
                failureReader, "Maps", type, unchanged, nullptr, &codecs);

            if (result.Code != failure.Expected || result.Field != failure.Field ||
                !SameObjectMapSettings(unchanged, source))
            {
                std::cerr << "Object map error field: " << result.Field << '\n';
                return Fail("Failed object map reading changed the destination.");
            }
        }

        archive::JsonArchive directReader;
        if (!directReader.Parse(R"({"Entries":[{"Key":1,"Value":)" + validValue +
            R"(},{"Key":2,"Value":{"Enabled":true,"Speed":"bad"}}]})"))
        {
            return Fail("Cannot prepare direct object map reading failure.");
        }

        ObjectMapSettings direct = source;
        const auto directResult = directReader.ReadObjectMap("Entries", direct.Ordered,
            [](archive::GlazeArchiveBase& entry, Settings& value) -> archive::ArchiveResult
            {
                const auto result = entry.Read("Enabled", value.Enabled);
                if (!result)
                {
                    return result;
                }

                return entry.Read("Speed", value.Speed);
            });

        if (directResult.Code != Code::InvalidValue || directResult.Field != "Entries[1].Value.Speed" ||
            !SameObjectMapSettings(direct, source))
        {
            return Fail("Direct object map reading was not transactional.");
        }

        std::string beforeFailure;
        if (!writer.ToJson(beforeFailure))
        {
            return Fail("Cannot capture the object map document.");
        }

        const auto expectWriteFailure = [&](const ObjectMapSettings& invalid, std::string_view field)
            {
                const auto result = archive_reflection::WriteObject(
                    writer, "Maps", type, reflection::ObjectView::From(invalid), nullptr, &codecs);
                std::string afterFailure;

                return result.Code == Code::InvalidValue && result.Field == field &&
                    writer.ToJson(afterFailure) && afterFailure == beforeFailure;
            };

        ObjectMapSettings invalid = source;
        invalid.Ordered.at(2).Speed = std::numeric_limits<float>::infinity();

        if (!expectWriteFailure(invalid, "Ordered[1].Value.Speed"))
        {
            return Fail("Failed object map writing changed the document.");
        }

        const auto directWriteResult = writer.WriteObjectMap("Direct", invalid.Ordered,
            [](archive::GlazeArchiveBase& entry, const Settings& value) -> archive::ArchiveResult
            {
                return entry.Write("Speed", value.Speed);
            });
        std::string afterDirectFailure;

        if (directWriteResult.Code != Code::InvalidValue ||
            directWriteResult.Field != "Direct[1].Value.Speed" ||
            !writer.ToJson(afterDirectFailure) || afterDirectFailure != beforeFailure)
        {
            return Fail("Direct object map writing changed the document.");
        }

        invalid = source;
        invalid.Unordered.clear();
        invalid.Unordered.emplace(std::wstring(1, static_cast<wchar_t>(0xD800)), Settings{});

        if (!expectWriteFailure(invalid, "Unordered[0].Key"))
        {
            return Fail("An invalid object map key changed the document.");
        }

        struct DynamicCompare
        {
            bool Descending = false;

            bool operator()(std::int32_t left, std::int32_t right) const
            {
                return Descending ? left > right : left < right;
            }
        };

        struct PolicyMaps
        {
            std::map<std::int32_t, Settings, DynamicCompare> Values{ DynamicCompare{ true } };
        };

        using PolicyMap = decltype(PolicyMaps::Values);
        PolicyMaps policy;
        policy.Values.try_emplace(1);
        policy.Values.try_emplace(2);
        auto policyProperty = reflection::MakeProperty<&PolicyMaps::Values>("Values", "Values");

        archive_reflection::PropertyCodecs policyCodecs;
        if (!archive_reflection::RegisterObjectMap<PolicyMap>(policyCodecs, MakeSettingsType()))
        {
            return Fail("Stateful object map codec registration failed.");
        }

        archive::JsonArchive policyArchive;
        const auto written = policyCodecs.WriteValue(
            policyArchive, "Values", reflection::ValueView::From(policy.Values));

        if (!written || !*written)
        {
            return Fail("Stateful object map writing failed.");
        }

        policy.Values.clear();
        const auto read = policyCodecs.ReadProperty(
            policyArchive, policyProperty, reflection::ObjectView::From(policy));

        if (!read || !*read || policy.Values.size() != 2 ||
            !policy.Values.key_comp().Descending || policy.Values.begin()->first != 2)
        {
            return Fail("Object map reading lost its comparison policy.");
        }

        std::cout << "Reflection object map archive checks passed.\n";
        std::cout << "Reflection object map archive failure checks passed.\n";
        std::cout << "Reflection object map policy checks passed.\n";
        return 0;
    }

    enum class Mode : std::int8_t
    {
        Disabled = -1,
        Ready = 0,
        Active = 1
    };

    enum class Permission : std::uint64_t
    {
        None = 0,
        Read = 1,
        Write = 2,
        High = std::uint64_t{ 1 } << 63,
        All = std::numeric_limits<std::uint64_t>::max()
    };

    constexpr Permission operator|(Permission lhs, Permission rhs)
    {
        return static_cast<Permission>(static_cast<std::uint64_t>(lhs) | static_cast<std::uint64_t>(rhs));
    }

    struct EnumSettings
    {
        float Speed = 25.0f;
        Mode State = Mode::Disabled;
        Permission Bits = Permission::Read | Permission::Write | Permission::High;
        Permission Mask = Permission::All;
    };

    bool SameEnumSettings(const EnumSettings& lhs, const EnumSettings& rhs)
    {
        return lhs.Speed == rhs.Speed && lhs.State == rhs.State && lhs.Bits == rhs.Bits && lhs.Mask == rhs.Mask;
    }

    int CheckEnumArchive(reflection::Registry& registry, const std::filesystem::path& directory)
    {
        auto modeInfo = reflection::EnumInfo::For<Mode>("example::Mode");
        modeInfo.UnderlyingType = "std::int8_t";
        modeInfo.Entries = {
            { "Disabled", std::int64_t{ -1 } }, { "Ready", std::int64_t{ 0 } }, { "Active", std::int64_t{ 1 } }
        };

        auto permissionInfo = reflection::EnumInfo::For<Permission>("example::Permission");
        permissionInfo.UnderlyingType = "std::uint64_t";
        permissionInfo.Entries = {
            { "None", std::uint64_t{ 0 } }, { "Read", std::uint64_t{ 1 } }, { "Write", std::uint64_t{ 2 } },
            { "High", std::uint64_t{ 1 } << 63 }, { "All", std::numeric_limits<std::uint64_t>::max() }
        };

        if (!registry.RegisterEnum(std::move(modeInfo)) || !registry.RegisterEnum(std::move(permissionInfo)))
        {
            return Fail("Enum registration failed.");
        }

        auto type = reflection::TypeInfo::For<EnumSettings>("example::EnumSettings");
        type.Properties.push_back(reflection::MakeProperty<&EnumSettings::Speed>("Speed", "float"));
        type.Properties.push_back(reflection::MakeProperty<&EnumSettings::State>("State", "Mode"));
        type.Properties.push_back(reflection::MakeProperty<&EnumSettings::Bits>("Bits", "Permission"));
        type.Properties.push_back(reflection::MakeProperty<&EnumSettings::Mask>("Mask", "Permission"));

        const EnumSettings source;
        archive::JsonArchive writer;

        if (!archive_reflection::WriteObject(writer, "Enums", type, reflection::ObjectView::From(source), &registry))
        {
            return Fail("Reflected enum writing failed.");
        }

        const auto path = directory / "reflection-enums.json";

        if (!writer.SaveToFile(path))
        {
            return Fail("Reflected enum file saving failed.");
        }

        archive::JsonArchive reader;
        EnumSettings restored;
        restored.Speed = -123.0f;
        restored.State = Mode::Ready;
        restored.Bits = Permission::None;
        restored.Mask = Permission::None;

        if (!reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Enums", type, restored, &registry) ||
            !SameEnumSettings(restored, source))
        {
            return Fail("Reflected enum round-trip failed.");
        }

        using Code = archive::ArchiveErrorCode;

        struct FailureCase
        {
            const char* Json;
            const char* Field;
            Code Expected;
        };

        const FailureCase failures[] = {
            { R"({"Enums":{"Speed":99,"State":128,"Bits":3,"Mask":7}})", "State", Code::InvalidValue },
            { R"({"Enums":{"Speed":99,"State":-129,"Bits":3,"Mask":7}})", "State", Code::InvalidValue },
            { R"({"Enums":{"Speed":99,"State":1.5,"Bits":3,"Mask":7}})", "State", Code::InvalidValue },
            { R"({"Enums":{"Speed":99,"State":"bad","Bits":3,"Mask":7}})", "State", Code::InvalidValue },
            { R"({"Enums":{"Speed":99,"State":1,"Bits":-1,"Mask":7}})", "Bits", Code::InvalidValue },
            { R"({"Enums":{"Speed":99,"State":1,"Bits":1.5,"Mask":7}})", "Bits", Code::InvalidValue },
            { R"({"Enums":{"Speed":99,"State":1,"Mask":7}})", "Bits", Code::MissingField }
        };

        for (const auto& failure : failures)
        {
            archive::JsonArchive failureReader;

            if (!failureReader.Parse(failure.Json))
            {
                return Fail("Cannot prepare enum reading failure.");
            }

            EnumSettings unchanged = source;
            const auto result = archive_reflection::ReadObject(failureReader, "Enums", type, unchanged, &registry);

            if (result.Code != failure.Expected || result.Field != failure.Field ||
                !SameEnumSettings(unchanged, source))
            {
                return Fail("Failed enum reading changed the destination.");
            }
        }

        std::string beforeFailure;

        if (!writer.ToJson(beforeFailure))
        {
            return Fail("Cannot capture the enum document.");
        }

        EnumSettings changed = source;
        changed.Speed = 99.0f;
        const auto result = archive_reflection::WriteObject(
            writer, "Enums", type, reflection::ObjectView::From(changed));

        std::string afterFailure;

        if (result.Code != Code::UnsupportedType || result.Field != "State" ||
            !writer.ToJson(afterFailure) || afterFailure != beforeFailure)
        {
            return Fail("Missing enum registry check failed.");
        }

        std::cout << beforeFailure << '\n';
        std::cout << "Reflection enum archive checks passed.\n";
        std::cout << "Reflection enum archive failure checks passed.\n";
        return 0;
    }

#if defined(ARCHIVE_EXAMPLE_GLM)

    struct MathSettings
    {
        bool Enabled = true;
        glm::vec3 Position{ 1.0f, 2.0f, 3.0f };
        glm::quat Rotation = glm::quat::wxyz(0.8f, 0.0f, 0.0f, 0.6f);
        glm::mat4 Matrix{ 1.0f };
    };

    bool SameMathSettings(const MathSettings& lhs, const MathSettings& rhs)
    {
        const bool samePosition = lhs.Position.x == rhs.Position.x &&
            lhs.Position.y == rhs.Position.y && lhs.Position.z == rhs.Position.z;

        const bool sameRotation = lhs.Rotation.x == rhs.Rotation.x && lhs.Rotation.y == rhs.Rotation.y &&
            lhs.Rotation.z == rhs.Rotation.z && lhs.Rotation.w == rhs.Rotation.w;

        if (lhs.Enabled != rhs.Enabled || !samePosition || !sameRotation)
        {
            return false;
        }

        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                if (lhs.Matrix[column][row] != rhs.Matrix[column][row])
                {
                    return false;
                }
            }
        }

        return true;
    }

    int CheckReflectedMath(const std::filesystem::path& directory)
    {
        archive_reflection::PropertyCodecs codecs;

        if (!codecs.Register<glm::vec3>(archive_glm::WriteVec3, archive_glm::ReadVec3) ||
            !codecs.Register<glm::quat>(archive_glm::WriteQuat, archive_glm::ReadQuat) ||
            !codecs.Register<glm::mat4>(archive_glm::WriteMat4, archive_glm::ReadMat4))
        {
            return Fail("Math codec registration failed.");
        }

        if (codecs.Register<glm::vec3>(archive_glm::WriteVec3, archive_glm::ReadVec3))
        {
            return Fail("Duplicate math codec registration was accepted.");
        }

        auto type = reflection::TypeInfo::For<MathSettings>("example::MathSettings");
        type.Properties.push_back(reflection::MakeProperty<&MathSettings::Enabled>("Enabled", "bool"));
        type.Properties.push_back(reflection::MakeProperty<&MathSettings::Position>("Position", "glm::vec3"));
        type.Properties.push_back(reflection::MakeProperty<&MathSettings::Rotation>("Rotation", "glm::quat"));
        type.Properties.push_back(reflection::MakeProperty<&MathSettings::Matrix>("Matrix", "glm::mat4"));

        MathSettings source;

        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                source.Matrix[column][row] = static_cast<float>(column * 4 + row + 1);
            }
        }

        archive::JsonArchive writer;

        if (!archive_reflection::WriteObject(writer, "Math", type,
            reflection::ObjectView::From(source), nullptr, &codecs))
        {
            return Fail("Reflected math writing failed.");
        }

        const auto path = directory / "reflection-math.json";
        archive::JsonArchive reader;
        MathSettings restored;
        restored.Enabled = false;
        restored.Position = glm::vec3{ -10.0f };
        restored.Rotation = glm::quat::wxyz(1.0f, 0.0f, 0.0f, 0.0f);
        restored.Matrix = glm::mat4{ 0.0f };

        if (!writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Math", type, restored, nullptr, &codecs) ||
            !SameMathSettings(restored, source))
        {
            return Fail("Reflected math round-trip failed.");
        }

        std::array<float, 4> serializedRotation{};
        std::array<float, 16> serializedMatrix{};

        if (!reader.BeginReadObject("Math") ||
            !reader.ReadArray("Rotation", serializedRotation) ||
            !reader.ReadArray("Matrix", serializedMatrix) || !reader.EndObject())
        {
            return Fail("Cannot inspect serialized math components.");
        }

        const std::array<float, 4> expectedRotation{
            source.Rotation.x, source.Rotation.y, source.Rotation.z, source.Rotation.w
        };

        if (serializedRotation != expectedRotation)
        {
            return Fail("Serialized quaternion XYZW order differs.");
        }

        for (int index = 0; index < 16; ++index)
        {
            if (serializedMatrix[index] != static_cast<float>(index + 1))
            {
                return Fail("Serialized matrix column order differs.");
            }
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
                R"({"Math":{"Enabled":false,"Position":[9,"bad",7],"Rotation":[],"Matrix":[]}})",
                "Position", Code::InvalidValue
            },
            {
                R"({"Math":{"Enabled":false,"Position":[9,8,7],"Rotation":[0,0,1],"Matrix":[]}})",
                "Rotation", Code::InvalidValue
            },
            {
                R"({"Math":{"Enabled":false,"Position":[9,8,7],"Rotation":[0,0,0,1],"Matrix":[1,2,3]}})",
                "Matrix", Code::InvalidValue
            },
            {
                R"({"Math":{"Enabled":false,"Position":[9,8,7],"Rotation":[0,0,0,1]}})",
                "Matrix", Code::MissingField
            }
        };

        for (const auto& failure : failures)
        {
            archive::JsonArchive failureReader;

            if (!failureReader.Parse(failure.Json))
            {
                return Fail("Cannot prepare reflected math failure.");
            }

            MathSettings unchanged = source;
            const auto result = archive_reflection::ReadObject(
                failureReader, "Math", type, unchanged, nullptr, &codecs);

            if (result.Code != failure.Expected || result.Field != failure.Field ||
                !SameMathSettings(unchanged, source))
            {
                return Fail("Failed math codec reading changed the destination.");
            }
        }

        std::string beforeFailure;

        if (!writer.ToJson(beforeFailure))
        {
            return Fail("Cannot capture the math document.");
        }

        MathSettings invalid = source;
        invalid.Enabled = false;
        invalid.Position = glm::vec3{ 9.0f, 8.0f, 7.0f };
        invalid.Rotation = glm::quat::wxyz(1.0f, 0.0f, 0.0f, 0.0f);
        invalid.Matrix[3][3] = std::numeric_limits<float>::infinity();

        const auto result = archive_reflection::WriteObject(
            writer, "Math", type, reflection::ObjectView::From(invalid), nullptr, &codecs);

        std::string afterFailure;

        if (result.Code != Code::InvalidValue || result.Field != "Matrix" ||
            !writer.ToJson(afterFailure) || afterFailure != beforeFailure)
        {
            return Fail("Failed math codec writing changed the document.");
        }

        std::cout << beforeFailure << '\n';
        std::cout << "Reflection math codec checks passed.\n";
        std::cout << "Reflection math codec failure checks passed.\n";
        return 0;
    }

#endif
}

int main()
{
    reflection::Registry registry;

    if (!registry.Register(MakeSettingsType()))
    {
        return Fail("Settings registration failed.");
    }

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

    const auto directory = std::filesystem::current_path() / "archive-example";
    std::error_code directoryError;
    std::filesystem::create_directories(directory, directoryError);

    if (directoryError)
    {
        return Fail("Cannot create the reflection archive directory.");
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
        reflection::MakeProperty<&UnsupportedSettings::Values>("Values", "std::vector<std::int32_t>"));

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

#pragma region Read Failure Checks
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
#pragma endregion

    if (CheckEnumArchive(registry, directory) != 0)
    {
        return 1;
    }

#if defined(ARCHIVE_EXAMPLE_GLM)
    if (CheckReflectedMath(directory) != 0)
    {
        return 1;
    }
#endif

    if (CheckNestedArchive(registry, directory) != 0)
    {
        return 1;
    }

    if (CheckContainerArchive(directory) != 0)
    {
        return 1;
    }

    if (CheckSerializationPolicy(directory) != 0)
    {
        return 1;
    }

    if (CheckOptionalFields(registry, directory) != 0)
    {
        return 1;
    }

    if (CheckSetContainers(directory) != 0)
    {
        return 1;
    }

    if (CheckSetMutation(directory) != 0)
    {
        return 1;
    }

    if (CheckMapContainers(directory) != 0)
    {
        return 1;
    }

    if (CheckMapKeyAccess() != 0)
    {
        return 1;
    }

    if (CheckMapMutation(directory) != 0)
    {
        return 1;
    }

    if (CheckObjectMapAccess() != 0)
    {
        return 1;
    }

    if (CheckObjectMapArchive(directory) != 0)
    {
        return 1;
    }

    std::cout << beforeFailure << '\n';
    std::cout << "Reflection archive writing checks passed.\n";
    std::cout << "Reflection archive failure checks passed.\n";
    return 0;
}