#include "TestSupport.h"
#include "../GeneratedInheritanceTypes.h"

#include <archive/BeveArchive.h>
#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace archive_generated_tests
{
    namespace
    {
        template<typename Archive, typename T>
        bool RoundTrip(const reflection::Registry& registry, const T& source)
        {
            const auto* type = registry.FindType<T>();
            Archive writer;
            Archive reader;
            T restored;
            std::string document;
            if (type == nullptr)
            {
                return false;
            }
            const auto write = archive_reflection::WriteObject(
                writer, "Object", *type, reflection::ObjectView::From(source), &registry);
            if (!write)
            {
                std::cerr << type->QualifiedName << " write: " << static_cast<int>(write.Code)
                    << ", field=" << write.Field << '\n';
                return false;
            }
            if (!writer.Encode(document) || !reader.Decode(document))
            {
                std::cerr << type->QualifiedName << " encode/decode failed.\n";
                return false;
            }
            const auto read = archive_reflection::ReadObject(reader, "Object", *type, restored, &registry);
            if (!read || restored != source)
            {
                std::cerr << type->QualifiedName << " read: " << static_cast<int>(read.Code)
                    << ", field=" << read.Field << ", equal=" << (restored == source) << '\n';
                if constexpr (std::is_same_v<T, generated_inheritance::Diamond>)
                {
                    std::cerr << "Shared label: " << restored.Label << ", expected=" << source.Label
                        << ", marks=" << restored.Marks.size() << "/" << source.Marks.size()
                        << ", id=" << restored.GetId() << "/" << source.GetId() << '\n';
                }
                return false;
            }
            return true;
        }
    }

    int CheckInheritedObjects(const reflection::Registry& registry)
    {
        using namespace generated_inheritance;
        using Code = archive::ArchiveErrorCode;
        const auto* type = registry.FindType<Derived>();
        const auto* identity = registry.FindType<Identity>();
        const auto* diamondType = registry.FindType<Diamond>();
        const auto* leftType = registry.FindType<Left>();
        if (type == nullptr || identity == nullptr || diamondType == nullptr || leftType == nullptr ||
            type->Bases.size() != 2 || leftType->Bases.size() != 1 || !leftType->Bases[0].IsVirtual)
        {
            return Fail("Generated inheritance relationships are missing.");
        }
        Derived source;
        source.Label = "상속 \xF0\x9F\x98\x80";
        source.SetId(42);
        source.Marks = { -7, 19 };
        source.Retries = 13;
        source.X = 8.0f;
        source.Health = 91;
        MultiLevel multi;
        static_cast<Derived&>(multi) = source;
        multi.Level = 9;
        Diamond diamond;
        static_cast<Identity&>(diamond) = source;
        diamond.LeftValue = 31;
        diamond.RightValue = 37;
        diamond.Enabled = false;
        PrivateDerived hidden;
        hidden.GetIdentity() = source;
        hidden.Secret = 41;
        ProtectedDerived protectedValue;
        protectedValue.GetIdentity() = source;
        protectedValue.Secret = 43;
        Containers containers;
        containers.Items = { source, source };
        containers.Fixed[0] = diamond;
        containers.ByName = { { "Player", multi } };
        const auto check = [&]<typename T>(const T& expected)
        {
            return RoundTrip<archive::JsonArchive>(registry, expected) &&
                RoundTrip<archive::BeveArchive>(registry, expected);
        };
        if (!check(source) || !check(multi) || !check(diamond) || !check(hidden) ||
            !check(protectedValue) || !check(containers))
        {
            return Fail("Inherited JSON/BEVE object or container round-trip failed.");
        }

        archive::JsonArchive reader;
        const std::string valid = R"({"Object":{"Label":"Fresh","Marks":[],"m_id":4,"X":3,"Health":5}})";
        Derived restored = source;
        restored.Retries = 99;
        restored.Cached = 77;
        if (!reader.Parse(valid) || !archive_reflection::ReadObject(reader, "Object", *type, restored, &registry) ||
            restored.Label != "Fresh" || restored.GetId() != 4 || restored.X != 3 || restored.Health != 5 ||
            restored.Retries != 99 || restored.Cached != 77)
        {
            return Fail("Inherited fields were omitted, wrapped or lost their Optional/Transient policy.");
        }
        struct InvalidInput { std::string_view Document; std::string_view Field; Code Error; };
        const InvalidInput invalidInputs[]{
            { R"({"Object":{"Marks":[],"m_id":4,"X":3,"Health":5}})", "Label", Code::MissingField },
            { R"({"Object":{"Label":"A","Marks":[],"m_id":"bad","X":3,"Health":5}})",
                "m_id", Code::InvalidValue },
            { R"({"Object":{"Label":"A","Marks":[],"m_id":4,"X":true,"Health":5}})", "X", Code::InvalidValue },
            { R"({"Object":{"Label":"A","Marks":[],"m_id":4,"X":3,"Health":"bad"}})",
                "Health", Code::InvalidValue }
        };
        for (const auto& input : invalidInputs)
        {
            restored = source;
            if (!reader.Parse(input.Document))
            {
                return Fail("Cannot prepare invalid inherited input.");
            }
            const auto result = archive_reflection::ReadObject(reader, "Object", *type, restored, &registry);
            if (result.Code != input.Error || result.Field != input.Field || restored != source)
            {
                return Fail("Inherited read failure changed the destination or reported the wrong field.");
            }
        }

        auto itemsType = reflection::TypeInfo::For<Containers>("ItemsOnly");
        itemsType.Properties.push_back(*registry.FindType<Containers>()->FindProperty("Items"));
        Containers fresh = containers;
        fresh.Items[0].Retries = 99;
        fresh.Items[0].Cached = 77;
        if (!reader.Parse(R"({"Object":{"Items":[{"Label":"A","Marks":[],"m_id":2,"X":3,"Health":4}]}})") ||
            !archive_reflection::ReadObject(reader, "Object", itemsType, fresh, &registry) ||
            fresh.Items.size() != 1 || fresh.Items[0].Retries != 3 || fresh.Items[0].Cached != 11)
        {
            return Fail("Inherited object array fields did not use fresh element defaults.");
        }

        archive::JsonArchive writer;
        std::string before;
        std::string after;
        if (!archive_reflection::WriteObject(
            writer, "Object", *type, reflection::ObjectView::From(source), &registry) ||
            !writer.ToJson(before))
        {
            return Fail("Cannot prepare inherited document preservation checks.");
        }
        Containers invalid = containers;
        invalid.Items[1].X = std::numeric_limits<float>::infinity();
        const auto invalidWrite = archive_reflection::WriteObject(writer, "Object", *registry.FindType<Containers>(),
            reflection::ObjectView::From(invalid), &registry);
        if (invalidWrite.Code != Code::InvalidValue || invalidWrite.Field != "Items[1].X" ||
            !writer.ToJson(after) || after != before)
        {
            return Fail("Invalid inherited array data changed the existing document.");
        }

        reflection::Registry missing;
        const reflection::Registry* lookups[]{ nullptr, &missing };
        for (const auto* lookup : lookups)
        {
            restored = source;
            if (!reader.Parse(valid))
            {
                return Fail("Cannot prepare missing base registration input.");
            }
            const auto write = archive_reflection::WriteObject(
                writer, "Object", *type, reflection::ObjectView::From(source), lookup);
            const auto read = archive_reflection::ReadObject(reader, "Object", *type, restored, lookup);
            if (write.Code != Code::UnsupportedType || write.Field != "generated_inheritance::Identity" ||
                read.Code != Code::UnsupportedType || read.Field != write.Field || restored != source ||
                !writer.ToJson(after) || after != before)
            {
                return Fail("Missing base registration was silently ignored.");
            }
        }
        if (!missing.Register(*identity))
        {
            return Fail("Cannot register the first base for the partial registry check.");
        }
        const auto missingSecond = archive_reflection::WriteObject(
            writer, "Object", *type, reflection::ObjectView::From(source), &missing);
        MissingBase missingObject;
        const auto missingExternal = archive_reflection::WriteObject(
            writer, "Object", *registry.FindType<MissingBase>(),
            reflection::ObjectView::From(missingObject), &registry);
        if (missingSecond.Code != Code::UnsupportedType ||
            missingSecond.Field != "generated_inheritance::Coordinates" ||
            missingExternal.Code != Code::UnsupportedType ||
            missingExternal.Field != "generated_inheritance::ExternalBase")
        {
            return Fail("An unregistered additional or external base was ignored.");
        }

        int calls = 0;
        archive_reflection::PropertyCodecs collisionCodecs;
        if (!collisionCodecs.Register<std::string>(
            [&](archive::IArchive& ar, std::string_view key, const std::string& value)
            {
                ++calls;
                return ar.Write(key, value);
            },
            [&](const archive::IArchive& ar, std::string_view key, std::string& value)
            {
                ++calls;
                return ar.Read(key, value);
            }))
        {
            return Fail("Cannot register inheritance collision probe codecs.");
        }
        const auto collision = [&]<typename T>()
        {
            const auto* info = registry.FindType<T>();
            const T expected;
            T actual = expected;
            const auto write = archive_reflection::WriteObject(writer, "Object", *info,
                reflection::ObjectView::From(expected), &registry, &collisionCodecs);
            const auto read = archive_reflection::ReadObject(
                reader, "Object", *info, actual, &registry, &collisionCodecs);
            return write.Code == Code::InvalidOperation && write.Field == "Label" &&
                read.Code == Code::InvalidOperation && read.Field == "Label" && actual == expected && calls == 0 &&
                writer.ToJson(after) && after == before;
        };
        if (!collision.operator()<Collision>() || !collision.operator()<Repeated>())
        {
            return Fail("Inherited name collisions or distinct repeated bases were silently overwritten.");
        }
        auto excludedType = *registry.FindType<Collision>();
        if (!excludedType.Properties[0].Metadata.Add("Transient", true))
        {
            return Fail("Cannot exclude the hiding derived property.");
        }
        Collision excluded;
        excluded.Identity::Label = "SavedBase";
        Collision excludedResult;
        excludedResult.Label = "KeepDerived";
        archive::JsonArchive excludedWriter;
        std::string excludedDocument;
        if (!archive_reflection::WriteObject(excludedWriter, "Object", excludedType,
                reflection::ObjectView::From(excluded), &registry) || !excludedWriter.Encode(excludedDocument) ||
            !reader.Decode(excludedDocument) ||
            !archive_reflection::ReadObject(reader, "Object", excludedType, excludedResult, &registry) ||
            excludedResult.Identity::Label != "SavedBase" || excludedResult.Label != "KeepDerived")
        {
            return Fail("Excluded derived properties still collided with inherited serialized fields.");
        }

        int writes = 0;
        int reads = 0;
        using Marks = std::vector<std::int32_t>;
        archive_reflection::PropertyCodecs sharedCodecs;
        if (!sharedCodecs.Register<Marks>(
            [&](archive::IArchive& ar, std::string_view key, const Marks& value)
            {
                ++writes;
                return ar.WriteArray(key, value);
            },
            [&](const archive::IArchive& ar, std::string_view key, Marks& value)
            {
                ++reads;
                return ar.ReadArray(key, value);
            }))
        {
            return Fail("Cannot register the shared virtual base codec probe.");
        }
        archive::BeveArchive sharedWriter;
        archive::BeveArchive sharedReader;
        Diamond sharedResult;
        std::string document;
        if (!archive_reflection::WriteObject(sharedWriter, "Object", *diamondType,
                reflection::ObjectView::From(diamond), &registry, &sharedCodecs) || !sharedWriter.Encode(document) ||
            !sharedReader.Decode(document) || !archive_reflection::ReadObject(
                sharedReader, "Object", *diamondType, sharedResult, &registry, &sharedCodecs) ||
            writes != 1 || reads != 1 || sharedResult != diamond)
        {
            return Fail("Shared virtual base fields were processed more than once.");
        }
        std::cout << "Inherited object JSON/BEVE, casts, collisions, defaults and failure checks passed.\n";
        return 0;
    }
}
