#include "TestSupport.h"
#include "../GeneratedArchiveTypes.h"
#include "ArchiveDemo.gen.h"

#include <archive/JsonArchive.h>
#include <archive/BeveArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace archive_generated_tests
{
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

        archive::JsonArchive writer;
        archive::JsonArchive reader;
        Profile restored;
        const auto path = directory / "reflection-generated-object-array.json";

        if (!archive_reflection::WriteObject(writer, "Sequence", *type, object, &registry) ||
            !writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Sequence", *type, restored, &registry) ||
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
            failureReader, "Sequence", *type, restored, &registry);

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
            writer, "Sequence", *type, invalidObject, &registry);
        std::string afterWriteFailure;

        if (writeFailure.Code != Code::InvalidValue || writeFailure.Field != "m_steps[1].Speed" ||
            !writer.ToJson(afterWriteFailure) || afterWriteFailure != beforeWriteFailure)
        {
            return Fail("Failed generated object array writing changed the document.");
        }

        archive::JsonArchive replacementReader;
        const auto restoredElement = steps->TryEditElementObject(reflection::ObjectView::From(restored), 0);
        const auto* retries = elementType->FindProperty("Retries");
        const std::int32_t oldRetries = 77;

        if (!restoredElement || retries == nullptr ||
            retries->TryWriteValue(restoredElement.Object, Value::From(oldRetries)) != Error::None)
        {
            return Fail("Cannot prepare existing optional object array field.");
        }

        if (!replacementReader.Parse(R"({"Sequence":{"m_steps":[{"Title":"Legacy","Speed":10}]}})") ||
            !archive_reflection::ReadObject(
                replacementReader, "Sequence", *type, restored, &registry))
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
                replacementReader, "Sequence", *type, restored, &registry) ||
            !restored.GetSteps().empty())
        {
            return Fail("Generated empty object array replacement failed.");
        }

        const auto checkRoundTrip = [&]<typename Archive>(const Profile& expected)
        {
            Archive output;
            Archive input;
            std::string encoded;
            Profile actual = source;

            return archive_reflection::WriteObject(output, "Sequence", *type,
                reflection::ObjectView::From(expected), &registry) && output.Encode(encoded) && input.Decode(encoded) &&
                archive_reflection::ReadObject(input, "Sequence", *type, actual, &registry) && same(expected, actual);
        };

        if (!checkRoundTrip.operator()<archive::JsonArchive>(source) ||
            !checkRoundTrip.operator()<archive::BeveArchive>(source) ||
            !checkRoundTrip.operator()<archive::JsonArchive>(restored) ||
            !checkRoundTrip.operator()<archive::BeveArchive>(restored))
        {
            return Fail("Automatic object array JSON/BEVE or empty round-trip failed.");
        }

        struct InvalidInput
        {
            std::string_view Json;
            Code ExpectedCode;
            std::string_view Field;
        };

        const InvalidInput invalidInputs[]{
            { R"({"Sequence":{}})", Code::MissingField, "m_steps" },
            { R"({"Sequence":{"m_steps":{}}})", Code::InvalidValue, "m_steps" },
            { R"({"Sequence":{"m_steps":[7]}})", Code::InvalidValue, "m_steps[0]" },
            { R"({"Sequence":{"m_steps":[{"Speed":5}]}})", Code::MissingField, "m_steps[0].Title" }
        };

        for (const auto& input : invalidInputs)
        {
            Profile destination = source;

            if (!failureReader.Parse(input.Json))
            {
                return Fail("Cannot prepare invalid automatic object array input.");
            }

            const auto result = archive_reflection::ReadObject(
                failureReader, "Sequence", *type, destination, &registry);

            if (result.Code != input.ExpectedCode || result.Field != input.Field || !same(source, destination))
            {
                return Fail("Automatic object array failure path or destination preservation failed.");
            }
        }

        const reflection::Registry missingElementRegistry;

        for (const auto* lookup : { static_cast<const reflection::Registry*>(nullptr), &missingElementRegistry })
        {
            for (const auto* value : { &source, &restored })
            {
                const auto result = archive_reflection::WriteObject(
                    writer, "Sequence", *type, reflection::ObjectView::From(*value), lookup);

                if (result.Code != Code::UnsupportedType || result.Field != "m_steps" ||
                    !writer.ToJson(afterWriteFailure) || afterWriteFailure != beforeWriteFailure)
                {
                    return Fail("Missing object array registration changed the document or accepted an empty array.");
                }
            }

            for (const auto* input : { &reader, &replacementReader })
            {
                Profile destination = source;
                const auto result = archive_reflection::ReadObject(*input, "Sequence", *type, destination, lookup);

                if (result.Code != Code::UnsupportedType || result.Field != "m_steps" || !same(source, destination))
                {
                    return Fail("Missing object array registration changed the destination.");
                }
            }
        }

        archive_reflection::PropertyCodecs codecs;
        Profile explicitRestored;

        if (!archive_reflection::RegisterObjectArray<Settings>(codecs, *elementType, &registry) ||
            !archive_reflection::WriteObject(writer, "Sequence", *type, object, &registry, &codecs) ||
            !archive_reflection::ReadObject(reader, "Sequence", *type, explicitRestored, &registry, &codecs) ||
            !same(source, explicitRestored))
        {
            return Fail("Explicit object array codec compatibility failed.");
        }

        archive_reflection::PropertyCodecs rejectingCodecs;

        if (!rejectingCodecs.Register<std::vector<Settings>>(
            [](archive::IArchive&, std::string_view key, const std::vector<Settings>&) -> archive::ArchiveResult
            {
                return { Code::UnsupportedType, std::string{ key } };
            },
            [](const archive::IArchive&, std::string_view key, std::vector<Settings>&) -> archive::ArchiveResult
            {
                return { Code::UnsupportedType, std::string{ key } };
            }))
        {
            return Fail("Cannot register rejecting object array codec.");
        }

        Profile destination = source;
        const auto rejectedWrite = archive_reflection::WriteObject(
            writer, "Sequence", *type, object, &registry, &rejectingCodecs);
        const auto rejectedRead = archive_reflection::ReadObject(
            reader, "Sequence", *type, destination, &registry, &rejectingCodecs);

        if (rejectedWrite.Code != Code::UnsupportedType || rejectedWrite.Field != "m_steps" ||
            rejectedRead.Code != Code::UnsupportedType || rejectedRead.Field != "m_steps" ||
            !same(source, destination) || !writer.ToJson(afterWriteFailure) || afterWriteFailure != beforeWriteFailure)
        {
            return Fail("An explicit object array codec failure fell through to automatic serialization.");
        }

        std::cout << "Generated object array binding checks passed.\n";
        std::cout << "Generated object array archive checks passed.\n";
        std::cout << "Generated object array archive failure checks passed.\n";
        return 0;
    }
}
