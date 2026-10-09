#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>

#include <reflection_archive/ReflectionArchiveAdapter.h>
#include "GeneratedArchiveTypes.h"
#include "ArchiveDemo.gen.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <variant>
#include <array>
#include <utility>
#include <unordered_set>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }

    bool SameProfile(const generated_archive::Profile& lhs, const generated_archive::Profile& rhs)
    {
        return lhs.Name == rhs.Name && lhs.Options.Title == rhs.Options.Title &&
            lhs.Options.Speed == rhs.Options.Speed && lhs.Options.CachedSpeed == rhs.Options.CachedSpeed &&
            lhs.Options.Retries == rhs.Options.Retries && lhs.Tags == rhs.Tags &&
            lhs.Current == rhs.Current && lhs.GetScore() == rhs.GetScore();
    }

    bool SameContainerProfile(
        const generated_archive::ContainerProfile& lhs, const generated_archive::ContainerProfile& rhs)
    {
        const auto sameMap = [](const auto& left, const auto& right)
            {
                if (left.size() != right.size())
                {
                    return false;
                }

                for (const auto& [key, value] : left)
                {
                    const auto found = right.find(key);
                    if (found == right.end())
                    {
                        return false;
                    }

                    const auto& other = found->second;
                    if (value.Title != other.Title || value.Speed != other.Speed ||
                        value.CachedSpeed != other.CachedSpeed || value.Retries != other.Retries)
                    {
                        return false;
                    }
                }

                return true;
            };

        return lhs.Numbers == rhs.Numbers && lhs.Counts == rhs.Counts && lhs.Sorted == rhs.Sorted &&
            sameMap(lhs.GetSettings(), rhs.GetSettings()) &&
            sameMap(lhs.GetNamedSettings(), rhs.GetNamedSettings());
    }

    int CheckGeneratedContainers(const reflection::Registry& registry, const std::filesystem::path& directory)
    {
        using Profile = generated_archive::ContainerProfile;
        using Settings = generated_archive::Settings;
        using SettingsMap = std::map<std::int32_t, Settings>;
        using NamedSettingsMap = std::unordered_map<std::wstring, Settings>;
        using Error = reflection::PropertyAccessError;
        using Value = reflection::ValueView;
        using Code = archive::ArchiveErrorCode;

        const auto* type = registry.FindType<Profile>();
        const auto* settingsType = registry.FindType<Settings>();
        if (type == nullptr || settingsType == nullptr)
        {
            return Fail("Generated container types were not registered.");
        }

        const auto* numbers = type->FindProperty("Numbers");
        const auto* counts = type->FindProperty("Counts");
        const auto* sorted = type->FindProperty("Sorted");
        const auto* settings = type->FindProperty("m_settings");
        const auto* namedSettings = type->FindProperty("m_namedSettings");
        const auto* speed = settingsType->FindProperty("Speed");
        const auto* title = settingsType->FindProperty("Title");
        const auto* retries = settingsType->FindProperty("Retries");

        if (numbers == nullptr || counts == nullptr || sorted == nullptr ||
            settings == nullptr || namedSettings == nullptr ||
            speed == nullptr || title == nullptr || retries == nullptr)
        {
            return Fail("Generated container properties were not found.");
        }

        const auto* classMetadata = type->Metadata.Find("DisplayName");
        const auto* propertyMetadata = settings->Metadata.Find("DisplayName");
        const auto* classLabel = classMetadata ? std::get_if<std::string>(classMetadata) : nullptr;
        const auto* propertyLabel = propertyMetadata ? std::get_if<std::string>(propertyMetadata) : nullptr;

        if (classLabel == nullptr || *classLabel != "Container Profile" ||
            propertyLabel == nullptr || *propertyLabel != "Settings by ID" ||
            !numbers->IsMap() || !counts->IsMap() || !sorted->IsSet() ||
            !settings->CanEditMapValueObject() || !namedSettings->CanEditMapValueObject())
        {
            return Fail("Generated container metadata or bindings were incorrect.");
        }

        Profile source;
        const auto object = reflection::ObjectView::From(source);

        const std::int64_t numberKey = 99;
        const std::int64_t removedNumber = -1;
        const std::wstring numberValue = L"추가된 항목";
        const std::wstring playerKey = L"플레이어";
        const std::wstring enemyKey = L"적";
        const std::uint64_t countValue = 25;
        const std::int32_t setValue = 4;
        const std::int32_t removedSetValue = 1;

        if (numbers->TryInsertMapEntry(object, Value::From(numberKey), Value::From(numberValue)) != Error::None ||
            numbers->TryEraseMapEntry(object, Value::From(removedNumber)) != Error::None ||
            counts->TryInsertMapEntry(object, Value::From(enemyKey), Value::From(countValue)) != Error::None ||
            counts->TryEraseMapEntry(object, Value::From(playerKey)) != Error::None ||
            sorted->TryInsertSetElement(object, Value::From(setValue)) != Error::None ||
            sorted->TryEraseSetElement(object, Value::From(removedSetValue)) != Error::None)
        {
            return Fail("Generated scalar container editing failed.");
        }

        const std::int32_t objectKey = 1;
        const std::int32_t addedObjectKey = 2;
        const float newSpeed = 80.0f;
        const std::int32_t newRetries = 7;
        const std::wstring newTitle = L"수정된 설정 \U0001F600";

        const auto editable = settings->TryEditMapValueObject(object, Value::From(objectKey));
        const auto namedEditable = namedSettings->TryEditMapValueObject(object, Value::From(playerKey));

        if (!editable || !namedEditable ||
            registry.FindType(editable.Object) != settingsType ||
            registry.FindType(namedEditable.Object) != settingsType ||
            speed->TryWriteValue(editable.Object, Value::From(newSpeed)) != Error::None ||
            retries->TryWriteValue(editable.Object, Value::From(newRetries)) != Error::None ||
            title->TryWriteValue(namedEditable.Object, Value::From(newTitle)) != Error::None)
        {
            return Fail("Generated private mapped object editing failed.");
        }

        Settings added;
        added.Title = L"추가한 설정";
        added.Speed = 60.0f;
        added.Retries = 5;

        if (settings->TryInsertMapEntry(object, Value::From(addedObjectKey), Value::From(added)) != Error::None ||
            namedSettings->TryInsertMapEntry(object, Value::From(enemyKey), Value::From(added)) != Error::None ||
            settings->TryInsertMapEntry(object, Value::From(objectKey), Value::From(added)) !=
            Error::KeyAlreadyExists)
        {
            return Fail("Generated private object map insertion failed.");
        }

        if (source.Numbers.size() != 2 || !source.Numbers.contains(numberKey) ||
            source.Numbers.contains(removedNumber) || source.Numbers.at(numberKey) != numberValue ||
            source.Counts.size() != 1 || !source.Counts.contains(enemyKey) ||
            source.Counts.at(enemyKey) != countValue || source.Sorted != std::set<std::int32_t>{ 2, 3, 4 } ||
            source.GetSettings().size() != 2 || !source.GetSettings().contains(objectKey) ||
            !source.GetSettings().contains(addedObjectKey) ||
            source.GetSettings().at(objectKey).Speed != newSpeed ||
            source.GetSettings().at(objectKey).Retries != newRetries ||
            source.GetSettings().at(addedObjectKey).Speed != added.Speed ||
            source.GetNamedSettings().size() != 2 || !source.GetNamedSettings().contains(playerKey) ||
            !source.GetNamedSettings().contains(enemyKey) ||
            source.GetNamedSettings().at(playerKey).Title != newTitle)
        {
            return Fail("Generated container editing did not update the stored values.");
        }

        archive_reflection::PropertyCodecs codecs;
        if (!codecs.RegisterMap<std::map<std::int64_t, std::wstring>>() ||
            !codecs.RegisterMap<std::unordered_map<std::wstring, std::uint64_t>>() ||
            !codecs.RegisterArray<std::set<std::int32_t>>() ||
            !archive_reflection::RegisterObjectMap<SettingsMap>(codecs, *settingsType, &registry) ||
            !archive_reflection::RegisterObjectMap<NamedSettingsMap>(codecs, *settingsType, &registry))
        {
            return Fail("Generated container codec registration failed.");
        }

        archive::JsonArchive writer;
        archive::JsonArchive reader;
        Profile restored;

        if (!archive_reflection::WriteObject(writer, "Containers", *type, object, &registry, &codecs) ||
            !writer.SaveToFile(directory / "reflection-generated-containers.json") ||
            !reader.LoadFromFile(directory / "reflection-generated-containers.json") ||
            !archive_reflection::ReadObject(reader, "Containers", *type, restored, &registry, &codecs) ||
            !SameContainerProfile(restored, source))
        {
            return Fail("Generated container archive round-trip failed.");
        }

        if (!reader.BeginReadObject("Containers") ||
            !reader.BeginReadObjectElement("m_settings", 0) || !reader.BeginReadObject("Value"))
        {
            return Fail("Cannot inspect generated mapped settings.");
        }

        bool hasCache = true;
        bool hasRetries = false;
        const auto cacheResult = reader.HasField("CachedSpeed", hasCache);
        const auto retriesResult = reader.HasField("Retries", hasRetries);
        const auto valueEnd = reader.EndObject();
        const auto entryEnd = reader.EndObject();
        const auto containersEnd = reader.EndObject();

        if (!cacheResult || !retriesResult || !valueEnd || !entryEnd || !containersEnd ||
            hasCache || !hasRetries || reader.GetObjectDepth() != 0)
        {
            return Fail("Generated mapped object serialization metadata was not applied.");
        }

        archive::JsonArchive failureReader;
        const std::string failureJson =
            R"({"Containers":{"Numbers":[{"Key":7,"Value":"Changed"}],"Counts":[],"Sorted":[9],)"
            R"("m_settings":[{"Key":1,"Value":{"Title":"First","Speed":5,"Retries":2}},)"
            R"({"Key":2,"Value":{"Title":"Second","Speed":"bad","Retries":3}}]}})";

        if (!failureReader.Parse(failureJson))
        {
            return Fail("Cannot prepare generated container reading failure.");
        }

        const auto beforeReadFailure = restored;
        const auto readFailure = archive_reflection::ReadObject(
            failureReader, "Containers", *type, restored, &registry, &codecs);

        if (readFailure.Code != Code::InvalidValue ||
            readFailure.Field != "m_settings[1].Value.Speed" ||
            !SameContainerProfile(restored, beforeReadFailure))
        {
            return Fail("Failed generated container reading changed the destination.");
        }

        std::string beforeWriteFailure;
        if (!writer.ToJson(beforeWriteFailure))
        {
            return Fail("Cannot capture the generated container document.");
        }

        Profile invalid = source;
        const auto invalidObject = reflection::ObjectView::From(invalid);
        const auto invalidValue = settings->TryEditMapValueObject(invalidObject, Value::From(objectKey));
        const float infinity = std::numeric_limits<float>::infinity();

        if (!invalidValue ||
            speed->TryWriteValue(invalidValue.Object, Value::From(infinity)) != Error::None)
        {
            return Fail("Cannot prepare generated container writing failure.");
        }

        const auto writeFailure = archive_reflection::WriteObject(
            writer, "Containers", *type, invalidObject, &registry, &codecs);
        std::string afterWriteFailure;

        if (writeFailure.Code != Code::InvalidValue ||
            writeFailure.Field != "m_settings[0].Value.Speed" ||
            !writer.ToJson(afterWriteFailure) || afterWriteFailure != beforeWriteFailure)
        {
            return Fail("Failed generated container writing changed the document.");
        }

        std::cout << "Generated container binding checks passed.\n";
        std::cout << "Generated container archive checks passed.\n";
        std::cout << "Generated container archive failure checks passed.\n";
        return 0;
    }

    int CheckGeneratedObjectArray(const reflection::Registry& registry, const std::filesystem::path& directory)
    {
        using Profile = generated_archive::SequenceProfile;
        using Settings = generated_archive::Settings;
        using Error = reflection::PropertyAccessError;
        using Code = archive::ArchiveErrorCode;
        using Value = reflection::ValueView;

        const auto* type = registry.FindType<Profile>();
        const auto* elementType = registry.FindType<Settings>();

        if (type == nullptr || elementType == nullptr)
        {
            return Fail("Generated object array type lookup failed.");
        }

        const auto* steps = type->FindProperty("m_steps");
        const auto* speed = elementType->FindProperty("Speed");

        if (steps == nullptr || speed == nullptr)
        {
            return Fail("Generated object array property lookup failed.");
        }

        const auto same = [](const Profile& lhs, const Profile& rhs)
            {
                const auto& left = lhs.GetSteps();
                const auto& right = rhs.GetSteps();

                if (left.size() != right.size())
                {
                    return false;
                }

                for (std::size_t index = 0; index < left.size(); ++index)
                {
                    const auto& a = left[index];
                    const auto& b = right[index];

                    if (a.Title != b.Title || a.Speed != b.Speed ||
                        a.CachedSpeed != b.CachedSpeed || a.Retries != b.Retries)
                    {
                        return false;
                    }
                }

                return true;
            };

        Profile source;
        const auto object = reflection::ObjectView::From(source);
        Settings appended;
        appended.Title = L"추가 단계 \U0001F600";
        appended.Speed = 60.0f;
        appended.Retries = 7;

        Settings inserted;
        inserted.Title = L"중간 단계";
        inserted.Speed = 40.0f;

        if (steps->TryResize(object, 2) != Error::None ||
            steps->TryAppend(object, Value::From(appended)) != Error::None ||
            steps->TryInsert(object, 1, Value::From(inserted)) != Error::None)
        {
            return Fail("Generated object array structural access failed.");
        }

        const auto element = steps->TryEditElementObject(object, 2);
        const float changedSpeed = 80.0f;

        if (!element || speed->TryWriteValue(element.Object, Value::From(changedSpeed)) != Error::None)
        {
            return Fail("Generated object array element editing failed.");
        }

        const auto& values = source.GetSteps();

        if (values.size() != 4 || values[0].Speed != 25.0f || values[1].Speed != 40.0f ||
            values[2].Speed != 80.0f || values[3].Speed != 60.0f)
        {
            return Fail("Generated object array binding produced incorrect values.");
        }

        archive_reflection::PropertyCodecs codecs;

        if (!archive_reflection::RegisterObjectArray<Settings>(codecs, *elementType, &registry))
        {
            return Fail("Generated object array codec registration failed.");
        }

        archive::JsonArchive writer;
        archive::JsonArchive reader;
        Profile restored;
        const auto path = directory / "reflection-generated-object-array.json";

        if (!archive_reflection::WriteObject(writer, "Sequence", *type, object, &registry, &codecs) ||
            !writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Sequence", *type, restored, &registry, &codecs) ||
            !same(source, restored))
        {
            return Fail("Generated object array round-trip failed.");
        }

        if (!reader.BeginReadObject("Sequence") || !reader.BeginReadObjectElement("m_steps", 0))
        {
            return Fail("Cannot inspect generated object array metadata.");
        }

        bool hasCache = true;
        bool hasRetries = false;
        const auto cacheResult = reader.HasField("CachedSpeed", hasCache);
        const auto retriesResult = reader.HasField("Retries", hasRetries);
        const auto elementEnd = reader.EndObject();
        const auto sequenceEnd = reader.EndObject();

        if (!cacheResult || !retriesResult || !elementEnd || !sequenceEnd || hasCache || !hasRetries)
        {
            return Fail("Generated object array metadata checks failed.");
        }

        archive::JsonArchive failureReader;
        const std::string failureJson =
            R"({"Sequence":{"m_steps":[{"Title":"First","Speed":5,"Retries":2},)"
            R"({"Title":"Second","Speed":"bad","Retries":3}]}})";

        if (!failureReader.Parse(failureJson))
        {
            return Fail("Cannot prepare generated object array reading failure.");
        }

        const auto beforeReadFailure = restored;
        const auto readFailure = archive_reflection::ReadObject(
            failureReader, "Sequence", *type, restored, &registry, &codecs);

        if (readFailure.Code != Code::InvalidValue || readFailure.Field != "m_steps[1].Speed" ||
            !same(restored, beforeReadFailure))
        {
            return Fail("Failed generated object array reading changed the destination.");
        }

        std::string beforeWriteFailure;

        if (!writer.ToJson(beforeWriteFailure))
        {
            return Fail("Cannot capture the generated object array document.");
        }

        Profile invalid = source;
        const auto invalidObject = reflection::ObjectView::From(invalid);
        const auto invalidElement = steps->TryEditElementObject(invalidObject, 1);
        const float infinity = std::numeric_limits<float>::infinity();

        if (!invalidElement || speed->TryWriteValue(invalidElement.Object, Value::From(infinity)) != Error::None)
        {
            return Fail("Cannot prepare generated object array writing failure.");
        }

        const auto writeFailure = archive_reflection::WriteObject(
            writer, "Sequence", *type, invalidObject, &registry, &codecs);
        std::string afterWriteFailure;

        if (writeFailure.Code != Code::InvalidValue || writeFailure.Field != "m_steps[1].Speed" ||
            !writer.ToJson(afterWriteFailure) || afterWriteFailure != beforeWriteFailure)
        {
            return Fail("Failed generated object array writing changed the document.");
        }

        archive::JsonArchive replacementReader;

        if (!replacementReader.Parse(R"({"Sequence":{"m_steps":[{"Title":"Legacy","Speed":10}]}})") ||
            !archive_reflection::ReadObject(
                replacementReader, "Sequence", *type, restored, &registry, &codecs))
        {
            return Fail("Generated object array replacement failed.");
        }

        const auto& replaced = restored.GetSteps();

        if (replaced.size() != 1 || replaced[0].Title != L"Legacy" ||
            replaced[0].Speed != 10.0f || replaced[0].Retries != 3 ||
            replaced[0].CachedSpeed != std::numeric_limits<float>::infinity())
        {
            return Fail("Generated object array default field checks failed.");
        }

        if (!replacementReader.Parse(R"({"Sequence":{"m_steps":[]}})") ||
            !archive_reflection::ReadObject(
                replacementReader, "Sequence", *type, restored, &registry, &codecs) ||
            !restored.GetSteps().empty())
        {
            return Fail("Generated empty object array replacement failed.");
        }

        std::cout << "Generated object array binding checks passed.\n";
        std::cout << "Generated object array archive checks passed.\n";
        std::cout << "Generated object array archive failure checks passed.\n";
        return 0;
    }

    int CheckGeneratedFixedArray(const reflection::Registry& registry, const std::filesystem::path& directory)
    {
        using Profile = generated_archive::FixedProfile;
        using ReadOnlyProfile = generated_archive::ReadOnlyArrayProfile;
        using State = generated_archive::State;
        using Error = reflection::PropertyAccessError;
        using Value = reflection::ValueView;

        const auto* type = registry.FindType<Profile>();
        const auto* settingsType = registry.FindType<generated_archive::Settings>();
        const auto* readOnlyType = registry.FindType<ReadOnlyProfile>();

        if (type == nullptr || settingsType == nullptr || readOnlyType == nullptr)
        {
            return Fail("Generated fixed array type lookup failed.");
        }

        const auto* samples = type->FindProperty("m_samples");
        const auto* flags = type->FindProperty("Flags");
        const auto* states = type->FindProperty("States");
        const auto* empty = type->FindProperty("Empty");
        const auto* objects = type->FindProperty("Objects");
        const auto* speed = settingsType->FindProperty("Speed");
        const auto* fixedValues = readOnlyType->FindProperty("Values");

        if (samples == nullptr || flags == nullptr || states == nullptr || empty == nullptr ||
            objects == nullptr || speed == nullptr || fixedValues == nullptr)
        {
            return Fail("Generated fixed array property lookup failed.");
        }

        const auto same = [](const Profile& lhs, const Profile& rhs)
            {
                if (lhs.GetSamples() != rhs.GetSamples() || lhs.Flags != rhs.Flags ||
                    lhs.States != rhs.States || lhs.Empty != rhs.Empty)
                {
                    return false;
                }

                for (std::size_t index = 0; index < lhs.Objects.size(); ++index)
                {
                    const auto& a = lhs.Objects[index];
                    const auto& b = rhs.Objects[index];

                    if (a.Title != b.Title || a.Speed != b.Speed ||
                        a.CachedSpeed != b.CachedSpeed || a.Retries != b.Retries)
                    {
                        return false;
                    }
                }

                return true;
            };

        Profile source;
        const auto object = reflection::ObjectView::From(source);
        const float number = 50.0f;
        const bool enabled = true;

        const auto size = samples->TryGetSize(object);

        if (!size || size.Size != 3 ||
            samples->TryWriteElement(object, 1, Value::From(number)) != Error::None ||
            flags->TryWriteElement(object, 0, Value::From(enabled)) != Error::None ||
            states->TryWriteEnumElement(object, 1, reflection::EnumValue{ std::uint64_t{ 1 } }) != Error::None)
        {
            return Fail("Generated fixed array element writing failed.");
        }

        const auto sample = samples->TryReadElement(object, 1);
        const auto flag = flags->TryReadElement(object, 0);
        const auto* readNumber = sample.Value.Get<float>();
        const auto* readFlag = flag.Value.Get<bool>();

        if (!sample || !flag || readNumber == nullptr || readFlag == nullptr ||
            *readNumber != number || !*readFlag || source.States[1] != State::Active)
        {
            return Fail("Generated fixed array element reading failed.");
        }

        const auto element = objects->TryEditElementObject(object, 0);

        if (!element || speed->TryWriteValue(element.Object, Value::From(number)) != Error::None ||
            source.Objects[0].Speed != number)
        {
            return Fail("Generated fixed array object editing failed.");
        }

        const auto emptySize = empty->TryGetSize(object);
        const auto readOnlyObject = reflection::ObjectView::From(std::as_const(source));
        ReadOnlyProfile fixed;
        const auto fixedObject = reflection::ObjectView::From(fixed);

        if (!emptySize || emptySize.Size != 0 ||
            empty->TryReadElement(object, 0).Error != Error::IndexOutOfRange ||
            samples->TryReadElement(object, 3).Error != Error::IndexOutOfRange ||
            samples->TryWriteElement(readOnlyObject, 0, Value::From(number)) != Error::ReadOnlyObject ||
            fixedValues->TryWriteElement(fixedObject, 0, Value::From(number)) != Error::ReadOnlyProperty ||
            states->TryWriteEnumElement(object, 1, reflection::EnumValue{ std::uint64_t{ 300 } }) !=
            Error::ValueOutOfRange || source.States[1] != State::Active)
        {
            return Fail("Generated fixed array boundary checks failed.");
        }

        if (samples->CanResize() || samples->CanClear() || samples->CanAppend() ||
            samples->CanInsert() || samples->CanErase() ||
            samples->TryResize(object, 4) != Error::ResizeUnavailable)
        {
            return Fail("Generated fixed array exposed structural mutation.");
        }

        archive_reflection::PropertyCodecs codecs;

        if (!codecs.RegisterArray<std::array<float, 3>>() ||
            !codecs.RegisterArray<std::array<bool, 2>>() ||
            !codecs.RegisterArray<std::array<State, 2>>() ||
            !codecs.RegisterArray<std::array<std::int32_t, 0>>())
        {
            return Fail("Generated fixed array codec registration failed.");
        }

        archive::JsonArchive writer;
        archive::JsonArchive reader;
        Profile restored;
        restored.Objects[0].Speed = 123.0f;
        auto expected = source;
        expected.Objects[0].Speed = 123.0f;
        const auto path = directory / "reflection-generated-fixed-array.json";

        if (!archive_reflection::WriteObject(writer, "Fixed", *type, object, &registry, &codecs) ||
            !writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Fixed", *type, restored, &registry, &codecs) ||
            !same(restored, expected))
        {
            return Fail("Generated fixed array archive round-trip failed.");
        }

        archive::JsonArchive failureReader;
        const std::string failureJson =
            R"({"Fixed":{"Flags":[false,false],"States":[0,0],"Empty":[],"m_samples":[1,2]}})";

        if (!failureReader.Parse(failureJson))
        {
            return Fail("Cannot prepare generated fixed array reading failure.");
        }

        const auto beforeFailure = restored;
        const auto failure = archive_reflection::ReadObject(
            failureReader, "Fixed", *type, restored, &registry, &codecs);

        if (failure.Code != archive::ArchiveErrorCode::InvalidValue ||
            failure.Field != "m_samples" || !same(restored, beforeFailure))
        {
            return Fail("Invalid fixed array length changed the destination.");
        }

        std::cout << "Generated fixed array access checks passed.\n";
        std::cout << "Generated fixed array archive checks passed.\n";
        std::cout << "Generated fixed array failure checks passed.\n";
        return 0;
    }

    int CheckGeneratedContainerTraversal(const reflection::Registry& registry)
    {
        using Profile = generated_archive::ContainerProfile;
        using FixedProfile = generated_archive::FixedProfile;
        using Error = reflection::PropertyAccessError;
        using Value = reflection::ValueView;

        const auto* type = registry.FindType<Profile>();
        const auto* profileType = registry.FindType<generated_archive::Profile>();
        const auto* fixedType = registry.FindType<FixedProfile>();

        if (type == nullptr || profileType == nullptr || fixedType == nullptr)
        {
            return Fail("Generated traversal type lookup failed.");
        }

        const auto* numbers = type->FindProperty("Numbers");
        const auto* counts = type->FindProperty("Counts");
        const auto* sorted = type->FindProperty("Sorted");
        const auto* tags = profileType->FindProperty("Tags");
        const auto* samples = fixedType->FindProperty("m_samples");
        const auto* empty = fixedType->FindProperty("Empty");

        if (numbers == nullptr || counts == nullptr || sorted == nullptr ||
            tags == nullptr || samples == nullptr || empty == nullptr)
        {
            return Fail("Generated traversal property lookup failed.");
        }

        Profile source;

        for (std::int64_t index = 0; index < 64; ++index)
        {
            source.Numbers.emplace(index, L"순회 항목");
        }

        const auto object = reflection::ObjectView::From(std::as_const(source));
        std::map<std::int64_t, std::wstring> visitedNumbers;

        auto result = numbers->TryForEachMapEntry(object,
            [&visitedNumbers](std::size_t index, const Value& key, const Value& value) -> bool
            {
                const auto* typedKey = key.Get<std::int64_t>();
                const auto* typedValue = value.Get<std::wstring>();

                if (typedKey == nullptr || typedValue == nullptr || index != visitedNumbers.size())
                {
                    return false;
                }

                return visitedNumbers.emplace(*typedKey, *typedValue).second;
            });

        if (result != Error::None || visitedNumbers != source.Numbers)
        {
            return Fail("Generated map traversal failed.");
        }

        std::unordered_map<std::wstring, std::uint64_t> visitedCounts;

        result = counts->TryForEachMapEntry(object,
            [&visitedCounts](std::size_t index, const Value& key, const Value& value) -> bool
            {
                const auto* typedKey = key.Get<std::wstring>();
                const auto* typedValue = value.Get<std::uint64_t>();

                if (typedKey == nullptr || typedValue == nullptr || index != visitedCounts.size())
                {
                    return false;
                }

                return visitedCounts.emplace(*typedKey, *typedValue).second;
            });

        if (result != Error::None || visitedCounts != source.Counts)
        {
            return Fail("Generated unordered map traversal failed.");
        }

        std::set<std::int32_t> visitedSorted;

        result = sorted->TryForEachElement(object,
            [&visitedSorted](std::size_t index, const Value& value) -> bool
            {
                const auto* typedValue = value.Get<std::int32_t>();

                if (typedValue == nullptr || index != visitedSorted.size())
                {
                    return false;
                }

                return visitedSorted.insert(*typedValue).second;
            });

        if (result != Error::None || visitedSorted != source.Sorted)
        {
            return Fail("Generated set traversal failed.");
        }

        const generated_archive::Profile tagged;
        const auto taggedObject = reflection::ObjectView::From(tagged);
        std::unordered_set<std::wstring> visitedTags;

        result = tags->TryForEachElement(taggedObject,
            [&visitedTags](std::size_t index, const Value& value) -> bool
            {
                const auto* typedValue = value.Get<std::wstring>();

                if (typedValue == nullptr || index != visitedTags.size())
                {
                    return false;
                }

                return visitedTags.insert(*typedValue).second;
            });

        if (result != Error::None || visitedTags != tagged.Tags)
        {
            return Fail("Generated unordered set traversal failed.");
        }

        const FixedProfile fixed;
        const auto fixedObject = reflection::ObjectView::From(fixed);
        std::size_t visitedSamples = 0;

        result = samples->TryForEachElement(fixedObject,
            [&fixed, &visitedSamples](std::size_t index, const Value& value) -> bool
            {
                const auto* typedValue = value.Get<float>();

                if (typedValue == nullptr || index != visitedSamples ||
                    index >= fixed.GetSamples().size() || *typedValue != fixed.GetSamples()[index])
                {
                    return false;
                }

                ++visitedSamples;
                return true;
            });

        if (result != Error::None || visitedSamples != fixed.GetSamples().size())
        {
            return Fail("Generated indexed container traversal failed.");
        }

        std::size_t calls = 0;

        const auto stop = [&calls](std::size_t, const Value&, const Value&) -> bool
            {
                ++calls;
                return false;
            };

        if (numbers->TryForEachMapEntry(object, stop) != Error::None || calls != 1)
        {
            return Fail("Generated traversal early stopping failed.");
        }

        calls = 0;

        const auto visit = [&calls](std::size_t, const Value&) -> bool
            {
                ++calls;
                return true;
            };

        if (empty->TryForEachElement(fixedObject, visit) != Error::None || calls != 0 ||
            numbers->TryForEachMapEntry({}, stop) != Error::InvalidObject ||
            numbers->TryForEachMapEntry(fixedObject, stop) != Error::OwnerTypeMismatch ||
            sorted->TryForEachMapEntry(object, stop) != Error::NotMap || calls != 0)
        {
            return Fail("Generated traversal validation failed.");
        }

        source.Numbers.clear();

        if (numbers->TryForEachMapEntry(object, stop) != Error::None || calls != 0)
        {
            return Fail("Generated empty map traversal failed.");
        }

        std::cout << "Generated container traversal checks passed.\n";
        std::cout << "Generated container traversal boundary checks passed.\n";
        return 0;
    }
}

int main()
{
    reflection::Registry registry;

    if (!reflection_generated::Register_ArchiveDemo(registry))
    {
        return Fail("Generated archive registration failed.");
    }

    const auto* type = registry.FindType<generated_archive::Profile>();

    if (type == nullptr)
    {
        return Fail("Generated profile lookup failed.");
    }

    archive_reflection::PropertyCodecs codecs;

    if (!codecs.RegisterArray<std::unordered_set<std::wstring>>())
    {
        return Fail("Generated archive codec registration failed.");
    }

    generated_archive::Profile source;
    source.SetScore(42);
    archive::JsonArchive writer;

    if (!archive_reflection::WriteObject(
        writer, "Profile", *type, reflection::ObjectView::From(source), &registry, &codecs))
    {
        return Fail("Generated reflected writing failed.");
    }

    const auto directory = std::filesystem::current_path() / "archive-example";
    std::error_code directoryError;
    std::filesystem::create_directories(directory, directoryError);

    if (directoryError)
    {
        return Fail("Cannot create the generated archive directory.");
    }

    const auto path = directory / "reflection-generated.json";
    archive::JsonArchive reader;
    generated_archive::Profile restored;
    restored.Name = L"Before";
    restored.Options.Title = L"Before";
    restored.Options.Speed = -1.0f;
    restored.Options.CachedSpeed = 123.0f;
    restored.Options.Retries = -1;
    restored.Tags = { L"Before" };
    restored.Current = generated_archive::State::Idle;
    restored.SetScore(-1);

    auto expected = source;
    expected.Options.CachedSpeed = 123.0f;

    if (!writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
        !archive_reflection::ReadObject(reader, "Profile", *type, restored, &registry, &codecs) ||
        !SameProfile(restored, expected))
    {
        return Fail("Generated reflected round-trip failed.");
    }

    if (!reader.BeginReadObject("Profile") || !reader.BeginReadObject("Options"))
    {
        return Fail("Cannot inspect generated settings.");
    }

    bool hasCache = true;
    bool hasRetries = false;
    const auto cacheResult = reader.HasField("CachedSpeed", hasCache);
    const auto retriesResult = reader.HasField("Retries", hasRetries);
    const auto optionsEnd = reader.EndObject();
    const auto profileEnd = reader.EndObject();

    if (!cacheResult || !retriesResult || !optionsEnd || !profileEnd || hasCache || !hasRetries)
    {
        return Fail("Generated serialization metadata was not applied.");
    }

    archive::JsonArchive legacyReader;
    const std::string legacyJson =
        R"({"Profile":{"Name":"Legacy","Options":{"Title":"Old","Speed":10,"CachedSpeed":"ignored"},)"
        R"("Tags":[],"Current":1,"m_score":5}})";

    restored.Options.Retries = 77;

    if (!legacyReader.Parse(legacyJson) ||
        !archive_reflection::ReadObject(legacyReader, "Profile", *type, restored, &registry, &codecs) ||
        restored.Name != L"Legacy" || restored.Options.Title != L"Old" ||
        restored.Options.Speed != 10.0f || restored.Options.CachedSpeed != 123.0f ||
        restored.Options.Retries != 77 || !restored.Tags.empty() ||
        restored.Current != generated_archive::State::Active || restored.GetScore() != 5)
    {
        return Fail("Generated optional field checks failed.");
    }

    archive::JsonArchive failureReader;

    if (!failureReader.Parse(R"({"Profile":{"Name":"Changed","Options":{"Title":42}}})"))
    {
        return Fail("Cannot prepare generated reading failure.");
    }

    const auto beforeFailure = restored;
    const auto failure = archive_reflection::ReadObject(
        failureReader, "Profile", *type, restored, &registry, &codecs);

    if (failure.Code != archive::ArchiveErrorCode::InvalidValue ||
        failure.Field != "Options.Title" || !SameProfile(restored, beforeFailure))
    {
        return Fail("Failed generated reading changed the destination.");
    }

    std::string document;

    if (!writer.ToJson(document))
    {
        return Fail("Generated document formatting failed.");
    }

    if (CheckGeneratedContainers(registry, directory) != 0)
    {
        return 1;
    }

    if (CheckGeneratedObjectArray(registry, directory) != 0)
    {
        return 1;
    }

    if (CheckGeneratedFixedArray(registry, directory) != 0)
    {
        return 1;
    }

    if (CheckGeneratedContainerTraversal(registry) != 0)
    {
        return 1;
    }

    std::cout << document << '\n';
    std::cout << "Generated reflection archive checks passed.\n";
    std::cout << "Generated reflection archive policy checks passed.\n";
    std::cout << "Generated reflection archive failure checks passed.\n";
    return 0;
}