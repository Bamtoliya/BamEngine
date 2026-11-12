#include "RenderPipelineProfile.h"

#include <archive/JsonArchive.h>
#include <algorithm>
#include <initializer_list>
#include <string_view>
#include <utility>

namespace Engine
{
    namespace
    {
        using archive::ArchiveErrorCode;
        using archive::ArchiveResult;
        using archive::IArchive;

        ArchiveResult CheckProfileFields(const IArchive& object, std::initializer_list<std::string_view> supported)
        {
            std::vector<std::string> names;
            auto result = object.GetFieldNames(names);
            if (!result)
                return result;

            for (const auto& name : names)
            {
                if (std::ranges::find(supported, std::string_view(name)) == supported.end())
                    return { ArchiveErrorCode::UnsupportedType, name };
            }
            return {};
        }

        ArchiveResult ReadProfilePass(IArchive& object, RenderPipelinePassProfile& pass)
        {
            auto result = CheckProfileFields(object, { "Id", "Type", "Inputs", "Settings" });
            if (result) result = object.Read("Id", pass.Id);
            if (result) result = object.Read("Type", pass.Type);
            if (!result)
                return result;

            result = object.ReadObject("Inputs", pass.Inputs, [](IArchive& inputs, auto& values) -> ArchiveResult
            {
                std::vector<std::string> names;
                auto read = inputs.GetFieldNames(names);
                if (!read)
                    return read;

                for (const auto& name : names)
                {
                    std::string reference;
                    read = inputs.Read(name, reference);
                    if (!read)
                        return { read.Code, "Inputs." + name };
                    values.emplace(name, std::move(reference));
                }
                return {};
            });
            if (!result && (result.Code != ArchiveErrorCode::MissingField || result.Field != "Inputs"))
                return result;
            // Missing Inputs is valid for passes with no inputs. Type validation checks required slots later.

            result = object.ReadObject("Settings", pass.Resolution, [](IArchive& settings, auto& resolution) -> ArchiveResult
            {
                auto read = CheckProfileFields(settings, { "Resolution" });
                if (!read)
                    return { read.Code, "Settings." + read.Field };

                bool hasResolution = false;
                read = settings.HasField("Resolution", hasResolution);
                if (!read || !hasResolution)
                    return read;

                std::uint32_t value = 0;
                read = settings.Read("Resolution", value);
                if (!read)
                    return { read.Code, "Settings.Resolution" };
                resolution = value;
                return {};
            });
            if (result.Code == ArchiveErrorCode::MissingField && result.Field == "Settings")
                return {};
            return result;
        }

        ArchiveResult WriteProfilePass(IArchive& object, const RenderPipelinePassProfile& pass)
        {
            auto result = object.Write("Id", pass.Id);
            if (result) result = object.Write("Type", pass.Type);
            if (result && !pass.Inputs.empty())
            {
                result = object.WriteObject("Inputs", pass.Inputs, [](IArchive& inputs, const auto& values) -> ArchiveResult
                {
                    for (const auto& [name, reference] : values)
                    {
                        const auto written = inputs.Write(name, reference);
                        if (!written)
                            return { written.Code, "Inputs." + name };
                    }
                    return {};
                });
            }
            if (result && pass.Resolution)
            {
                result = object.WriteObject("Settings", *pass.Resolution, [](IArchive& settings, std::uint32_t value)
                {
                    return settings.Write("Resolution", value);
                });
            }
            return result;
        }

        RenderPipelineProfileResult ProfileArchiveError(
            const std::filesystem::path& path, const ArchiveResult& result, std::string_view operation)
        {
            std::string message;
            switch (result.Code)
            {
            case ArchiveErrorCode::MissingField: message = "Missing field."; break;
            case ArchiveErrorCode::InvalidValue: message = "Invalid field value or type."; break;
            case ArchiveErrorCode::InvalidJson: message = "Invalid JSON."; break;
            case ArchiveErrorCode::UnsupportedType: message = "Unknown field or unsupported type."; break;
            case ArchiveErrorCode::FileOpenFailed: message = "Could not open file."; break;
            case ArchiveErrorCode::FileReadFailed: message = "Could not read file."; break;
            case ArchiveErrorCode::FileWriteFailed: message = "Could not write file."; break;
            default: message = "Archive error " + std::to_string(static_cast<int>(result.Code)) + "."; break;
            }
            return { path, result.Field, std::string(operation) + ": " + message };
        }
    }

    RenderPipelineProfileResult LoadRenderPipelineProfile(const std::filesystem::path& path, RenderPipelineProfile& profile)
    {
        archive::JsonArchive reader;
        auto result = reader.LoadFromFile(path);
        if (!result)
            return ProfileArchiveError(path, result, "Load render pipeline profile");

        RenderPipelineProfile candidate;
        result = reader.Read("Version", candidate.Version);
        if (!result)
            return ProfileArchiveError(path, result, "Load render pipeline profile");
        if (candidate.Version != RenderPipelineProfile::CurrentVersion)
            return { path, "Version", "Unsupported render pipeline profile version." };

        result = CheckProfileFields(reader, { "Version", "Passes", "Output" });
        if (result) result = reader.ReadObjectArray("Passes", candidate.Passes, ReadProfilePass);
        if (result) result = reader.Read("Output", candidate.Output);
        if (!result)
            return ProfileArchiveError(path, result, "Load render pipeline profile");

        auto validation = ValidateRenderPipelineProfile(candidate);
        if (!validation)
        {
            validation.Path = path;
            return validation;
        }

        profile = std::move(candidate);
        return {};
    }

    RenderPipelineProfileResult SaveRenderPipelineProfile(const std::filesystem::path& path, const RenderPipelineProfile& profile)
    {
        auto validation = ValidateRenderPipelineProfile(profile);
        if (!validation)
        {
            validation.Path = path;
            return validation;
        }

        archive::JsonArchive writer;
        auto result = writer.Write("Version", profile.Version);
        if (result) result = writer.WriteObjectArray("Passes", profile.Passes, WriteProfilePass);
        if (result) result = writer.Write("Output", profile.Output);
        if (result) result = writer.SaveToFile(path);
        if (!result)
            return ProfileArchiveError(path, result, "Save render pipeline profile");
        return {};
    }
}
