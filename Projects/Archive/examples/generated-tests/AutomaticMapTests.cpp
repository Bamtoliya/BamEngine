#include "TestSupport.h"
#include "../GeneratedArchiveTypes.h"

#include <archive/BeveArchive.h>
#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace archive_generated_tests
{
    namespace
    {
        struct DirectionalLess
        {
            bool Descending = false;
            bool operator()(std::int64_t left, std::int64_t right) const
            {
                return Descending ? left > right : left < right;
            }
        };

        struct SaltedHash
        {
            std::size_t Salt = 0;
            std::size_t operator()(const std::wstring& value) const
            {
                return std::hash<std::wstring>{}(value) ^ Salt;
            }
        };

        using Settings = generated_archive::Settings;
        using Numbers = std::map<std::int64_t, std::wstring, DirectionalLess>;
        using Counts = std::unordered_map<std::wstring, std::uint64_t, SaltedHash>;
        using Objects = std::map<std::int32_t, Settings>;

        struct MapProfile
        {
            Numbers NumbersById{ { { -1, L"음수" }, { 7, L"항목 \U0001F600" } }, DirectionalLess{ true } };
            Counts CountsByName{ { { L"최대", (std::numeric_limits<std::uint64_t>::max)() } }, 0, SaltedHash{ 17 } };
            Objects ObjectsById{ { 1, Settings{} } };
            std::unordered_map<std::wstring, Settings> NamedObjects{ { L"설정 \U0001F600", Settings{} } };
        };

        reflection::TypeInfo MakeMapProfileType()
        {
            auto type = reflection::TypeInfo::For<MapProfile>("tests::MapProfile");
            type.Properties.push_back(reflection::MakeProperty<&MapProfile::NumbersById>("Numbers", "Numbers"));
            type.Properties.push_back(reflection::MakeProperty<&MapProfile::CountsByName>("Counts", "Counts"));
            type.Properties.push_back(reflection::MakeProperty<&MapProfile::ObjectsById>("Objects", "Objects"));
            type.Properties.push_back(reflection::MakeProperty<&MapProfile::NamedObjects>("Named", "Named"));
            return type;
        }

        bool SameProfile(const MapProfile& left, const MapProfile& right)
        {
            const auto sameObjects = [](const auto& lhs, const auto& rhs)
            {
                if (lhs.size() != rhs.size())
                {
                    return false;
                }

                for (const auto& [key, value] : lhs)
                {
                    const auto found = rhs.find(key);

                    if (found == rhs.end() || value.Title != found->second.Title ||
                        value.Speed != found->second.Speed || value.Retries != found->second.Retries ||
                        value.CachedSpeed != found->second.CachedSpeed)
                    {
                        return false;
                    }
                }

                return true;
            };

            if (left.NumbersById.size() != right.NumbersById.size() || left.CountsByName != right.CountsByName)
            {
                return false;
            }

            // 비교자 상태가 다르면 순회 순서도 달라지므로 키로 내용을 비교합니다.
            for (const auto& [key, value] : left.NumbersById)
            {
                const auto found = right.NumbersById.find(key);

                if (found == right.NumbersById.end() || value != found->second)
                {
                    return false;
                }
            }

            return sameObjects(left.ObjectsById, right.ObjectsById) &&
                sameObjects(left.NamedObjects, right.NamedObjects);
        }

        std::string MakeDocument(std::string_view field, std::string_view array)
        {
            std::string result = R"({"Maps":{)";

            for (const auto name : { "Numbers", "Counts", "Objects", "Named" })
            {
                if (name != std::string_view{ "Numbers" })
                {
                    result += ',';
                }

                result += '"';
                result += name;
                result += "\":";
                result += field == name ? array : "[]";
            }

            return result + "}}";
        }
    }

    int CheckAutomaticMaps(const reflection::Registry& registry)
    {
        using Code = archive::ArchiveErrorCode;
        using Error = reflection::PropertyAccessError;
        const auto type = MakeMapProfileType();
        MapProfile source;
        source.ObjectsById.at(1).Retries = 77;

        const auto roundTrip = [&]<typename Archive>(const MapProfile& expected)
        {
            Archive writer;
            Archive reader;
            std::string encoded;
            MapProfile actual;
            actual.NumbersById = Numbers{ DirectionalLess{ false } };
            actual.CountsByName = Counts{ 0, SaltedHash{ 99 } };
            actual.CountsByName.max_load_factor(0.7f);

            return archive_reflection::WriteObject(writer, "Maps", type,
                reflection::ObjectView::From(expected), &registry) && writer.Encode(encoded) &&
                reader.Decode(encoded) && archive_reflection::ReadObject(reader, "Maps", type, actual, &registry) &&
                SameProfile(expected, actual) &&
                !actual.NumbersById.key_comp().Descending && actual.CountsByName.hash_function().Salt == 99 &&
                actual.CountsByName.max_load_factor() == 0.7f;
        };

        MapProfile empty;
        empty.NumbersById.clear();
        empty.CountsByName.clear();
        empty.ObjectsById.clear();
        empty.NamedObjects.clear();

        if (!roundTrip.operator()<archive::JsonArchive>(source) ||
            !roundTrip.operator()<archive::BeveArchive>(source) ||
            !roundTrip.operator()<archive::JsonArchive>(empty) || !roundTrip.operator()<archive::BeveArchive>(empty))
        {
            return Fail("Automatic map JSON/BEVE round-trip or comparator/hash state preservation failed.");
        }

        struct InvalidInput
        {
            std::string_view Field;
            std::string_view Array;
            Code ExpectedCode;
            std::string_view ExpectedField;
        };

        const InvalidInput invalidInputs[]{
            { "Numbers", "{}", Code::InvalidValue, "Numbers" },
            { "Numbers", "[7]", Code::InvalidValue, "Numbers[0]" },
            { "Numbers", R"([{"Value":"Text"}])", Code::MissingField, "Numbers[0].Key" },
            { "Numbers", R"([{"Key":1}])", Code::MissingField, "Numbers[0].Value" },
            { "Numbers", R"([{"Key":"bad","Value":"Text"}])", Code::InvalidValue, "Numbers[0].Key" },
            { "Numbers", R"([{"Key":1,"Value":"A"},{"Key":1,"Value":"B"}])",
                Code::InvalidValue, "Numbers[1].Key" },
            { "Counts", R"([{"Key":"Name","Value":-1}])", Code::InvalidValue, "Counts[0].Value" },
            { "Counts", R"([{"Key":"Name","Value":1},{"Key":"Name","Value":2}])",
                Code::InvalidValue, "Counts[1].Key" },
            { "Objects", R"([{"Key":1}])", Code::MissingField, "Objects[0].Value" },
            { "Objects", R"([{"Key":1,"Value":7}])", Code::InvalidValue, "Objects[0].Value" },
            { "Objects", R"([{"Key":1,"Value":{"Title":"A","Speed":"bad"}}])",
                Code::InvalidValue, "Objects[0].Value.Speed" },
            { "Objects", R"([{"Key":1,"Value":{"Speed":1}}])", Code::MissingField, "Objects[0].Value.Title" },
            { "Objects", R"([{"Key":1,"Value":{"Title":"A","Speed":1}},)"
                R"({"Key":1,"Value":{"Title":"B","Speed":2}}])", Code::InvalidValue, "Objects[1].Key" },
            { "Named", R"([{"Key":"Name","Value":{"Title":"A","Speed":1}},)"
                R"({"Key":"Name","Value":{"Title":"B","Speed":2}}])", Code::InvalidValue, "Named[1].Key" }
        };

        archive::JsonArchive reader;

        for (const auto& input : invalidInputs)
        {
            MapProfile destination = source;

            if (!reader.Parse(MakeDocument(input.Field, input.Array)))
            {
                return Fail("Cannot prepare invalid automatic map input.");
            }

            const auto result = archive_reflection::ReadObject(reader, "Maps", type, destination, &registry);

            if (result.Code != input.ExpectedCode || result.Field != input.ExpectedField ||
                !SameProfile(source, destination))
            {
                std::cerr << "Map error: code=" << static_cast<int>(result.Code) << ", field=" << result.Field << '\n';
                return Fail("Automatic map error path or destination preservation failed.");
            }
        }

        MapProfile restored = source;

        if (!reader.Parse(MakeDocument("Objects", R"([{"Key":1,"Value":{"Title":"Legacy","Speed":5}}])")) ||
            !archive_reflection::ReadObject(reader, "Maps", type, restored, &registry) ||
            restored.ObjectsById.size() != 1 || restored.ObjectsById.at(1).Retries != 3 ||
            restored.ObjectsById.at(1).CachedSpeed != std::numeric_limits<float>::infinity())
        {
            return Fail("Automatic object map did not replace existing values with fresh defaults.");
        }

        archive::JsonArchive writer;
        std::string before;
        std::string after;

        if (!archive_reflection::WriteObject(writer, "Maps", type, reflection::ObjectView::From(source), &registry) ||
            !writer.ToJson(before))
        {
            return Fail("Cannot prepare existing automatic map document.");
        }

        const reflection::Registry missingRegistry;

        for (const auto* lookup : { static_cast<const reflection::Registry*>(nullptr), &missingRegistry })
        {
            for (const auto* value : { &source, &empty })
            {
                const auto result = archive_reflection::WriteObject(
                    writer, "Maps", type, reflection::ObjectView::From(*value), lookup);

                if (result.Code != Code::UnsupportedType || result.Field != "Objects" ||
                    !writer.ToJson(after) || before != after)
                {
                    return Fail("Unregistered mapped object was accepted or changed the document.");
                }
            }

            for (const auto array : { "[]", R"([{"Key":1,"Value":{"Title":"A","Speed":1}}])" })
            {
                MapProfile destination = source;

                if (!reader.Parse(MakeDocument("Objects", array)))
                {
                    return Fail("Cannot prepare unregistered map reading.");
                }

                const auto result = archive_reflection::ReadObject(reader, "Maps", type, destination, lookup);

                if (result.Code != Code::UnsupportedType || result.Field != "Objects" ||
                    !SameProfile(source, destination))
                {
                    return Fail("Reading an unregistered mapped object changed the destination.");
                }
            }
        }

        MapProfile invalid = source;
        invalid.ObjectsById.at(1).Speed = std::numeric_limits<float>::infinity();
        const auto writeFailure = archive_reflection::WriteObject(
            writer, "Maps", type, reflection::ObjectView::From(invalid), &registry);

        if (writeFailure.Code != Code::InvalidValue || writeFailure.Field != "Objects[0].Value.Speed" ||
            !writer.ToJson(after) || before != after)
        {
            return Fail("Invalid mapped object changed the existing document.");
        }

        archive_reflection::PropertyCodecs rejectingCodecs;

        if (!rejectingCodecs.Register<Objects>(
            [](archive::IArchive&, std::string_view key, const Objects&) -> archive::ArchiveResult
            {
                return { Code::UnsupportedType, std::string{ key } };
            },
            [](const archive::IArchive&, std::string_view key, Objects&) -> archive::ArchiveResult
            {
                return { Code::UnsupportedType, std::string{ key } };
            }) || !reader.Parse(MakeDocument("Objects", "[]")))
        {
            return Fail("Cannot prepare rejecting map codec.");
        }

        MapProfile destination = source;
        const auto rejectedWrite = archive_reflection::WriteObject(
            writer, "Maps", type, reflection::ObjectView::From(source), &registry, &rejectingCodecs);
        const auto rejectedRead = archive_reflection::ReadObject(
            reader, "Maps", type, destination, &registry, &rejectingCodecs);

        if (rejectedWrite.Code != Code::UnsupportedType || rejectedWrite.Field != "Objects" ||
            rejectedRead.Code != Code::UnsupportedType || rejectedRead.Field != "Objects" ||
            !SameProfile(source, destination) || !writer.ToJson(after) || before != after)
        {
            return Fail("A map codec error fell through to automatic processing.");
        }

        const auto property = reflection::MakeProperty<&MapProfile::ObjectsById>("Objects", "Objects");
        const std::int32_t newKey = 2;
        const std::int64_t wrongKey = 2;
        const auto key = reflection::ValueView::From(newKey);
        const auto readOnly = reflection::ObjectView::From(static_cast<const MapProfile&>(source));
        const auto destinationObject = reflection::ObjectView::From(destination);
        const auto emplaced = property.TryEmplaceMapValueObject(destinationObject, key);

        if (!property.CanEmplaceMapValueObject() || !emplaced || !emplaced.Object.Is<Settings>() ||
            property.TryEmplaceMapValueObject(destinationObject, key).Error != Error::KeyAlreadyExists ||
            property.TryEmplaceMapValueObject(readOnly, key).Error != Error::ReadOnlyObject ||
            property.TryEmplaceMapValueObject({}, key).Error != Error::InvalidObject ||
            property.TryEmplaceMapValueObject(destinationObject, {}).Error != Error::InvalidValue ||
            property.TryEmplaceMapValueObject(destinationObject,
                reflection::ValueView::From(wrongKey)).Error != Error::KeyTypeMismatch)
        {
            return Fail("Map object emplacement access checks failed.");
        }

        struct NoDefault
        {
            explicit NoDefault(int) {}
        };
        struct UnsupportedMaps
        {
            const Objects ReadOnly;
            std::map<std::int32_t, NoDefault> NoDefaultValues;
            std::map<std::vector<std::int32_t>, std::int32_t> ContainerKeys;
        };

        const auto constProperty = reflection::MakeProperty<&UnsupportedMaps::ReadOnly>("ReadOnly", "ReadOnly");
        const auto noDefault = reflection::MakeProperty<&UnsupportedMaps::NoDefaultValues>("Values", "Values");
        UnsupportedMaps unsupported;
        const auto unsupportedObject = reflection::ObjectView::From(unsupported);

        if (constProperty.CanEmplaceMapValueObject() || noDefault.CanEmplaceMapValueObject() ||
            constProperty.TryEmplaceMapValueObject(unsupportedObject, key).Error != Error::ReadOnlyProperty ||
            noDefault.TryEmplaceMapValueObject(unsupportedObject, key).Error != Error::InsertUnavailable)
        {
            return Fail("Read-only or non-default-constructible mapped objects were accepted for emplacement.");
        }

        auto unsupportedType = reflection::TypeInfo::For<UnsupportedMaps>("tests::UnsupportedMaps");
        unsupportedType.Properties.push_back(
            reflection::MakeProperty<&UnsupportedMaps::ContainerKeys>("Keys", "Keys"));
        const auto unsupportedWrite = archive_reflection::WriteObject(
            writer, "Unsupported", unsupportedType, reflection::ObjectView::From(unsupported), &registry);

        if (unsupportedWrite.Code != Code::UnsupportedType || unsupportedWrite.Field != "Keys" ||
            !writer.ToJson(after) || after != before)
        {
            return Fail("An unsupported empty map key type was silently accepted.");
        }

        std::cout << "Automatic map JSON/BEVE, policy, codec compatibility and failure checks passed.\n";
        return 0;
    }
}
