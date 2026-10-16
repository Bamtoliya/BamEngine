#include "TestSupport.h"
#include "../GeneratedArchiveTypes.h"

#include <archive/BeveArchive.h>
#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace archive_generated_tests
{
    namespace
    {
        using Profile = generated_archive::ObjectArrayProfile;
        using Settings = generated_archive::Settings;
        using Values = std::array<Settings, 2>;

        bool SameProfile(const Profile& left, const Profile& right)
        {
            for (std::size_t index = 0; index < left.Values.size(); ++index)
            {
                const auto& a = left.Values[index];
                const auto& b = right.Values[index];

                if (a.Title != b.Title || a.Speed != b.Speed || a.Retries != b.Retries ||
                    a.CachedSpeed != b.CachedSpeed)
                {
                    return false;
                }
            }

            return true;
        }
    }

    int CheckFixedObjectArrays(const reflection::Registry& registry)
    {
        using Code = archive::ArchiveErrorCode;
        const auto* type = registry.FindType<Profile>();

        if (type == nullptr)
        {
            return Fail("Generated fixed object array profile lookup failed.");
        }

        Profile source;
        source.Values[0].Title = L"첫 단계 \U0001F600";
        source.Values[0].Speed = 7.0f;
        source.Values[0].Retries = 9;
        source.Values[1].Title = L"Second";
        source.Values[1].Speed = 11.0f;
        source.Values[1].Retries = 13;
        const auto roundTrip = [&]<typename Archive>()
        {
            Archive writer;
            Archive reader;
            std::string document;
            Profile actual;
            actual.Values[0].CachedSpeed = 99.0f;
            actual.Values[1].Retries = 77;

            return archive_reflection::WriteObject(writer, "Arrays", *type,
                reflection::ObjectView::From(source), &registry) && writer.Encode(document) &&
                reader.Decode(document) && archive_reflection::ReadObject(reader, "Arrays", *type, actual, &registry) &&
                SameProfile(source, actual);
        };

        if (!roundTrip.operator()<archive::JsonArchive>() || !roundTrip.operator()<archive::BeveArchive>())
        {
            return Fail("Fixed object array JSON/BEVE or zero-length array round-trip failed.");
        }

        struct InvalidInput
        {
            std::string_view Document;
            Code ExpectedCode;
            std::string_view ExpectedField;
        };

        const InvalidInput invalidInputs[]{
            { R"({"Arrays":{"Empty":[]}})", Code::MissingField, "Values" },
            { R"({"Arrays":{"Values":{}}})", Code::InvalidValue, "Values" },
            { R"({"Arrays":{"Values":[]}})", Code::InvalidValue, "Values" },
            { R"({"Arrays":{"Values":[{}]}})", Code::InvalidValue, "Values" },
            { R"({"Arrays":{"Values":[{},{},{}]}})", Code::InvalidValue, "Values" },
            { R"({"Arrays":{"Values":[{"Title":"A","Speed":1},7]}})", Code::InvalidValue, "Values[1]" },
            { R"({"Arrays":{"Values":[{"Title":"A","Speed":1},{"Title":"B","Speed":"bad"}]}})",
                Code::InvalidValue, "Values[1].Speed" },
            { R"({"Arrays":{"Values":[{"Title":"A","Speed":1},{"Speed":2}]}})",
                Code::MissingField, "Values[1].Title" },
            { R"({"Arrays":{"Values":[{"Title":"A","Speed":1},{"Title":"B","Speed":2}],"Empty":[{}]}})",
                Code::InvalidValue, "Empty" }
        };

        archive::JsonArchive reader;

        for (const auto& input : invalidInputs)
        {
            Profile actual = source;

            if (!reader.Parse(input.Document))
            {
                return Fail("Cannot prepare invalid fixed object array input.");
            }

            const auto result = archive_reflection::ReadObject(reader, "Arrays", *type, actual, &registry);

            if (result.Code != input.ExpectedCode || result.Field != input.ExpectedField ||
                !SameProfile(source, actual))
            {
                std::cerr << "Fixed object array error: code=" << static_cast<int>(result.Code)
                    << ", field=" << result.Field << '\n';
                return Fail("Fixed object array failure path or destination preservation failed.");
            }
        }

        Profile actual = source;
        actual.Values[0].CachedSpeed = 99.0f;
        const std::string legacyDocument =
            R"({"Arrays":{"Values":[{"Title":"Legacy A","Speed":3},{"Title":"Legacy B","Speed":5}],"Empty":[]}})";

        if (!reader.Parse(legacyDocument) ||
            !archive_reflection::ReadObject(reader, "Arrays", *type, actual, &registry) ||
            actual.Values[0].Title != L"Legacy A" || actual.Values[1].Speed != 5.0f ||
            actual.Values[0].Retries != 3 || actual.Values[1].Retries != 3 ||
            actual.Values[0].CachedSpeed != std::numeric_limits<float>::infinity())
        {
            return Fail("Fixed object array replacement retained old optional or excluded values.");
        }

        archive::JsonArchive writer;
        std::string before;
        std::string after;

        if (!archive_reflection::WriteObject(
            writer, "Arrays", *type, reflection::ObjectView::From(source), &registry) ||
            !writer.ToJson(before))
        {
            return Fail("Cannot prepare an existing fixed object array document.");
        }

        Profile invalid = source;
        invalid.Values[1].Speed = std::numeric_limits<float>::infinity();
        const auto writeFailure = archive_reflection::WriteObject(
            writer, "Arrays", *type, reflection::ObjectView::From(invalid), &registry);

        if (writeFailure.Code != Code::InvalidValue || writeFailure.Field != "Values[1].Speed" ||
            !writer.ToJson(after) || before != after)
        {
            return Fail("Failed fixed object array writing changed the document.");
        }

        const reflection::Registry missingRegistry;

        for (const auto& property : type->Properties)
        {
            auto subset = reflection::TypeInfo::For<Profile>("tests::FixedObjectSubset");
            subset.Properties.push_back(property);

            for (const auto* lookup : { static_cast<const reflection::Registry*>(nullptr), &missingRegistry })
            {
                const auto write = archive_reflection::WriteObject(
                    writer, "Arrays", subset, reflection::ObjectView::From(source), lookup);
                actual = source;
                const auto read = archive_reflection::ReadObject(reader, "Arrays", subset, actual, lookup);

                if (write.Code != Code::UnsupportedType || write.Field != property.Name ||
                    read.Code != Code::UnsupportedType || read.Field != property.Name || !SameProfile(source, actual) ||
                    !writer.ToJson(after) || before != after)
                {
                    return Fail("An unregistered fixed object array was accepted or changed original state.");
                }
            }
        }

        archive_reflection::PropertyCodecs rejecting;

        if (!rejecting.Register<Values>(
            [](archive::IArchive&, std::string_view field, const Values&) -> archive::ArchiveResult
            {
                return { Code::UnsupportedType, std::string{ field } };
            },
            [](const archive::IArchive&, std::string_view field, Values&) -> archive::ArchiveResult
            {
                return { Code::UnsupportedType, std::string{ field } };
            }))
        {
            return Fail("Cannot register rejecting fixed object array codec.");
        }

        actual = source;
        const auto rejectedWrite = archive_reflection::WriteObject(
            writer, "Arrays", *type, reflection::ObjectView::From(source), &registry, &rejecting);
        const auto rejectedRead = archive_reflection::ReadObject(
            reader, "Arrays", *type, actual, &registry, &rejecting);

        if (rejectedWrite.Code != Code::UnsupportedType || rejectedWrite.Field != "Values" ||
            rejectedRead.Code != Code::UnsupportedType || rejectedRead.Field != "Values" ||
            !SameProfile(source, actual) || !writer.ToJson(after) || before != after)
        {
            return Fail("An explicit fixed object array codec error fell through to automatic processing.");
        }

        struct NoDefault
        {
            explicit NoDefault(std::int32_t value) : Value(value) {}
            std::int32_t Value;
        };
        struct ConstructedArrays
        {
            std::array<NoDefault, 1> Values{ NoDefault{ 17 } };
            std::array<NoDefault, 0> Empty{};
        };

        reflection::Registry constructedRegistry;
        auto valueType = reflection::TypeInfo::For<NoDefault>("tests::NoDefault");
        valueType.Properties.push_back(reflection::MakeProperty<&NoDefault::Value>("Value", "int32_t"));

        if (!constructedRegistry.Register(std::move(valueType)))
        {
            return Fail("Cannot prepare a non-default-constructible array element.");
        }

        auto constructedType = reflection::TypeInfo::For<ConstructedArrays>("tests::ConstructedArrays");
        constructedType.Properties.push_back(reflection::MakeProperty<&ConstructedArrays::Values>("Values", "Values"));
        ConstructedArrays constructed;
        archive::JsonArchive constructedReader;

        if (!constructedReader.Parse(R"({"Arrays":{"Values":[{"Value":3}],"Empty":[]}})"))
        {
            return Fail("Cannot prepare constructed fixed object array input.");
        }

        const auto unsupported = archive_reflection::ReadObject(
            constructedReader, "Arrays", constructedType, constructed, &constructedRegistry);

        if (unsupported.Code != Code::InvalidOperation || unsupported.Field != "Values" ||
            constructed.Values[0].Value != 17)
        {
            return Fail("A non-default-constructible nonempty array was accepted or changed the destination.");
        }

        constructedType.Properties.clear();
        constructedType.Properties.push_back(reflection::MakeProperty<&ConstructedArrays::Empty>("Empty", "Empty"));

        if (!archive_reflection::WriteObject(writer, "Constructed", constructedType,
            reflection::ObjectView::From(constructed), &constructedRegistry) ||
            !archive_reflection::ReadObject(
                constructedReader, "Arrays", constructedType, constructed, &constructedRegistry))
        {
            return Fail("A zero-length array incorrectly required constructing an element.");
        }

        std::cout << "Fixed object array JSON/BEVE, defaults, length and failure checks passed.\n";
        return 0;
    }
}
