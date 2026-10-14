#include "TestSupport.h"
#include "SettingsTestData.h"
#include "MapTestData.h"

#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include <reflection/Reflection.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

namespace archive_reflection_tests
{
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
}