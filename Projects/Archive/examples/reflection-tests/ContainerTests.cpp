#include "TestSupport.h"

#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_set>
#include <vector>

namespace archive_reflection_tests
{
    namespace
    {
        struct ContainerSettings
        {
            std::int32_t Level = 3;
            std::vector<std::int32_t> Values{ 1, -2, 300 };
            std::vector<bool> Flags{ true, false, true };
            std::array<double, 2> Samples{ 0.25, 0.75 };
            std::unordered_set<std::wstring> Tags{ L"플레이어", L"이동 \U0001F600" };
        };

        bool SameContainers(const ContainerSettings& lhs, const ContainerSettings& rhs)
        {
            return lhs.Level == rhs.Level && lhs.Values == rhs.Values && lhs.Flags == rhs.Flags &&
                lhs.Samples == rhs.Samples && lhs.Tags == rhs.Tags;
        }
    }

    int CheckContainerArchive(const std::filesystem::path& directory)
    {
        archive_reflection::PropertyCodecs codecs;

        if (!codecs.RegisterArray<std::vector<std::int32_t>>() ||
            !codecs.RegisterArray<std::vector<bool>>() ||
            !codecs.RegisterArray<std::array<double, 2>>() ||
            !codecs.RegisterArray<std::unordered_set<std::wstring>>())
        {
            return Fail("Container codec registration failed.");
        }

        auto type = reflection::TypeInfo::For<ContainerSettings>("example::ContainerSettings");
        type.Properties.push_back(reflection::MakeProperty<&ContainerSettings::Level>("Level", "std::int32_t"));
        type.Properties.push_back(
            reflection::MakeProperty<&ContainerSettings::Values>("Values", "std::vector<std::int32_t>"));
        type.Properties.push_back(
            reflection::MakeProperty<&ContainerSettings::Flags>("Flags", "std::vector<bool>"));
        type.Properties.push_back(
            reflection::MakeProperty<&ContainerSettings::Samples>("Samples", "std::array<double, 2>"));
        type.Properties.push_back(
            reflection::MakeProperty<&ContainerSettings::Tags>("Tags", "std::unordered_set<std::wstring>"));

        const ContainerSettings source;
        archive::JsonArchive writer;

        if (!archive_reflection::WriteObject(
            writer, "Containers", type, reflection::ObjectView::From(source), nullptr, &codecs))
        {
            return Fail("Reflected container writing failed.");
        }

        const auto path = directory / "reflection-containers.json";
        archive::JsonArchive reader;
        ContainerSettings restored;
        restored.Level = -1;
        restored.Values = { 9 };
        restored.Flags = { false };
        restored.Samples = { -1.0, -2.0 };
        restored.Tags = { L"Before" };

        if (!writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Containers", type, restored, nullptr, &codecs) ||
            !SameContainers(restored, source))
        {
            return Fail("Reflected container round-trip failed.");
        }

        using Code = archive::ArchiveErrorCode;

        struct FailureCase
        {
            const char* Json;
            const char* Field;
            Code Expected;
        };

        const FailureCase failures[] = {
            {
                R"({"Containers":{"Level":99,"Values":[1,2147483648]}})",
                "Values", Code::InvalidValue
            },
            {
                R"({"Containers":{"Level":99,"Values":[9],"Flags":[true,1]}})",
                "Flags", Code::InvalidValue
            },
            {
                R"({"Containers":{"Level":99,"Values":[9],"Flags":[false],"Samples":[1]}})",
                "Samples", Code::InvalidValue
            },
            {
                R"({"Containers":{"Level":99,"Values":[9],"Flags":[false],)"
                R"("Samples":[1,2],"Tags":["same","same"]}})",
                "Tags", Code::InvalidValue
            },
            {
                R"({"Containers":{"Level":99,"Values":[9],"Flags":[false],"Samples":[1,2]}})",
                "Tags", Code::MissingField
            }
        };

        for (const auto& failure : failures)
        {
            archive::JsonArchive failureReader;

            if (!failureReader.Parse(failure.Json))
            {
                return Fail("Cannot prepare reflected container failure.");
            }

            ContainerSettings unchanged = source;
            const auto result = archive_reflection::ReadObject(
                failureReader, "Containers", type, unchanged, nullptr, &codecs);

            if (result.Code != failure.Expected || result.Field != failure.Field ||
                !SameContainers(unchanged, source))
            {
                return Fail("Failed container reading changed the destination.");
            }
        }

        std::string beforeFailure;

        if (!writer.ToJson(beforeFailure))
        {
            return Fail("Cannot capture the container document.");
        }

        ContainerSettings invalid = source;
        invalid.Level = 99;
        invalid.Values = { 9 };
        invalid.Flags = { false };
        invalid.Samples[1] = std::numeric_limits<double>::infinity();

        const auto result = archive_reflection::WriteObject(
            writer, "Containers", type, reflection::ObjectView::From(invalid), nullptr, &codecs);

        std::string afterFailure;

        if (result.Code != Code::InvalidValue || result.Field != "Samples" ||
            !writer.ToJson(afterFailure) || afterFailure != beforeFailure)
        {
            return Fail("Failed container writing changed the document.");
        }

        std::cout << beforeFailure << '\n';
        std::cout << "Reflection container archive checks passed.\n";
        std::cout << "Reflection container archive failure checks passed.\n";
        return 0;
    }
}