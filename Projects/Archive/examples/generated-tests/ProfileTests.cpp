#include "TestSupport.h"
#include "../GeneratedArchiveTypes.h"
#include "ArchiveDemo.gen.h"

#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_set>

namespace archive_generated_tests
{
    namespace
    {
        bool SameProfile(const generated_archive::Profile& lhs, const generated_archive::Profile& rhs)
        {
            return lhs.Name == rhs.Name && lhs.Options.Title == rhs.Options.Title &&
                lhs.Options.Speed == rhs.Options.Speed && lhs.Options.CachedSpeed == rhs.Options.CachedSpeed &&
                lhs.Options.Retries == rhs.Options.Retries && lhs.Tags == rhs.Tags &&
                lhs.Current == rhs.Current && lhs.GetScore() == rhs.GetScore();
        }
    }

    int CheckGeneratedProfile(const reflection::Registry& registry, const std::filesystem::path& directory)
    {
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

        std::cout << document << '\n';
        std::cout << "Generated reflection archive checks passed.\n";
        std::cout << "Generated reflection archive policy checks passed.\n";
        std::cout << "Generated reflection archive failure checks passed.\n";
        return 0;
    }
}
