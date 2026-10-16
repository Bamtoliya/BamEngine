#include "ConsumerTypes.h"
#include "ArchiveConsumer.gen.h"

#include <archive/BeveArchive.h>
#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }

    template<typename Archive>
    bool CheckRoundTrip(const reflection::Registry& registry,
        const reflection::TypeInfo& type, const consumer::Settings& source)
    {
        Archive writer;
        Archive reader;
        std::string document;

        consumer::Settings restored;
        restored.SchemaVersion = -1;
        restored.Enabled = false;
        restored.Entries = { { L"Before", 99.0f, 99 } };
        restored.SetRevision(-1);

        const auto result = archive_reflection::WriteObject(
            writer, "Settings", type, reflection::ObjectView::From(source), &registry);

        if (!result)
        {
            std::cerr << "WriteObject failed: code=" << static_cast<int>(result.Code)
                << ", field=" << result.Field << '\n';
            return false;
        }

        return writer.Encode(document) && reader.Decode(document) &&
            archive_reflection::ReadObject(reader, "Settings", type, restored, &registry) &&
            restored == source;
    }
}

int main()
{
    reflection::Registry registry;

    if (!reflection_generated::Register_ArchiveConsumer(registry))
    {
        return Fail("Generated registration failed.");
    }

    const auto* type = registry.FindType<consumer::Settings>();

    if (type == nullptr)
    {
        return Fail("Settings type lookup failed.");
    }

    consumer::Settings source;
    source.Entries = { { L"첫 번째", 10.0f, 5 }, { L"Second", 20.0f, 9 } };
    source.SchemaVersion = 17;
    source.Entries[0].SetId(101);
    source.Entries[1].SetId(103);
    source.Labels = { { -1, L"음수" }, { 7, L"이름 \U0001F600" } };
    source.Counts = { { L"최대", (std::numeric_limits<std::uint64_t>::max)() } };
    source.EntriesById = { { 10, source.Entries[0] }, { 20, source.Entries[1] } };
    source.NamedEntries = { { L"설정 \U0001F600", source.Entries[1] } };
    source.Modes = { consumer::Mode::Backward, consumer::Mode::Forward };
    source.FixedModes = { consumer::Mode::Backward, consumer::Mode::Forward };
    source.ModeSet = { consumer::Mode::Backward, consumer::Mode::Forward };
    source.Flags = { consumer::Permission::High, consumer::Permission::All };
    source.PermissionsByMode = { { consumer::Mode::Backward, consumer::Permission::All } };
    source.ModesByPermission = { { consumer::Permission::All, consumer::Mode::Backward } };
    source.EntriesByMode = { { consumer::Mode::Forward, source.Entries[1] } };
    source.FixedEntries = { source.Entries[1], source.Entries[0] };
    source.Matrix = { { -1, 7 }, {}, { 42 } };
    source.EntryGroups = { { L"그룹 \U0001F600", source.Entries } };
    source.ModeGroups = { std::vector<consumer::Mode>{ consumer::Mode::Backward, consumer::Mode::Forward },
        std::vector<consumer::Mode>{} };
    source.SetRevision(42);

    if (!CheckRoundTrip<archive::JsonArchive>(registry, *type, source))
    {
        return Fail("Installed package JSON round-trip failed.");
    }

    if (!CheckRoundTrip<archive::BeveArchive>(registry, *type, source))
    {
        return Fail("Installed package BEVE round-trip failed.");
    }

    archive::JsonArchive invalidReader;
    const std::string invalidDocument =
        R"({"Settings":{"Enabled":false,"Entries":[)"
        R"({"Name":"First","Speed":2},{"Name":"Second","Speed":"bad"}]}})";

    if (!invalidReader.Parse(invalidDocument))
    {
        return Fail("Invalid-field document parsing failed.");
    }

    consumer::Settings unchanged = source;
    const auto result = archive_reflection::ReadObject(invalidReader, "Settings", *type, unchanged, &registry);

    if (result.Code != archive::ArchiveErrorCode::InvalidValue ||
        result.Field != "Entries[1].Speed" || unchanged != source)
    {
        return Fail("Invalid object-vector field changed the destination.");
    }

    const std::string invalidMapDocument =
        R"({"Settings":{"Enabled":false,"Entries":[],"Labels":[],"Counts":[],"EntriesById":[)"
        R"({"Key":1,"Value":{"Name":"First","Speed":2}},)"
        R"({"Key":2,"Value":{"Name":"Second","Speed":"bad"}}]}})";

    if (!invalidReader.Parse(invalidMapDocument))
    {
        return Fail("Invalid mapped-field document parsing failed.");
    }

    const auto mapFailure = archive_reflection::ReadObject(invalidReader, "Settings", *type, unchanged, &registry);

    if (mapFailure.Code != archive::ArchiveErrorCode::InvalidValue ||
        mapFailure.Field != "EntriesById[1].Value.Speed" || unchanged != source)
    {
        return Fail("Invalid object-map field changed the destination.");
    }

    const std::string invalidEnumDocument =
        R"({"Settings":{"Enabled":false,"Entries":[],"Labels":[],"Counts":[],"EntriesById":[],)"
        R"("NamedEntries":[],"Modes":[128]}})";

    if (!invalidReader.Parse(invalidEnumDocument))
    {
        return Fail("Invalid enum container document parsing failed.");
    }

    const auto enumFailure = archive_reflection::ReadObject(invalidReader, "Settings", *type, unchanged, &registry);

    if (enumFailure.Code != archive::ArchiveErrorCode::InvalidValue ||
        enumFailure.Field != "Modes[0]" || unchanged != source)
    {
        return Fail("An out-of-range enum container value changed the destination.");
    }

    const std::string fixedPrefix =
        R"({"Settings":{"Enabled":false,"Entries":[],"Labels":[],"Counts":[],"EntriesById":[],"NamedEntries":[],)"
        R"("Modes":[],"FixedModes":[0,1],"ModeSet":[],"Flags":[],"PermissionsByMode":[],"ModesByPermission":[],)"
        R"("EntriesByMode":[],"FixedEntries":)";

    struct FixedFailure
    {
        std::string_view Value;
        std::string_view Field;
    };

    const FixedFailure fixedFailures[]{
        { "[]", "FixedEntries" },
        { R"([{"Name":"A","Speed":1},{"Name":"B","Speed":"bad"}])", "FixedEntries[1].Speed" }
    };

    for (const auto& failure : fixedFailures)
    {
        if (!invalidReader.Parse(fixedPrefix + std::string{ failure.Value } + "}}"))
        {
            return Fail("Invalid fixed object array document parsing failed.");
        }

        const auto fixedFailure = archive_reflection::ReadObject(
            invalidReader, "Settings", *type, unchanged, &registry);

        if (fixedFailure.Code != archive::ArchiveErrorCode::InvalidValue ||
            fixedFailure.Field != failure.Field || unchanged != source)
        {
            return Fail("Invalid fixed object array length or field changed the destination.");
        }
    }

    const auto* matrixProperty = type->FindProperty("Matrix");
    auto matrixType = reflection::TypeInfo::For<consumer::Settings>("MatrixOnly");
    if (matrixProperty == nullptr)
    {
        return Fail("Nested matrix property registration is missing.");
    }
    matrixType.Properties.push_back(*matrixProperty);
    if (!invalidReader.Parse(R"({"Settings":{"Matrix":[[1],false]}})"))
    {
        return Fail("Invalid nested matrix input parsing failed.");
    }
    const auto nestedFailure = archive_reflection::ReadObject(
        invalidReader, "Settings", matrixType, unchanged, &registry);
    if (nestedFailure.Code != archive::ArchiveErrorCode::InvalidValue ||
        nestedFailure.Field != "Matrix[1]" || unchanged != source)
    {
        return Fail("Invalid nested matrix changed the destination.");
    }

    source.Matrix.clear();
    source.EntryGroups.clear();
    source.ModeGroups = {};
    source.Entries.clear();
    source.Labels.clear();
    source.Counts.clear();
    source.EntriesById.clear();
    source.NamedEntries.clear();
    source.Modes.clear();
    source.ModeSet.clear();
    source.Flags.clear();
    source.PermissionsByMode.clear();
    source.ModesByPermission.clear();
    source.EntriesByMode.clear();

    if (!CheckRoundTrip<archive::JsonArchive>(registry, *type, source) ||
        !CheckRoundTrip<archive::BeveArchive>(registry, *type, source))
    {
        return Fail("Installed package empty vector/map round-trip failed.");
    }

    std::cout << "Installed ReflectionArchive generated checks passed.\n";
    return 0;
}
