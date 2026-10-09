#include <reflection/Reflection.h>

#include "PackageTypes.h"
#include "PackageCheck.gen.h"

#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }
}

int main()
{
    using Error = reflection::PropertyAccessError;
    using Value = reflection::ValueView;

    reflection::Registry registry;

    if (!reflection_generated::Register_PackageCheck(registry))
    {
        return Fail("Installed package registration failed.");
    }

    const auto* settingsType = registry.FindType<PackageSettings>();
    const auto* containerType = registry.FindType<PackageContainers>();

    if (settingsType == nullptr || containerType == nullptr)
    {
        return Fail("Installed package type lookup failed.");
    }

    const auto* speed = settingsType->FindProperty("Speed");
    const auto* samples = containerType->FindProperty("Samples");
    const auto* sorted = containerType->FindProperty("Sorted");
    const auto* tags = containerType->FindProperty("Tags");
    const auto* counts = containerType->FindProperty("Counts");
    const auto* items = containerType->FindProperty("Items");
    const auto* numbers = containerType->FindProperty("m_numbers");

    if (speed == nullptr || samples == nullptr || sorted == nullptr || tags == nullptr ||
        counts == nullptr || items == nullptr || numbers == nullptr)
    {
        return Fail("Installed package property lookup failed.");
    }

    const auto* metadata = speed->Metadata.Find("DisplayName");
    const auto* displayName = metadata ? std::get_if<std::string>(metadata) : nullptr;

    if (displayName == nullptr || *displayName != "Speed")
    {
        return Fail("Installed package metadata lookup failed.");
    }

    PackageSettings settings;

    if (!speed->Write<float>(settings, 12.0f))
    {
        return Fail("Installed package scalar writing failed.");
    }

    const auto* value = speed->Read<float>(settings);

    if (value == nullptr || *value != 12.0f || settings.Speed != 12.0f)
    {
        return Fail("Installed package scalar reading failed.");
    }

    PackageContainers containers;
    const auto object = reflection::ObjectView::From(containers);
    const float replacement = 12.0f;
    const std::int32_t numberKey = 2;
    const std::wstring numberValue = L"추가된 항목 \U0001F600";
    const std::int32_t setValue = 3;
    const std::wstring tagValue = L"추가 태그";
    const std::wstring countKey = L"플레이어";
    const std::uint64_t countValue = 25;

    if (samples->TryWriteElement(object, 1, Value::From(replacement)) != Error::None ||
        numbers->TryInsertMapEntry(object, Value::From(numberKey), Value::From(numberValue)) != Error::None ||
        counts->TryWriteMapValue(object, Value::From(countKey), Value::From(countValue)) != Error::None ||
        sorted->TryInsertSetElement(object, Value::From(setValue)) != Error::None ||
        tags->TryInsertSetElement(object, Value::From(tagValue)) != Error::None)
    {
        return Fail("Installed package STL writing failed.");
    }

    const auto sample = samples->TryReadElement(object, 1);
    const auto mapped = numbers->TryFindMapValue(object, Value::From(numberKey));
    const auto* sampleValue = sample.Value.Get<float>();
    const auto* mappedValue = mapped.Value.Get<std::wstring>();

    if (!sample || !mapped || sampleValue == nullptr || mappedValue == nullptr ||
        *sampleValue != replacement || *mappedValue != numberValue ||
        containers.Counts.at(countKey) != countValue ||
        !containers.Sorted.contains(setValue) || !containers.Tags.contains(tagValue))
    {
        return Fail("Installed package STL reading failed.");
    }

    const auto item = items->TryEditElementObject(object, 0);
    PackageSettings appended;
    appended.Speed = 42.0f;

    if (!item || speed->TryWriteValue(item.Object, Value::From(replacement)) != Error::None ||
        items->TryAppend(object, Value::From(appended)) != Error::None ||
        containers.Items.size() != 2 || containers.Items[0].Speed != replacement ||
        containers.Items[1].Speed != appended.Speed)
    {
        return Fail("Installed package object vector access failed.");
    }

    const auto readOnlyObject = reflection::ObjectView::From(std::as_const(containers));

    if (samples->TryResize(object, 3) != Error::ResizeUnavailable ||
        samples->TryWriteElement(readOnlyObject, 0, Value::From(replacement)) != Error::ReadOnlyObject ||
        sorted->TryInsertSetElement(object, Value::From(setValue)) != Error::ElementAlreadyExists)
    {
        return Fail("Installed package access boundary checks failed.");
    }

    std::map<std::int32_t, std::wstring> visitedNumbers;

    auto result = numbers->TryForEachMapEntry(readOnlyObject,
        [&visitedNumbers](std::size_t index, const Value& key, const Value& mappedValue) -> bool
        {
            const auto* typedKey = key.Get<std::int32_t>();
            const auto* typedValue = mappedValue.Get<std::wstring>();

            if (typedKey == nullptr || typedValue == nullptr || index != visitedNumbers.size())
            {
                return false;
            }

            return visitedNumbers.emplace(*typedKey, *typedValue).second;
        });

    if (result != Error::None || visitedNumbers != containers.GetNumbers())
    {
        return Fail("Installed package private map traversal failed.");
    }

    std::set<std::int32_t> visitedSorted;

    result = sorted->TryForEachElement(readOnlyObject,
        [&visitedSorted](std::size_t index, const Value& element) -> bool
        {
            const auto* typedValue = element.Get<std::int32_t>();

            if (typedValue == nullptr || index != visitedSorted.size())
            {
                return false;
            }

            return visitedSorted.insert(*typedValue).second;
        });

    if (result != Error::None || visitedSorted != containers.Sorted)
    {
        return Fail("Installed package set traversal failed.");
    }

    std::unordered_set<std::wstring> visitedTags;

    result = tags->TryForEachElement(readOnlyObject,
        [&visitedTags](std::size_t index, const Value& element) -> bool
        {
            const auto* typedValue = element.Get<std::wstring>();

            if (typedValue == nullptr || index != visitedTags.size())
            {
                return false;
            }

            return visitedTags.insert(*typedValue).second;
        });

    if (result != Error::None || visitedTags != containers.Tags)
    {
        return Fail("Installed package unordered set traversal failed.");
    }

    std::size_t visitedSamples = 0;
    float sampleSum = 0.0f;

    result = samples->TryForEachElement(readOnlyObject,
        [&visitedSamples, &sampleSum](std::size_t index, const Value& element) -> bool
        {
            const auto* typedValue = element.Get<float>();

            if (typedValue == nullptr || index != visitedSamples)
            {
                return false;
            }

            ++visitedSamples;
            sampleSum += *typedValue;
            return true;
        });

    if (result != Error::None || visitedSamples != containers.Samples.size() ||
        sampleSum != containers.Samples[0] + containers.Samples[1])
    {
        return Fail("Installed package fixed array traversal failed.");
    }

    std::cout << "Installed package generation checks passed.\n";
    std::cout << "Installed package STL access checks passed.\n";
    std::cout << "Installed package traversal checks passed.\n";
    return 0;
}