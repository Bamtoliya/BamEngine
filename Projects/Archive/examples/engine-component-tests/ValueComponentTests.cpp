#include "TestSupport.h"
#include "EngineComponents.gen.h"

#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include "Components/TagComponent.h"
#include "Components/IDComponent.h"
#include "Components/FlagComponent.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_set>
#include <variant>

namespace archive_engine_component_tests
{
    int CheckEngineValueComponents(const reflection::Registry& registry, const std::filesystem::path& directory)
    {
        const auto* tagType = registry.FindType<Engine::TagComponent>();
        const auto* idType = registry.FindType<Engine::IDComponent>();
        const auto* flagType = registry.FindType<Engine::FlagComponent>();
        const auto* enumeration = registry.FindEnum<Engine::EEntityFlag>();

        if (tagType == nullptr || idType == nullptr || flagType == nullptr || enumeration == nullptr)
        {
            return Fail("Engine value component lookup failed.");
        }

        const auto* idProperty = idType->FindProperty("id");
        const auto* readOnlyMetadata = idProperty ? idProperty->Metadata.Find("ReadOnly") : nullptr;
        const auto* readOnly = readOnlyMetadata ? std::get_if<bool>(readOnlyMetadata) : nullptr;
        const auto* bitmaskMetadata = enumeration->Metadata.Find("Bitmask");
        const auto* bitmask = bitmaskMetadata ? std::get_if<bool>(bitmaskMetadata) : nullptr;

        if (readOnly == nullptr || !*readOnly || bitmask == nullptr || !*bitmask)
        {
            return Fail("Engine component metadata checks failed.");
        }

        Engine::TagComponent sourceTags;
        sourceTags.tags = { L"플레이어", L"테스트 \U0001F600" };

        Engine::IDComponent sourceID;
        sourceID.id = 123;

        Engine::FlagComponent sourceFlags;
        sourceFlags.flags = Engine::EEntityFlag::Active | Engine::EEntityFlag::Paused;

        if (!Engine::HasFlag(sourceFlags.flags, Engine::EEntityFlag::Active) ||
            !Engine::HasFlag(sourceFlags.flags, Engine::EEntityFlag::Paused) ||
            Engine::HasFlag(sourceFlags.flags, Engine::EEntityFlag::Visible))
        {
            return Fail("Engine bit flag operators failed.");
        }

        archive_reflection::PropertyCodecs codecs;

        if (!codecs.RegisterArray<std::unordered_set<std::wstring>>())
        {
            return Fail("Engine tag codec registration failed.");
        }

        archive::JsonArchive writer;

        if (!archive_reflection::WriteObject(
            writer, "Tags", *tagType, reflection::ObjectView::From(sourceTags), &registry, &codecs) ||
            !archive_reflection::WriteObject(
                writer, "ID", *idType, reflection::ObjectView::From(sourceID), &registry) ||
            !archive_reflection::WriteObject(
                writer, "Flags", *flagType, reflection::ObjectView::From(sourceFlags), &registry))
        {
            return Fail("Engine value component writing failed.");
        }

        const auto path = directory / "engine-value-components.json";
        archive::JsonArchive reader;
        Engine::TagComponent restoredTags;
        Engine::IDComponent restoredID;
        Engine::FlagComponent restoredFlags;
        restoredFlags.flags = Engine::EEntityFlag::None;

        if (!writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Tags", *tagType, restoredTags, &registry, &codecs) ||
            !archive_reflection::ReadObject(reader, "ID", *idType, restoredID, &registry) ||
            !archive_reflection::ReadObject(reader, "Flags", *flagType, restoredFlags, &registry) ||
            restoredTags.tags != sourceTags.tags || restoredID.id != sourceID.id ||
            restoredFlags.flags != sourceFlags.flags)
        {
            return Fail("Engine value component round-trip failed.");
        }

        archive::JsonArchive failureReader;

        if (!failureReader.Parse(R"({"Tags":{"tags":["Changed",42]},"Flags":{"flags":300}})"))
        {
            return Fail("Cannot prepare engine value component reading failure.");
        }

        const auto beforeTags = restoredTags.tags;
        const auto beforeFlags = restoredFlags.flags;
        const auto tagFailure = archive_reflection::ReadObject(
            failureReader, "Tags", *tagType, restoredTags, &registry, &codecs);
        const auto flagFailure = archive_reflection::ReadObject(
            failureReader, "Flags", *flagType, restoredFlags, &registry);

        if (tagFailure.Code != archive::ArchiveErrorCode::InvalidValue || tagFailure.Field != "tags" ||
            restoredTags.tags != beforeTags ||
            flagFailure.Code != archive::ArchiveErrorCode::InvalidValue || flagFailure.Field != "flags" ||
            restoredFlags.flags != beforeFlags)
        {
            return Fail("Failed engine value component reading changed the destination.");
        }

        std::cout << "Engine value component generation checks passed.\n";
        std::cout << "Engine value component archive checks passed.\n";
        std::cout << "Engine value component failure checks passed.\n";
        return 0;
    }
}
