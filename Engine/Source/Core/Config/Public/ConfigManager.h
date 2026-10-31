#pragma once
#include "Engine_API.h"
#include <archive/JsonArchive.h>
#include <reflection/Registry.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <filesystem>
#include <system_error>

namespace Engine
{
    enum class EConfigLoadStatus
    {
        Loaded,
        NotFound,
        Failed
    };

    struct ConfigLoadResult
    {
        EConfigLoadStatus Status = EConfigLoadStatus::Failed;
        archive::ArchiveResult Error;
        std::error_code FileError;
    };

    struct ConfigSaveResult
    {
        archive::ArchiveResult Error;
        std::error_code FileError;

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return static_cast<bool>(Error) && !FileError;
        }
    };

    class ENGINE_API ConfigManager final
    {
    public:
        template<typename T>
        [[nodiscard]] static ConfigLoadResult Load(T& config, const std::filesystem::path& path,
            const reflection::Registry& registry)
        {
            const auto* type = registry.FindType<T>();
            if (!type)
            {
                return { EConfigLoadStatus::Failed,
                    { archive::ArchiveErrorCode::UnsupportedType, "Settings" }, {} };
            }

            std::error_code error;
            const bool exists = std::filesystem::exists(path, error);

            if (error)
                return { EConfigLoadStatus::Failed, {}, error };

            if (!exists)
                return { EConfigLoadStatus::NotFound, {}, {} };

            archive::JsonArchive reader;
            auto result = reader.LoadFromFile(path);
            if (!result)
                return { EConfigLoadStatus::Failed, result, {} };

            result = archive_reflection::ReadObject(reader, "Settings", *type, config, &registry);
            if (!result)
                return { EConfigLoadStatus::Failed, result, {} };

            return { EConfigLoadStatus::Loaded, {}, {} };
        }
    public:
        template<typename T>
        [[nodiscard]] static ConfigSaveResult Save(const T& config, const std::filesystem::path& path,
            const reflection::Registry& registry)
        {
            const auto* type = registry.FindType<T>();
            if (!type)
                return { { archive::ArchiveErrorCode::UnsupportedType, "Settings" }, {} };

            archive::JsonArchive writer;
            const auto result = archive_reflection::WriteObject(
                writer, "Settings", *type, reflection::ObjectView::From(config), &registry);

            if (!result)
                return { result, {} };

            return SaveDocument(writer, path);
        }

    public:
        [[nodiscard]] static ConfigSaveResult SaveDocument(const archive::JsonArchive& document,
            const std::filesystem::path& path);
    };
}