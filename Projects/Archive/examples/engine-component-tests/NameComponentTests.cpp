#include "TestSupport.h"
#include "EngineComponents.gen.h"

#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include "Components/NameComponent.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <variant>

namespace archive_engine_component_tests
{
    int CheckEngineNameComponent(const reflection::Registry& registry, const std::filesystem::path& directory)
    {
        const auto* type = registry.FindType<Engine::NameComponent>();
        const auto* property = type != nullptr ? type->FindProperty("name") : nullptr;

        if (property == nullptr)
        {
            return Fail("Engine name property lookup failed.");
        }

        const auto* metadata = property->Metadata.Find("Editable");
        const auto* editable = metadata != nullptr ? std::get_if<bool>(metadata) : nullptr;

        if (editable == nullptr || !*editable)
        {
            return Fail("Engine name metadata generation failed.");
        }

        Engine::NameComponent source;
        source.name = L"플레이어 \U0001F600";
        archive::JsonArchive writer;

        if (!archive_reflection::WriteObject(
            writer, "NameComponent", *type, reflection::ObjectView::From(source)))
        {
            return Fail("Engine name component writing failed.");
        }

        const auto path = directory / "engine-name-component.json";
        archive::JsonArchive reader;
        Engine::NameComponent restored;
        restored.name = L"Before";

        if (!writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "NameComponent", *type, restored) ||
            restored.name != source.name)
        {
            return Fail("Engine name component round-trip failed.");
        }

        archive::JsonArchive failureReader;

        if (!failureReader.Parse(R"({"NameComponent":{"name":42}})"))
        {
            return Fail("Cannot prepare engine component reading failure.");
        }

        const std::wstring beforeFailure = restored.name;
        const auto result = archive_reflection::ReadObject(failureReader, "NameComponent", *type, restored);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue ||
            result.Field != "name" || restored.name != beforeFailure)
        {
            return Fail("Failed engine component reading changed the destination.");
        }

        std::cout << "Engine name component archive checks passed.\n";
        std::cout << "Engine name component archive failure checks passed.\n";
        return 0;
    }
}
