#include "TestSupport.h"
#include "SettingsTestData.h"
#include "ProfileTestData.h"
#include "MapTestData.h"

#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include <reflection/Reflection.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>

namespace archive_reflection_tests
{
    namespace
    {
        struct ObjectMapSettings
        {
            std::map<std::int32_t, Settings> Ordered{ { 1, Settings{} } };
            std::unordered_map<std::wstring, Settings> Unordered{ { L"플레이어", Settings{} } };
        };
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
    }

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
            [](archive::IArchive& entry, Settings& value) -> archive::ArchiveResult
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
            [](archive::IArchive& entry, const Settings& value) -> archive::ArchiveResult
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
}