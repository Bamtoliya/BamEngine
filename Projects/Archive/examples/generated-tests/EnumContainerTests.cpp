#include "TestSupport.h"
#include "../GeneratedArchiveTypes.h"

#include <archive/BeveArchive.h>
#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace archive_generated_tests
{
    namespace
    {
        using Profile = generated_archive::EnumContainerProfile;
        using Mode = generated_archive::SignedMode;
        using State = generated_archive::State;
        using Flags = generated_archive::WideFlags;

        bool SameProfile(const Profile& left, const Profile& right)
        {
            if (left.Modes != right.Modes || left.Fixed != right.Fixed || left.Ordered != right.Ordered ||
                left.Bits != right.Bits || left.Binary != right.Binary || left.Pairs != right.Pairs ||
                left.Reverse != right.Reverse || left.WidePairs != right.WidePairs || left.ByName != right.ByName ||
                left.Labels != right.Labels || left.Objects.size() != right.Objects.size())
            {
                return false;
            }

            for (const auto& [key, value] : left.Objects)
            {
                const auto found = right.Objects.find(key);

                if (found == right.Objects.end() || value.Title != found->second.Title ||
                    value.Speed != found->second.Speed || value.Retries != found->second.Retries ||
                    value.CachedSpeed != found->second.CachedSpeed)
                {
                    return false;
                }
            }

            return true;
        }

        std::string MakeDocument(std::string_view field, std::string_view value)
        {
            std::string result = R"({"Enums":{)";
            const std::string_view names[]{ "Modes", "Fixed", "Empty", "Ordered", "Bits", "Binary", "Pairs",
                "Reverse", "WidePairs", "ByName", "Labels", "Objects" };

            for (const auto name : names)
            {
                if (name != "Modes")
                {
                    result += ',';
                }

                result += '"';
                result += name;
                result += "\":";
                result += name == field ? value : name == "Fixed" ? "[0,1]" : "[]";
            }

            return result + "}}";
        }
    }

    int CheckEnumContainers(const reflection::Registry& registry)
    {
        using Code = archive::ArchiveErrorCode;
        const auto* type = registry.FindType<Profile>();

        if (type == nullptr)
        {
            return Fail("Generated enum container profile lookup failed.");
        }

        Profile source;
        source.Modes.push_back(static_cast<Mode>(-1));
        source.Ordered.insert(static_cast<State>(7));
        source.Pairs.emplace(static_cast<Mode>(5), static_cast<State>(7));
        source.Objects.at(State::Active).Retries = 77;

        const auto roundTrip = [&]<typename Archive>(const Profile& expected)
        {
            Archive writer;
            Archive reader;
            std::string document;
            Profile actual = source;

            return archive_reflection::WriteObject(writer, "Enums", *type,
                reflection::ObjectView::From(expected), &registry) && writer.Encode(document) &&
                reader.Decode(document) && archive_reflection::ReadObject(reader, "Enums", *type, actual, &registry) &&
                SameProfile(expected, actual);
        };

        Profile empty;
        empty.Modes.clear();
        empty.Ordered.clear();
        empty.Bits.clear();
        empty.Binary.clear();
        empty.Pairs.clear();
        empty.Reverse.clear();
        empty.WidePairs.clear();
        empty.ByName.clear();
        empty.Labels.clear();
        empty.Objects.clear();

        if (!roundTrip.operator()<archive::JsonArchive>(source) ||
            !roundTrip.operator()<archive::BeveArchive>(source) ||
            !roundTrip.operator()<archive::JsonArchive>(empty) || !roundTrip.operator()<archive::BeveArchive>(empty))
        {
            return Fail("Automatic enum container JSON/BEVE or empty round-trip failed.");
        }

        struct InvalidInput
        {
            std::string_view Field;
            std::string_view Value;
            Code ExpectedCode;
            std::string_view ExpectedField;
        };

        const InvalidInput invalidInputs[]{
            { "Modes", R"([0,"bad"])", Code::InvalidValue, "Modes" },
            { "Modes", "[0,0.5]", Code::InvalidValue, "Modes" },
            { "Modes", "[-129]", Code::InvalidValue, "Modes[0]" },
            { "Modes", "[0,128]", Code::InvalidValue, "Modes[1]" },
            { "Fixed", "[0,256]", Code::InvalidValue, "Fixed[1]" },
            { "Fixed", "[0]", Code::InvalidValue, "Fixed" },
            { "Fixed", "[0,1,2]", Code::InvalidValue, "Fixed" },
            { "Empty", "[0]", Code::InvalidValue, "Empty" },
            { "Ordered", "[1,1]", Code::InvalidValue, "Ordered[1]" },
            { "Bits", "[-1]", Code::InvalidValue, "Bits" },
            { "Bits", "[18446744073709551616]", Code::InvalidValue, "Bits" },
            { "Binary", "[2]", Code::InvalidValue, "Binary[0]" },
            { "Pairs", R"([{"Key":128,"Value":0}])", Code::InvalidValue, "Pairs[0].Key" },
            { "Pairs", R"([{"Key":0,"Value":256}])", Code::InvalidValue, "Pairs[0].Value" },
            { "Pairs", R"([{"Key":0}])", Code::MissingField, "Pairs[0].Value" },
            { "Reverse", R"([{"Key":-1,"Value":0}])", Code::InvalidValue, "Reverse[0].Key" },
            { "Reverse", R"([{"Key":1,"Value":-129}])", Code::InvalidValue, "Reverse[0].Value" },
            { "WidePairs", R"([{"Key":18446744073709551616,"Value":0}])",
                Code::InvalidValue, "WidePairs[0].Key" },
            { "WidePairs", R"([{"Key":18446744073709551615,"Value":0},)"
                R"({"Key":18446744073709551615,"Value":1}])", Code::InvalidValue, "WidePairs[1].Key" },
            { "ByName", R"([{"Key":"Name","Value":256}])", Code::InvalidValue, "ByName[0].Value" },
            { "Labels", R"([{"Key":1,"Value":"A"},{"Key":1,"Value":"B"}])",
                Code::InvalidValue, "Labels[1].Key" },
            { "Objects", R"([{"Key":256,"Value":{}}])", Code::InvalidValue, "Objects[0].Key" },
            { "Objects", R"([{"Key":1,"Value":{"Title":"A","Speed":"bad"}}])",
                Code::InvalidValue, "Objects[0].Value.Speed" }
        };

        archive::JsonArchive reader;

        for (const auto& input : invalidInputs)
        {
            Profile destination = source;

            if (!reader.Parse(MakeDocument(input.Field, input.Value)))
            {
                return Fail("Cannot prepare invalid enum container input.");
            }

            const auto result = archive_reflection::ReadObject(reader, "Enums", *type, destination, &registry);

            if (result.Code != input.ExpectedCode || result.Field != input.ExpectedField ||
                !SameProfile(source, destination))
            {
                std::cerr << "Enum container error: code=" << static_cast<int>(result.Code)
                    << ", field=" << result.Field << '\n';
                return Fail("Enum container range checking, error path or destination preservation failed.");
            }
        }

        archive::JsonArchive writer;
        std::string before;
        std::string after;

        if (!archive_reflection::WriteObject(writer, "Enums", *type, reflection::ObjectView::From(source), &registry) ||
            !writer.ToJson(before))
        {
            return Fail("Cannot prepare an existing enum container document.");
        }

        const reflection::Registry missingRegistry;

        for (const auto& property : type->Properties)
        {
            auto subset = reflection::TypeInfo::For<Profile>("tests::EnumSubset");
            subset.Properties.push_back(property);

            for (const auto* lookup : { static_cast<const reflection::Registry*>(nullptr), &missingRegistry })
            {
                for (const auto* expected : { &source, &empty })
                {
                    const auto result = archive_reflection::WriteObject(
                        writer, "Enums", subset, reflection::ObjectView::From(*expected), lookup);

                    if (result.Code != Code::UnsupportedType || result.Field != property.Name ||
                        !writer.ToJson(after) || before != after)
                    {
                        return Fail("Missing enum registration was accepted or changed the document.");
                    }
                }

                Profile destination = source;

                if (!reader.Parse(MakeDocument("", "")))
                {
                    return Fail("Cannot prepare empty unregistered enum container input.");
                }

                const auto result = archive_reflection::ReadObject(reader, "Enums", subset, destination, lookup);

                if (result.Code != Code::UnsupportedType || result.Field != property.Name ||
                    !SameProfile(source, destination))
                {
                    return Fail("Reading an unregistered enum container changed the destination.");
                }
            }
        }

        const auto* pairs = type->FindProperty("Pairs");

        if (pairs == nullptr)
        {
            return Fail("Generated enum map property lookup failed.");
        }

        for (const auto* enumeration : { registry.FindEnum<Mode>(), registry.FindEnum<State>() })
        {
            reflection::Registry incomplete;

            if (enumeration == nullptr || !incomplete.RegisterEnum(*enumeration))
            {
                return Fail("Cannot prepare a partially registered enum map.");
            }

            auto subset = reflection::TypeInfo::For<Profile>("tests::EnumPairSubset");
            subset.Properties.push_back(*pairs);
            Profile destination = source;
            const auto write = archive_reflection::WriteObject(
                writer, "Enums", subset, reflection::ObjectView::From(empty), &incomplete);
            const auto read = archive_reflection::ReadObject(reader, "Enums", subset, destination, &incomplete);

            if (write.Code != Code::UnsupportedType || write.Field != "Pairs" ||
                read.Code != Code::UnsupportedType || read.Field != "Pairs" || !SameProfile(source, destination) ||
                !writer.ToJson(after) || before != after)
            {
                return Fail("An empty map with only its enum key or value registered was accepted.");
            }
        }

        archive_reflection::PropertyCodecs codecs;
        using Modes = std::vector<Mode>;
        using Pairs = std::map<Mode, State>;

        if (!codecs.RegisterArray<Modes>() || !codecs.RegisterArray<std::array<State, 2>>() ||
            !codecs.RegisterMap<Pairs>())
        {
            return Fail("Cannot register legacy enum container codecs.");
        }

        archive::JsonArchive explicitWriter;
        Profile restored;

        if (!archive_reflection::WriteObject(explicitWriter, "Enums", *type,
            reflection::ObjectView::From(source), &registry, &codecs) || !explicitWriter.ToJson(after) ||
            !reader.Parse(after) || !archive_reflection::ReadObject(reader, "Enums", *type, restored, &registry) ||
            !SameProfile(source, restored) || !reader.Parse(before) ||
            !archive_reflection::ReadObject(reader, "Enums", *type, restored, &registry, &codecs) ||
            !SameProfile(source, restored))
        {
            return Fail("Explicit enum container codec compatibility failed.");
        }

        archive_reflection::PropertyCodecs rejecting;

        if (!rejecting.Register<Modes>(
            [](archive::IArchive&, std::string_view field, const Modes&) -> archive::ArchiveResult
            {
                return { Code::UnsupportedType, std::string{ field } };
            },
            [](const archive::IArchive&, std::string_view field, Modes&) -> archive::ArchiveResult
            {
                return { Code::UnsupportedType, std::string{ field } };
            }))
        {
            return Fail("Cannot register rejecting enum container codec.");
        }

        const auto rejectedWrite = archive_reflection::WriteObject(
            writer, "Enums", *type, reflection::ObjectView::From(source), &registry, &rejecting);
        const auto rejectedRead = archive_reflection::ReadObject(
            reader, "Enums", *type, restored, &registry, &rejecting);

        if (rejectedWrite.Code != Code::UnsupportedType || rejectedWrite.Field != "Modes" ||
            rejectedRead.Code != Code::UnsupportedType || rejectedRead.Field != "Modes" ||
            !SameProfile(source, restored) || !writer.ToJson(after) || before != after)
        {
            return Fail("An explicit enum container codec error fell through to automatic processing.");
        }

        enum Legacy { First = 0 };
        struct LegacyProfile { std::vector<Legacy> Values; };
        reflection::Registry legacyRegistry;
        auto legacy = reflection::EnumInfo::For<Legacy>("tests::Legacy");
        legacy.UnderlyingType = "int";
        auto legacyType = reflection::TypeInfo::For<LegacyProfile>("tests::LegacyProfile");
        legacyType.Properties.push_back(reflection::MakeProperty<&LegacyProfile::Values>("Values", "Values"));
        LegacyProfile legacyObject;

        if (!legacyRegistry.RegisterEnum(std::move(legacy)))
        {
            return Fail("Cannot prepare registered unscoped enum.");
        }

        const auto legacyWrite = archive_reflection::WriteObject(
            writer, "Legacy", legacyType, reflection::ObjectView::From(legacyObject), &legacyRegistry);

        if (legacyWrite.Code != Code::UnsupportedType || legacyWrite.Field != "Values" ||
            !writer.ToJson(after) || before != after)
        {
            return Fail("An unscoped enum container was silently accepted.");
        }

        std::cout << "Automatic enum container JSON/BEVE, range, registration and codec checks passed.\n";
        return 0;
    }
}
