#include "TestSupport.h"
#include "../GeneratedArchiveTypes.h"
#include "ArchiveDemo.gen.h"

#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace archive_generated_tests
{
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
