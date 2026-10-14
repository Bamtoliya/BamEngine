#include "TestSupport.h"

#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include <reflection/Registry.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

namespace archive_reflection_tests
{
    namespace
    {
        enum class Mode : std::int8_t
        {
            Disabled = -1,
            Ready = 0,
            Active = 1
        };

        enum class Permission : std::uint64_t
        {
            None = 0,
            Read = 1,
            Write = 2,
            High = std::uint64_t{ 1 } << 63,
            All = std::numeric_limits<std::uint64_t>::max()
        };

        constexpr Permission operator|(Permission lhs, Permission rhs)
        {
            return static_cast<Permission>(static_cast<std::uint64_t>(lhs) | static_cast<std::uint64_t>(rhs));
        }

        struct EnumSettings
        {
            float Speed = 25.0f;
            Mode State = Mode::Disabled;
            Permission Bits = Permission::Read | Permission::Write | Permission::High;
            Permission Mask = Permission::All;
        };

        bool SameEnumSettings(const EnumSettings& lhs, const EnumSettings& rhs)
        {
            return lhs.Speed == rhs.Speed && lhs.State == rhs.State && lhs.Bits == rhs.Bits && lhs.Mask == rhs.Mask;
        }
    }

    int CheckEnumArchive(reflection::Registry& registry, const std::filesystem::path& directory)
    {
        auto modeInfo = reflection::EnumInfo::For<Mode>("example::Mode");
        modeInfo.UnderlyingType = "std::int8_t";
        modeInfo.Entries = {
            { "Disabled", std::int64_t{ -1 } }, { "Ready", std::int64_t{ 0 } }, { "Active", std::int64_t{ 1 } }
        };

        auto permissionInfo = reflection::EnumInfo::For<Permission>("example::Permission");
        permissionInfo.UnderlyingType = "std::uint64_t";
        permissionInfo.Entries = {
            { "None", std::uint64_t{ 0 } }, { "Read", std::uint64_t{ 1 } }, { "Write", std::uint64_t{ 2 } },
            { "High", std::uint64_t{ 1 } << 63 }, { "All", std::numeric_limits<std::uint64_t>::max() }
        };

        if (!registry.RegisterEnum(std::move(modeInfo)) || !registry.RegisterEnum(std::move(permissionInfo)))
        {
            return Fail("Enum registration failed.");
        }

        auto type = reflection::TypeInfo::For<EnumSettings>("example::EnumSettings");
        type.Properties.push_back(reflection::MakeProperty<&EnumSettings::Speed>("Speed", "float"));
        type.Properties.push_back(reflection::MakeProperty<&EnumSettings::State>("State", "Mode"));
        type.Properties.push_back(reflection::MakeProperty<&EnumSettings::Bits>("Bits", "Permission"));
        type.Properties.push_back(reflection::MakeProperty<&EnumSettings::Mask>("Mask", "Permission"));

        const EnumSettings source;
        archive::JsonArchive writer;

        if (!archive_reflection::WriteObject(writer, "Enums", type, reflection::ObjectView::From(source), &registry))
        {
            return Fail("Reflected enum writing failed.");
        }

        const auto path = directory / "reflection-enums.json";

        if (!writer.SaveToFile(path))
        {
            return Fail("Reflected enum file saving failed.");
        }

        archive::JsonArchive reader;
        EnumSettings restored;
        restored.Speed = -123.0f;
        restored.State = Mode::Ready;
        restored.Bits = Permission::None;
        restored.Mask = Permission::None;

        if (!reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Enums", type, restored, &registry) ||
            !SameEnumSettings(restored, source))
        {
            return Fail("Reflected enum round-trip failed.");
        }

        using Code = archive::ArchiveErrorCode;

        struct FailureCase
        {
            const char* Json;
            const char* Field;
            Code Expected;
        };

        const FailureCase failures[] = {
            { R"({"Enums":{"Speed":99,"State":128,"Bits":3,"Mask":7}})", "State", Code::InvalidValue },
            { R"({"Enums":{"Speed":99,"State":-129,"Bits":3,"Mask":7}})", "State", Code::InvalidValue },
            { R"({"Enums":{"Speed":99,"State":1.5,"Bits":3,"Mask":7}})", "State", Code::InvalidValue },
            { R"({"Enums":{"Speed":99,"State":"bad","Bits":3,"Mask":7}})", "State", Code::InvalidValue },
            { R"({"Enums":{"Speed":99,"State":1,"Bits":-1,"Mask":7}})", "Bits", Code::InvalidValue },
            { R"({"Enums":{"Speed":99,"State":1,"Bits":1.5,"Mask":7}})", "Bits", Code::InvalidValue },
            { R"({"Enums":{"Speed":99,"State":1,"Mask":7}})", "Bits", Code::MissingField }
        };

        for (const auto& failure : failures)
        {
            archive::JsonArchive failureReader;

            if (!failureReader.Parse(failure.Json))
            {
                return Fail("Cannot prepare enum reading failure.");
            }

            EnumSettings unchanged = source;
            const auto result = archive_reflection::ReadObject(failureReader, "Enums", type, unchanged, &registry);

            if (result.Code != failure.Expected || result.Field != failure.Field ||
                !SameEnumSettings(unchanged, source))
            {
                return Fail("Failed enum reading changed the destination.");
            }
        }

        std::string beforeFailure;

        if (!writer.ToJson(beforeFailure))
        {
            return Fail("Cannot capture the enum document.");
        }

        EnumSettings changed = source;
        changed.Speed = 99.0f;
        const auto result = archive_reflection::WriteObject(
            writer, "Enums", type, reflection::ObjectView::From(changed));

        std::string afterFailure;

        if (result.Code != Code::UnsupportedType || result.Field != "State" ||
            !writer.ToJson(afterFailure) || afterFailure != beforeFailure)
        {
            return Fail("Missing enum registry check failed.");
        }

        std::cout << beforeFailure << '\n';
        std::cout << "Reflection enum archive checks passed.\n";
        std::cout << "Reflection enum archive failure checks passed.\n";
        return 0;
    }
}