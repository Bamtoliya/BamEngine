#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include <reflection_archive/PropertyArchiveCodecs.h>

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
    struct Settings
    {
        bool Enabled = true;
        std::wstring Name = L"Before";
        std::uint64_t ID = 0;
        std::vector<std::int32_t> Values{ 9 };

        bool operator==(const Settings&) const = default;
    };

    reflection::TypeInfo MakeSettingsType()
    {
        auto type = reflection::TypeInfo::For<Settings>("consumer::Settings");
        type.Properties.push_back(reflection::MakeProperty<&Settings::Enabled>("Enabled", "bool"));
        type.Properties.push_back(reflection::MakeProperty<&Settings::Name>("Name", "std::wstring"));
        type.Properties.push_back(reflection::MakeProperty<&Settings::ID>("ID", "std::uint64_t"));
        type.Properties.push_back(reflection::MakeProperty<&Settings::Values>("Values", "std::vector<std::int32_t>"));
        return type;
    }

    int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }
}

int main()
{
    reflection::Registry registry;

    if (!registry.Register(MakeSettingsType()))
    {
        return Fail("Type registration failed.");
    }

    const auto* type = registry.FindType<Settings>();

    if (type == nullptr)
    {
        return Fail("Type lookup failed.");
    }

    archive_reflection::PropertyCodecs codecs;

    if (!codecs.RegisterArray<std::vector<std::int32_t>>())
    {
        return Fail("Container codec registration failed.");
    }

    const Settings source{ true, L"Player \U0001F600", std::numeric_limits<std::uint64_t>::max(), { 1, -2, 300 } };
    archive::JsonArchive writer;
    std::string document;

    if (!archive_reflection::WriteObject(writer, "Settings", *type, reflection::ObjectView::From(source),
        &registry, &codecs) || !writer.ToJson(document))
    {
        return Fail("Writing failed.");
    }

    archive::JsonArchive reader;
    Settings restored;

    if (!reader.Parse(document) ||
        !archive_reflection::ReadObject(reader, "Settings", *type, restored, &registry, &codecs) ||
        restored != source)
    {
        return Fail("Round-trip failed.");
    }

    const Settings before = restored;
    archive::JsonArchive invalidReader;

    if (!invalidReader.Parse(R"({"Settings":{"Enabled":false,"Name":"Changed","ID":7,"Values":[1,"bad"]}})"))
    {
        return Fail("Invalid-field document parsing failed.");
    }

    auto result = archive_reflection::ReadObject(invalidReader, "Settings", *type, restored, &registry, &codecs);

    if (result.Code != archive::ArchiveErrorCode::InvalidValue || result.Field != "Values" || restored != before)
    {
        return Fail("Invalid-field rollback failed.");
    }

    archive::JsonArchive missingReader;

    if (!missingReader.Parse(R"({"Settings":{"Enabled":false,"Name":"Changed","ID":7}})"))
    {
        return Fail("Missing-field document parsing failed.");
    }

    result = archive_reflection::ReadObject(missingReader, "Settings", *type, restored, &registry, &codecs);

    if (result.Code != archive::ArchiveErrorCode::MissingField || result.Field != "Values" || restored != before)
    {
        return Fail("Missing-field rollback failed.");
    }

    std::cout << "Installed ReflectionArchive package checks passed.\n";
    return 0;
}