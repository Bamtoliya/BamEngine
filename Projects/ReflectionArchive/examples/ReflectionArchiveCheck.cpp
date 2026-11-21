#include <archive/BeveArchive.h>
#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/PropertyArchiveCodecs.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    int Fail(std::string_view message)
    {
        std::cerr << "ReflectionArchiveCheck failed: " << message << '\n';
        return 1;
    }

    enum class Mode : std::int8_t
    {
        Backward = -1,
        Idle = 0,
        Forward = 1
    };

    struct Item
    {
        std::string Label = "Default";
        float Weight = 1.0f;
        std::optional<std::int32_t> NestedOpt = 5;

        bool operator==(const Item&) const = default;
    };

    struct CustomPair
    {
        int Left = 0;
        int Right = 0;

        bool operator==(const CustomPair&) const = default;
    };

    struct UnregisteredStruct
    {
        int Value = 1;
    };

    struct OptionalBundle
    {
        std::optional<std::int32_t> OptInt;
        std::optional<float> OptFloat;
        std::optional<bool> OptBool;
        std::optional<std::string> OptString;
        std::optional<std::wstring> OptWString;
        std::optional<Mode> OptMode;
        std::optional<Item> OptItem;
        std::optional<std::vector<std::int32_t>> OptVec;
        std::optional<std::map<std::string, Item>> OptMap;
        std::optional<std::shared_ptr<Item>> OptPtr;
        std::optional<CustomPair> OptCustom;
        std::shared_ptr<Item> DirectPtr;

        bool operator==(const OptionalBundle& other) const
        {
            if (OptInt != other.OptInt || OptFloat != other.OptFloat || OptBool != other.OptBool ||
                OptString != other.OptString || OptWString != other.OptWString ||
                OptMode != other.OptMode || OptItem != other.OptItem ||
                OptVec != other.OptVec || OptMap != other.OptMap || OptCustom != other.OptCustom)
            {
                return false;
            }

            const bool optPtrEqual = OptPtr.has_value() == other.OptPtr.has_value() &&
                (!OptPtr.has_value() ||
                    ((!*OptPtr && !*other.OptPtr) || (*OptPtr && *other.OptPtr && **OptPtr == **other.OptPtr)));

            const bool directPtrEqual = (!DirectPtr && !other.DirectPtr) ||
                (DirectPtr && other.DirectPtr && *DirectPtr == *other.DirectPtr);

            return optPtrEqual && directPtrEqual;
        }
    };

    struct UniqueBundle
    {
        std::unique_ptr<Item> DirectUnique;
        std::optional<std::unique_ptr<Item>> OptUnique;
    };

    struct UnsupportedBundle
    {
        std::optional<UnregisteredStruct> MissingType;
    };

    struct ConstOptionalBundle
    {
        const std::optional<std::int32_t> ReadOnlyOpt = 12;
    };

    void RegisterTestTypes(reflection::Registry& registry, reflection::TypeInfo& outBundleType,
        reflection::TypeInfo& outUniqueType, reflection::TypeInfo& outUnsupportedType)
    {
        auto modeInfo = reflection::EnumInfo::For<Mode>("test::Mode");
        modeInfo.UnderlyingType = "std::int8_t";
        modeInfo.Entries = {
            { "Backward", std::int64_t{ -1 } },
            { "Idle", std::int64_t{ 0 } },
            { "Forward", std::int64_t{ 1 } }
        };
        static_cast<void>(registry.RegisterEnum(std::move(modeInfo)));

        auto itemType = reflection::TypeInfo::For<Item>("test::Item");
        itemType.Properties.push_back(reflection::MakeProperty<&Item::Label>("Label", "std::string"));
        itemType.Properties.push_back(reflection::MakeProperty<&Item::Weight>("Weight", "float"));
        itemType.Properties.push_back(reflection::MakeProperty<&Item::NestedOpt>("NestedOpt", "std::optional<int32_t>"));
        static_cast<void>(registry.RegisterType(std::move(itemType)));

        outBundleType = reflection::TypeInfo::For<OptionalBundle>("test::OptionalBundle");
        outBundleType.Properties.push_back(reflection::MakeProperty<&OptionalBundle::OptInt>("OptInt", "std::optional<int32_t>"));
        outBundleType.Properties.push_back(reflection::MakeProperty<&OptionalBundle::OptFloat>("OptFloat", "std::optional<float>"));
        outBundleType.Properties.push_back(reflection::MakeProperty<&OptionalBundle::OptBool>("OptBool", "std::optional<bool>"));
        outBundleType.Properties.push_back(reflection::MakeProperty<&OptionalBundle::OptString>("OptString", "std::optional<string>"));
        outBundleType.Properties.push_back(reflection::MakeProperty<&OptionalBundle::OptWString>("OptWString", "std::optional<wstring>"));
        outBundleType.Properties.push_back(reflection::MakeProperty<&OptionalBundle::OptMode>("OptMode", "std::optional<Mode>"));
        outBundleType.Properties.push_back(reflection::MakeProperty<&OptionalBundle::OptItem>("OptItem", "std::optional<Item>"));
        outBundleType.Properties.push_back(reflection::MakeProperty<&OptionalBundle::OptVec>("OptVec", "std::optional<vector<int32_t>>"));
        outBundleType.Properties.push_back(reflection::MakeProperty<&OptionalBundle::OptMap>("OptMap", "std::optional<map<string, Item>>"));
        outBundleType.Properties.push_back(reflection::MakeProperty<&OptionalBundle::OptPtr>("OptPtr", "std::optional<shared_ptr<Item>>"));
        outBundleType.Properties.push_back(reflection::MakeProperty<&OptionalBundle::OptCustom>("OptCustom", "std::optional<CustomPair>"));
        outBundleType.Properties.push_back(reflection::MakeProperty<&OptionalBundle::DirectPtr>("DirectPtr", "shared_ptr<Item>"));

        outUniqueType = reflection::TypeInfo::For<UniqueBundle>("test::UniqueBundle");
        outUniqueType.Properties.push_back(reflection::MakeProperty<&UniqueBundle::DirectUnique>("DirectUnique", "unique_ptr<Item>"));
        outUniqueType.Properties.push_back(reflection::MakeProperty<&UniqueBundle::OptUnique>("OptUnique", "std::optional<unique_ptr<Item>>"));

        outUnsupportedType = reflection::TypeInfo::For<UnsupportedBundle>("test::UnsupportedBundle");
        outUnsupportedType.Properties.push_back(reflection::MakeProperty<&UnsupportedBundle::MissingType>("MissingType", "std::optional<UnregisteredStruct>"));
    }

    archive_reflection::PropertyCodecs MakeCustomCodecs()
    {
        archive_reflection::PropertyCodecs codecs;
        static_cast<void>(codecs.Register<CustomPair>(
            [](archive::IArchive& ar, std::string_view key, const CustomPair& value) -> archive::ArchiveResult
            {
                return ar.WriteObject(key, value, [](archive::IArchive& entry, const CustomPair& src) -> archive::ArchiveResult
                {
                    if (const auto r = entry.Write("Left", src.Left); !r) return r;
                    return entry.Write("Right", src.Right);
                });
            },
            [](const archive::IArchive& ar, std::string_view key, CustomPair& value) -> archive::ArchiveResult
            {
                return ar.ReadObject(key, value, [](archive::IArchive& entry, CustomPair& dst) -> archive::ArchiveResult
                {
                    if (const auto r = entry.Read("Left", dst.Left); !r) return r;
                    return entry.Read("Right", dst.Right);
                });
            }));
        return codecs;
    }

    OptionalBundle MakePopulatedBundle()
    {
        OptionalBundle bundle;
        bundle.OptInt = -42;
        bundle.OptFloat = 3.25f;
        bundle.OptBool = true;
        bundle.OptString = "hello";
        bundle.OptWString = L"유니코드 \U0001F600";
        bundle.OptMode = Mode::Forward;
        bundle.OptItem = Item{ "Nested", 2.5f, 77 };
        bundle.OptVec = std::vector<std::int32_t>{ 10, 20, 30 };
        bundle.OptMap = std::map<std::string, Item>{
            { "alpha", Item{ "Alpha", 1.5f, std::nullopt } },
            { "beta", Item{ "Beta", 4.5f, 9 } }
        };
        bundle.OptPtr = std::make_shared<Item>(Item{ "OptShared", 8.0f, 11 });
        bundle.OptCustom = CustomPair{ 3, 7 };
        bundle.DirectPtr = std::make_shared<Item>(Item{ "DirectShared", 6.5f, 13 });
        return bundle;
    }

    template<typename Archive>
    bool CheckArchiveNullContract()
    {
        Archive writer;
        if (!writer.WriteNull("NullField") || !writer.Write("IntField", 7))
        {
            return false;
        }

        std::string encoded;
        Archive reader;
        if (!writer.Encode(encoded) || !reader.Decode(encoded))
        {
            return false;
        }

        bool isNull = false;
        if (!reader.IsNull("NullField", isNull) || !isNull)
        {
            return false;
        }

        isNull = true;
        if (!reader.IsNull("IntField", isNull) || isNull)
        {
            return false;
        }

        isNull = true;
        const auto missing = reader.IsNull("MissingField", isNull);
        if (missing.Code != archive::ArchiveErrorCode::MissingField || isNull)
        {
            return false;
        }

        return true;
    }

    template<typename Archive>
    bool CheckRoundTrip(const reflection::Registry& registry, const reflection::TypeInfo& bundleType,
        const archive_reflection::PropertyCodecs& codecs, const OptionalBundle& source, OptionalBundle initialTarget)
    {
        Archive writer;
        const auto writeResult = archive_reflection::WriteObject(
            writer, "Bundle", bundleType, reflection::ObjectView::From(source), &registry, &codecs);
        if (!writeResult)
        {
            return false;
        }

        std::string document;
        Archive reader;
        if (!writer.Encode(document) || !reader.Decode(document))
        {
            return false;
        }

        const auto readResult = archive_reflection::ReadObject(
            reader, "Bundle", bundleType, initialTarget, &registry, &codecs);
        return static_cast<bool>(readResult) && initialTarget == source;
    }

    template<typename Archive>
    bool CheckUniqueBundleRoundTrip(const reflection::Registry& registry, const reflection::TypeInfo& uniqueType,
        bool populated)
    {
        UniqueBundle source;
        if (populated)
        {
            source.DirectUnique = std::make_unique<Item>(Item{ "DirectU", 4.0f, 21 });
            source.OptUnique = std::make_unique<Item>(Item{ "OptU", 9.0f, std::nullopt });
        }

        Archive writer;
        if (!archive_reflection::WriteObject(writer, "Unique", uniqueType, reflection::ObjectView::From(source), &registry))
        {
            return false;
        }

        std::string document;
        Archive reader;
        if (!writer.Encode(document) || !reader.Decode(document))
        {
            return false;
        }

        UniqueBundle target;
        target.DirectUnique = std::make_unique<Item>(Item{ "Old", 0.0f, 1 });
        target.OptUnique = std::make_unique<Item>(Item{ "OldOpt", 0.0f, 2 });

        if (!reader.BeginReadObject("Unique"))
        {
            return false;
        }
        const auto readResult = archive_reflection::detail::ReadProperties(
            reader, uniqueType, reflection::ObjectView::From(target), &registry, nullptr);
        const auto endResult = reader.EndObject();
        if (!readResult || !endResult)
        {
            return false;
        }

        if (populated)
        {
            return target.DirectUnique && *target.DirectUnique == *source.DirectUnique &&
                target.OptUnique.has_value() && *target.OptUnique && **target.OptUnique == **source.OptUnique;
        }
        return target.DirectUnique == nullptr && !target.OptUnique.has_value();
    }
}

int main()
{
    reflection::Registry registry;
    reflection::TypeInfo bundleType;
    reflection::TypeInfo uniqueType;
    reflection::TypeInfo unsupportedType;
    RegisterTestTypes(registry, bundleType, uniqueType, unsupportedType);
    const auto codecs = MakeCustomCodecs();

    if (!CheckArchiveNullContract<archive::JsonArchive>() || !CheckArchiveNullContract<archive::BeveArchive>())
    {
        return Fail("IArchive WriteNull/IsNull contract failed.");
    }

    const OptionalBundle populated = MakePopulatedBundle();
    const OptionalBundle empty{};

    if (!CheckRoundTrip<archive::JsonArchive>(registry, bundleType, codecs, populated, empty) ||
        !CheckRoundTrip<archive::BeveArchive>(registry, bundleType, codecs, populated, empty))
    {
        return Fail("Populated OptionalBundle JSON/BEVE round-trip failed.");
    }

    if (!CheckRoundTrip<archive::JsonArchive>(registry, bundleType, codecs, empty, populated) ||
        !CheckRoundTrip<archive::BeveArchive>(registry, bundleType, codecs, empty, populated))
    {
        return Fail("Nullopt OptionalBundle JSON/BEVE round-trip failed.");
    }

    // std::nullopt 필드가 JSON에 명시적 null로 기록되는지 확인합니다.
    {
        archive::JsonArchive nullWriter;
        if (!archive_reflection::WriteObject(
            nullWriter, "Bundle", bundleType, reflection::ObjectView::From(empty), &registry, &codecs))
        {
            return Fail("Failed to write empty OptionalBundle.");
        }
        std::string json;
        if (!nullWriter.ToJson(json) || json.find("\"OptInt\": null") == std::string::npos ||
            json.find("\"OptItem\": null") == std::string::npos ||
            json.find("\"OptPtr\": null") == std::string::npos)
        {
            return Fail("Serialized JSON did not emit explicit null for std::nullopt.");
        }
    }

    // 필드가 아예 누락된 JSON과 명시적 null JSON 모두 std::nullopt / nullptr로 복원되는지 확인합니다.
    {
        archive::JsonArchive omittedReader;
        if (!omittedReader.Parse(R"({"Bundle":{}})"))
        {
            return Fail("Failed to parse empty JSON object.");
        }
        OptionalBundle restored = populated;
        if (!archive_reflection::ReadObject(omittedReader, "Bundle", bundleType, restored, &registry, &codecs) ||
            !(restored == empty))
        {
            return Fail("Omitted JSON fields did not reset std::optional/smart pointer members.");
        }

        archive::JsonArchive explicitNullReader;
        if (!explicitNullReader.Parse(R"({"Bundle":{"DirectPtr":null,"OptInt":null,"OptPtr":null}})"))
        {
            return Fail("Failed to parse explicit null smart pointer JSON.");
        }
        OptionalBundle explicitRestored = populated;
        if (!archive_reflection::ReadObject(
                explicitNullReader, "Bundle", bundleType, explicitRestored, &registry, &codecs) ||
            explicitRestored.DirectPtr != nullptr || explicitRestored.OptInt.has_value() ||
            explicitRestored.OptPtr.has_value())
        {
            return Fail("Explicit null literal did not reset smart pointer or optional.");
        }
    }

    // 타입 불일치 엄격 실패 및 대상 객체 비오염 검증
    {
        struct FailureCase
        {
            const char* Json;
            const char* ExpectedField;
        };

        const FailureCase failures[] = {
            { R"({"Bundle":{"OptInt":"bad"}})", "OptInt" },
            { R"({"Bundle":{"OptMode":"bad"}})", "OptMode" },
            { R"({"Bundle":{"OptMode":128}})", "OptMode" },
            { R"({"Bundle":{"OptItem":123}})", "OptItem" },
            { R"({"Bundle":{"OptItem":{"Label":"ok","Weight":1.0,"NestedOpt":"bad"}}})", "OptItem.NestedOpt" },
            { R"({"Bundle":{"OptVec":"bad"}})", "OptVec" },
            { R"({"Bundle":{"OptMap":[{"Key":"k","Value":123}]}})", "OptMap[0].Value" },
            { R"({"Bundle":{"OptPtr":"bad"}})", "OptPtr" },
            { R"({"Bundle":{"DirectPtr":"bad"}})", "DirectPtr" },
            { R"({"Bundle":{"DirectPtr":123}})", "DirectPtr" },
            { R"({"Bundle":{"OptCustom":"bad"}})", "OptCustom" }
        };

        for (const auto& testCase : failures)
        {
            archive::JsonArchive reader;
            if (!reader.Parse(testCase.Json))
            {
                return Fail("Failed to parse mismatch test JSON.");
            }
            OptionalBundle unchanged = populated;
            const auto result = archive_reflection::ReadObject(
                reader, "Bundle", bundleType, unchanged, &registry, &codecs);
            if (result.Code != archive::ArchiveErrorCode::InvalidValue ||
                result.Field != testCase.ExpectedField || !(unchanged == populated))
            {
                return Fail(std::string{ "Mismatch error or target preservation failed for field: " } +
                    testCase.ExpectedField);
            }
        }
    }

    // unique_ptr 및 optional<unique_ptr> 왕복 및 타입 불일치 검증
    if (!CheckUniqueBundleRoundTrip<archive::JsonArchive>(registry, uniqueType, true) ||
        !CheckUniqueBundleRoundTrip<archive::JsonArchive>(registry, uniqueType, false) ||
        !CheckUniqueBundleRoundTrip<archive::BeveArchive>(registry, uniqueType, true) ||
        !CheckUniqueBundleRoundTrip<archive::BeveArchive>(registry, uniqueType, false))
    {
        return Fail("UniqueBundle round-trip failed.");
    }

    {
        archive::JsonArchive reader;
        if (!reader.Parse(R"({"Unique":{"DirectUnique":"bad","OptUnique":123}})"))
        {
            return Fail("Failed to parse UniqueBundle mismatch JSON.");
        }
        UniqueBundle target;
        if (!reader.BeginReadObject("Unique"))
        {
            return Fail("BeginReadObject failed on UniqueBundle mismatch.");
        }
        const auto res = archive_reflection::detail::ReadProperties(
            reader, uniqueType, reflection::ObjectView::From(target), &registry, nullptr);
        const auto endRes = reader.EndObject();
        if (res.Code != archive::ArchiveErrorCode::InvalidValue || res.Field != "DirectUnique" ||
            !endRes || target.DirectUnique != nullptr || target.OptUnique.has_value())
        {
            return Fail("UniqueBundle mismatch error or clean state check failed.");
        }
    }

    // const std::optional 읽기 전용 쓰기 상태 검증
    {
        const auto readOnlyProp = reflection::MakeProperty<&ConstOptionalBundle::ReadOnlyOpt>(
            "ReadOnlyOpt", "const std::optional<int32_t>");
        if (readOnlyProp.CanWrite() || readOnlyProp.GetWriteStatus() != reflection::PropertyAccessStatus::ConstQualified)
        {
            return Fail("const std::optional<T> should not be writable.");
        }
    }

    // 미등록 내부 타입 std::optional 조용한 생략 금지 검증
    {
        UnsupportedBundle unsupported;
        unsupported.MissingType = UnregisteredStruct{ 9 };
        archive::JsonArchive writer;
        const auto writeRes = archive_reflection::WriteObject(
            writer, "Unsupported", unsupportedType, reflection::ObjectView::From(unsupported), &registry);
        if (writeRes.Code != archive::ArchiveErrorCode::UnsupportedType || writeRes.Field != "MissingType")
        {
            return Fail("Unregistered optional inner type write did not return UnsupportedType.");
        }

        archive::JsonArchive reader;
        if (!reader.Parse(R"({"Unsupported":{"MissingType":{"Value":5}}})"))
        {
            return Fail("Failed to parse unsupported inner type JSON.");
        }
        const auto readRes = archive_reflection::ReadObject(
            reader, "Unsupported", unsupportedType, unsupported, &registry);
        if (readRes.Code != archive::ArchiveErrorCode::UnsupportedType || readRes.Field != "MissingType")
        {
            return Fail("Unregistered optional inner type read did not return UnsupportedType.");
        }
    }

    std::cout << "ReflectionArchive standalone optional and smart pointer checks passed.\n";
    return 0;
}
