#include "TestSupport.h"
#include "../GeneratedArchiveTypes.h"
#include "ArchiveDemo.gen.h"

#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

namespace archive_generated_tests
{
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

        archive::JsonArchive writer;
        archive::JsonArchive reader;
        Profile restored;
        restored.Objects[0].Speed = 123.0f;
        auto expected = source;
        const auto path = directory / "reflection-generated-fixed-array.json";

        if (!archive_reflection::WriteObject(writer, "Fixed", *type, object, &registry) ||
            !writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Fixed", *type, restored, &registry) ||
            !same(restored, expected))
        {
            return Fail("Generated fixed array archive round-trip failed.");
        }

        struct FailureCase
        {
            const char* Json;
            const char* Field;
        };

        const FailureCase failures[] = {
            {
                R"({"Fixed":{"Flags":[false,false],"States":[0,0],"Empty":[],)"
                R"("Objects":[{"Title":"A","Speed":1},{"Title":"B","Speed":2}],"m_samples":[1,2]}})",
                "m_samples"
            },
            {
                R"({"Fixed":{"Flags":[false,false],"States":[0,0],"Empty":[],)"
                R"("Objects":[{"Title":"A","Speed":1},{"Title":"B","Speed":2}],"m_samples":[1,2,3,4]}})",
                "m_samples"
            },
            {
                R"({"Fixed":{"Flags":[false,false],"States":[0,0],"Empty":[],)"
                R"("Objects":[{"Title":"A","Speed":1},{"Title":"B","Speed":2}],"m_samples":[1,"bad",3]}})",
                "m_samples"
            },
            {
                R"({"Fixed":{"Flags":[false,false],"States":[0,0],"Empty":[1],"m_samples":[1,2,3]}})",
                "Empty"
            },
            {
                R"({"Fixed":{"Flags":[true,1]}})",
                "Flags"
            }
        };

        for (const auto& failure : failures)
        {
            archive::JsonArchive failureReader;

            if (!failureReader.Parse(failure.Json))
            {
                return Fail("Cannot prepare generated fixed array reading failure.");
            }

            const auto beforeFailure = restored;
            const auto result = archive_reflection::ReadObject(
                failureReader, "Fixed", *type, restored, &registry);

            if (result.Code != archive::ArchiveErrorCode::InvalidValue ||
                result.Field != failure.Field || !same(restored, beforeFailure))
            {
                return Fail("Invalid fixed array reading changed the destination.");
            }
        }

        std::cout << "Generated fixed array access checks passed.\n";
        std::cout << "Generated fixed array archive checks passed.\n";
        std::cout << "Generated fixed array failure checks passed.\n";
        return 0;
    }
}
