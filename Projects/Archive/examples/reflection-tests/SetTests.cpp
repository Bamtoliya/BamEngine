#include "TestSupport.h"
#include "SettingsTestData.h"

#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include <reflection/Reflection.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>

namespace archive_reflection_tests
{
    namespace
    {
        struct SetSettings
        {
            std::set<std::int32_t> Sorted{ 3, 1, 2 };
            std::unordered_set<std::wstring> Tags{ L"플레이어", L"이동" };
        };
    }

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

        archive::JsonArchive writer;
        archive::JsonArchive reader;
        SetSettings restored;
        restored.Sorted = { 99 };
        restored.Tags = { L"Before" };

        if (!archive_reflection::WriteObject(
            writer, "Sets", type, reflection::ObjectView::From(source)) ||
            !writer.SaveToFile(directory / "reflection-sets.json") ||
            !reader.LoadFromFile(directory / "reflection-sets.json") ||
            !archive_reflection::ReadObject(reader, "Sets", type, restored) ||
            restored.Sorted != source.Sorted || restored.Tags != source.Tags)
        {
            return Fail("Reflected set round-trip failed.");
        }

        SetSettings emptySource;
        emptySource.Sorted.clear();
        emptySource.Tags.clear();

        archive::JsonArchive emptyWriter;
        archive::JsonArchive emptyReader;
        std::string emptyDocument;
        SetSettings emptyRestored = source;

        if (!archive_reflection::WriteObject(emptyWriter, "Sets", type, reflection::ObjectView::From(emptySource)) ||
            !emptyWriter.ToJson(emptyDocument) || !emptyReader.Parse(emptyDocument) ||
            !archive_reflection::ReadObject(emptyReader, "Sets", type, emptyRestored) ||
            !emptyRestored.Sorted.empty() || !emptyRestored.Tags.empty())
        {
            return Fail("Automatic empty set round-trip failed.");
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
                failureReader, "Sets", type, unchanged);

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
}