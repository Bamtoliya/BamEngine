#include "TestSupport.h"
#include "../GeneratedArchiveTypes.h"

#include <archive/BeveArchive.h>
#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <cstdint>
#include <functional>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace archive_generated_tests
{
    int CheckNestedContainers(const reflection::Registry& registry)
    {
        using Profile = generated_archive::NestedContainerProfile;
        using Code = archive::ArchiveErrorCode;
        const auto* type = registry.FindType<Profile>();
        if (type == nullptr)
        {
            return Fail("Nested container profile registration is missing.");
        }
        Profile source;
        source.Objects.at("Steps")[0].Title = L"중첩 설정 \U0001F600";
        source.Objects.at("Steps")[0].Speed = 17.0f;
        const auto roundTrip = [&]<typename Archive>(const Profile& expected)
        {
            Archive writer;
            Archive reader;
            std::string document;
            Profile actual;
            actual.Matrix = { { 99 } };
            actual.Objects.at("Steps")[0].CachedSpeed = 42.0f;
            return archive_reflection::WriteObject(writer, "Nested", *type,
                reflection::ObjectView::From(expected), &registry) && writer.Encode(document) &&
                reader.Decode(document) && archive_reflection::ReadObject(reader, "Nested", *type, actual, &registry) &&
                actual == expected;
        };
        Profile empty;
        empty.Matrix.clear();
        empty.FixedRows = {};
        empty.Objects.clear();
        empty.EnumGroups.clear();
        empty.MapGroups.clear();
        empty.Tags.clear();
        empty.Masks.clear();
        empty.Deep.clear();
        for (const auto* expected : { &source, &empty })
        {
            if (!roundTrip.operator()<archive::JsonArchive>(*expected) ||
                !roundTrip.operator()<archive::BeveArchive>(*expected))
            {
                return Fail("Nested container JSON/BEVE round-trip failed.");
            }
        }

        const auto singleType = [&](std::string_view name)
        {
            auto result = reflection::TypeInfo::For<Profile>("Nested");
            result.Properties.push_back(*type->FindProperty(name));
            return result;
        };
        const auto document = [](std::string_view field, std::string_view value)
        {
            return "{\"Nested\":{\"" + std::string{ field } + "\":" + std::string{ value } + "}}";
        };
        struct InvalidInput
        {
            std::string_view Field;
            std::string_view Value;
            Code ExpectedCode;
            std::string_view ExpectedPath;
        };
        const InvalidInput invalidInputs[]{
            { "Matrix", "{}", Code::InvalidValue, "Matrix" },
            { "Matrix", "[1]", Code::InvalidValue, "Matrix[0]" },
            { "Matrix", "[[1],[true]]", Code::InvalidValue, "Matrix[1]" },
            { "FixedRows", "[[1]]", Code::InvalidValue, "FixedRows" },
            { "FixedRows", "[[1],7]", Code::InvalidValue, "FixedRows[1]" },
            { "EnumGroups", "[[0,256]]", Code::InvalidValue, "EnumGroups[0][1]" },
            { "Objects", R"([{"Key":"Steps","Value":[{"Title":"A","Speed":1},7]}])",
                Code::InvalidValue, "Objects[0].Value[1]" },
            { "Objects", R"([{"Key":"Steps","Value":[{"Title":"A","Speed":1},{"Title":"B","Speed":"bad"}]}])",
                Code::InvalidValue, "Objects[0].Value[1].Speed" },
            { "Objects", R"([{"Key":"Steps","Value":[{"Speed":1}]}])",
                Code::MissingField, "Objects[0].Value[0].Title" },
            { "Objects", R"([{"Key":"Steps","Value":[]},{"Key":"Steps","Value":[]}])",
                Code::InvalidValue, "Objects[1].Key" },
            { "MapGroups", R"([[{"Key":1,"Value":[]},{"Key":1,"Value":[]}]])",
                Code::InvalidValue, "MapGroups[0][1].Key" },
            { "Masks", R"([["bad"]])", Code::InvalidValue, "Masks[0]" },
            { "Deep", "[[[1],false]]", Code::InvalidValue, "Deep[0][1]" },
            { "Empty", "[[]]", Code::InvalidValue, "Empty" }
        };
        archive::JsonArchive reader;
        Profile rawMatrix = source;
        if (!reader.Parse(document("Matrix", "[[7,8],[],[9]]")) ||
            !archive_reflection::ReadObject(reader, "Nested", singleType("Matrix"), rawMatrix) ||
            rawMatrix.Matrix != std::vector<std::vector<std::int32_t>>{ { 7, 8 }, {}, { 9 } })
        {
            return Fail("Nested scalar arrays require unexpected object wrappers.");
        }
        for (const auto& input : invalidInputs)
        {
            Profile actual = source;
            if (!reader.Parse(document(input.Field, input.Value)))
            {
                return Fail("Cannot prepare invalid nested container input.");
            }
            const auto result = archive_reflection::ReadObject(
                reader, "Nested", singleType(input.Field), actual, &registry);
            if (result.Code != input.ExpectedCode || result.Field != input.ExpectedPath || actual != source)
            {
                std::cerr << "Nested error: " << input.Field << ", code=" << static_cast<int>(result.Code)
                    << ", path=" << result.Field << '\n';
                return Fail("Nested container error path or destination preservation failed.");
            }
        }

        Profile restored = source;
        restored.Objects.at("Steps")[0].Retries = 99;
        restored.Objects.at("Steps")[0].CachedSpeed = 42.0f;
        if (!reader.Parse(document("Objects", R"([{"Key":"Steps","Value":[{"Title":"Fresh","Speed":3}]}])")) ||
            !archive_reflection::ReadObject(reader, "Nested", singleType("Objects"), restored, &registry) ||
            restored.Objects.at("Steps")[0].Retries != 3 ||
            restored.Objects.at("Steps")[0].CachedSpeed != std::numeric_limits<float>::infinity())
        {
            return Fail("Nested object containers did not use fresh element defaults.");
        }

        archive::JsonArchive writer;
        std::string before;
        std::string after;
        if (!archive_reflection::WriteObject(
            writer, "Nested", *type, reflection::ObjectView::From(source), &registry) ||
            !writer.ToJson(before))
        {
            return Fail("Cannot prepare nested container document.");
        }
        Profile invalid = source;
        invalid.MapGroups[0].begin()->second[0].Speed = std::numeric_limits<float>::infinity();
        const auto invalidWrite = archive_reflection::WriteObject(
            writer, "Nested", *type, reflection::ObjectView::From(invalid), &registry);
        if (invalidWrite.Code != Code::InvalidValue || invalidWrite.Field != "MapGroups[0][0].Value[0].Speed" ||
            !writer.ToJson(after) || after != before)
        {
            return Fail("Invalid nested object value changed the existing document.");
        }
        const reflection::Registry missingRegistry;
        for (const auto* lookup : { static_cast<const reflection::Registry*>(nullptr), &missingRegistry })
        {
            for (const auto field : { "Empty", "Objects", "EnumGroups", "MapGroups" })
            {
                const auto partial = singleType(field);
                Profile actual = source;
                const auto write = archive_reflection::WriteObject(
                    writer, "Nested", partial, reflection::ObjectView::From(empty), lookup);
                if (!reader.Parse(document(field, "[]")))
                {
                    return Fail("Cannot prepare empty unregistered nested container.");
                }
                const auto read = archive_reflection::ReadObject(reader, "Nested", partial, actual, lookup);
                if (write.Code != Code::UnsupportedType || write.Field != field ||
                    read.Code != Code::UnsupportedType || read.Field != field || actual != source ||
                    !writer.ToJson(after) || after != before)
                {
                    return Fail("Empty nested containers bypassed required type registration.");
                }
            }
        }

        archive_reflection::PropertyCodecs codecs;
        using Row = std::vector<std::int32_t>;
        if (!codecs.Register<Row>(
            [](archive::IArchive&, std::string_view key, const Row&) -> archive::ArchiveResult
            {
                return { Code::InvalidValue, std::string{ key } };
            },
            [](const archive::IArchive&, std::string_view key, Row&) -> archive::ArchiveResult
            {
                return { Code::InvalidValue, std::string{ key } };
            }) || !reader.Parse(document("Matrix", "[[1]]")))
        {
            return Fail("Cannot prepare nested container codec rejection.");
        }
        restored = source;
        const auto rejectedWrite = archive_reflection::WriteObject(
            writer, "Nested", singleType("Matrix"), reflection::ObjectView::From(source), &registry, &codecs);
        const auto rejectedRead = archive_reflection::ReadObject(
            reader, "Nested", singleType("Matrix"), restored, &registry, &codecs);
        if (rejectedWrite.Code != Code::InvalidValue || rejectedWrite.Field != "Matrix[0]" ||
            rejectedRead.Code != Code::InvalidValue || rejectedRead.Field != "Matrix[0]" || restored != source ||
            !writer.ToJson(after) || after != before)
        {
            return Fail("Nested container codec errors were bypassed or changed existing data.");
        }

        using Descending = std::set<std::int32_t, std::greater<std::int32_t>>;
        struct CustomRows
        {
            std::vector<Descending> Values{ { 7, 3 } };
            bool operator==(const CustomRows&) const = default;
        };
        auto customType = reflection::TypeInfo::For<CustomRows>("CustomRows");
        customType.Properties.push_back(reflection::MakeProperty<&CustomRows::Values>("Values", "Values"));
        for (const auto& values : { std::vector<Descending>{}, std::vector<Descending>{ { 7, 3 } } })
        {
            const CustomRows expected{ values };
            const auto unsupported = archive_reflection::WriteObject(
                writer, "Nested", customType, reflection::ObjectView::From(expected));
            if (unsupported.Code != Code::UnsupportedType || unsupported.Field != "Values" ||
                !writer.ToJson(after) || after != before)
            {
                return Fail("Empty nested containers bypassed unsupported scalar container policies.");
            }
        }
        archive_reflection::PropertyCodecs customCodecs;
        archive::BeveArchive customWriter;
        archive::BeveArchive customReader;
        CustomRows expected;
        CustomRows actual;
        actual.Values.clear();
        std::string encoded;
        if (!customCodecs.RegisterArray<Descending>() ||
            !archive_reflection::WriteObject(customWriter, "Nested", customType,
                reflection::ObjectView::From(expected), nullptr, &customCodecs) || !customWriter.Encode(encoded) ||
            !customReader.Decode(encoded) ||
            !archive_reflection::ReadObject(customReader, "Nested", customType, actual, nullptr, &customCodecs) ||
            actual != expected)
        {
            return Fail("An explicit nested container codec did not override automatic support checks.");
        }
        std::cout << "Nested container JSON/BEVE, defaults, registration, codec and failure checks passed.\n";
        return 0;
    }
}
