#include "TestSupport.h"
#include "../GeneratedArchiveTypes.h"
#include "ArchiveDemo.gen.h"

#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <variant>

namespace archive_generated_tests
{
    namespace
    {
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
}
