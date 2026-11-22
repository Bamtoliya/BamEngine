#include "SaveGameManager.h"
#include "PlatformPath.h"
#include "Scene.h"
#include "TransformComponent.h"
#include "HealthComponent.h"
#include "Logger.h"
#include "Adapters/GlmArchiveAdapter.h"
#include <archive/JsonArchive.h>
#include <glaze/glaze.hpp>

#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <system_error>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace Engine
{
    namespace SaveGameDetail
    {
        struct SaveSlotMetadataDto
        {
            std::string slotName{};
            std::string sceneUri{};
            std::string checkpointId{};
            std::string timestampIso8601{};
            double playTimeSeconds = 0.0;
            uint32 formatVersion = 1;
        };

        struct SaveGameDataDto
        {
            SaveSlotMetadataDto metadata{};
            std::array<float, 3> playerPosition{ 0.0f, 0.0f, 0.0f };
            std::array<float, 3> playerEulerRotation{ 0.0f, 0.0f, 0.0f };
            float playerHealth = 100.0f;
            float playerMaxHealth = 100.0f;
            std::unordered_map<std::string, int64_t> intFlags{};
            std::unordered_map<std::string, float> floatFlags{};
            std::unordered_map<std::string, bool> boolFlags{};
            std::unordered_map<std::string, std::string> stringFlags{};
        };

        struct MetadataWrapperDto
        {
            SaveSlotMetadataDto metadata{};
        };
    }

    namespace
    {
        using SaveGameDetail::SaveSlotMetadataDto;
        using SaveGameDetail::SaveGameDataDto;
        using SaveGameDetail::MetadataWrapperDto;

        std::string GetCurrentTimestampIso8601()
        {
            const auto now = std::chrono::system_clock::now();
            const auto timeT = std::chrono::system_clock::to_time_t(now);
            std::tm gmTm{};
#if defined(_WIN32)
            gmtime_s(&gmTm, &timeT);
#else
            gmtime_r(&timeT, &gmTm);
#endif
            char buffer[32];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &gmTm);
            return std::string(buffer);
        }
    }

    bool SerializeToJson(const SaveGameData& data, std::string& outJson)
    {
        SaveGameDataDto dto;
        dto.metadata.slotName = data.metadata.slotName;
        dto.metadata.sceneUri = data.metadata.sceneUri;
        dto.metadata.checkpointId = data.metadata.checkpointId;
        dto.metadata.timestampIso8601 = data.metadata.timestampIso8601;
        dto.metadata.playTimeSeconds = data.metadata.playTimeSeconds;
        dto.metadata.formatVersion = (data.metadata.formatVersion == 0) ?
            SaveGameData::CurrentFormatVersion : data.metadata.formatVersion;

        dto.playerPosition = { data.playerPosition.x, data.playerPosition.y, data.playerPosition.z };
        dto.playerEulerRotation = { data.playerEulerRotation.x, data.playerEulerRotation.y, data.playerEulerRotation.z };
        dto.playerHealth = data.playerHealth;
        dto.playerMaxHealth = data.playerMaxHealth;
        dto.intFlags = data.intFlags;
        dto.floatFlags = data.floatFlags;
        dto.boolFlags = data.boolFlags;
        dto.stringFlags = data.stringFlags;

        const auto ec = glz::write<glz::opts{ .prettify = true, .indentation_width = 4 }>(dto, outJson);
        return !ec;
    }

    bool DeserializeFromJson(std::string_view json, SaveGameData& outData)
    {
        if (json.empty() || glz::validate_json(json))
        {
            return false;
        }

        SaveGameDataDto dto;
        dto.metadata.formatVersion = SaveGameData::CurrentFormatVersion;
        dto.playerHealth = 100.0f;
        dto.playerMaxHealth = 100.0f;

        const auto ec = glz::read_json(dto, json);
        if (ec)
        {
            return false;
        }

        if (dto.metadata.formatVersion == 0 || dto.metadata.formatVersion > SaveGameData::CurrentFormatVersion)
        {
            return false;
        }

        outData.metadata.slotName = std::move(dto.metadata.slotName);
        outData.metadata.sceneUri = std::move(dto.metadata.sceneUri);
        outData.metadata.checkpointId = std::move(dto.metadata.checkpointId);
        outData.metadata.timestampIso8601 = std::move(dto.metadata.timestampIso8601);
        outData.metadata.playTimeSeconds = dto.metadata.playTimeSeconds;
        outData.metadata.formatVersion = dto.metadata.formatVersion;

        outData.playerPosition = vec3(dto.playerPosition[0], dto.playerPosition[1], dto.playerPosition[2]);
        outData.playerEulerRotation = vec3(dto.playerEulerRotation[0], dto.playerEulerRotation[1], dto.playerEulerRotation[2]);
        outData.playerHealth = dto.playerHealth;
        outData.playerMaxHealth = dto.playerMaxHealth;
        outData.intFlags = std::move(dto.intFlags);
        outData.floatFlags = std::move(dto.floatFlags);
        outData.boolFlags = std::move(dto.boolFlags);
        outData.stringFlags = std::move(dto.stringFlags);
        return true;
    }

    bool DeserializeMetadataFromJson(std::string_view json, SaveSlotMetadata& outMetadata)
    {
        if (json.empty() || glz::validate_json(json))
        {
            return false;
        }

        MetadataWrapperDto wrapper;
        wrapper.metadata.formatVersion = SaveGameData::CurrentFormatVersion;

        const auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(wrapper, json);
        if (ec || wrapper.metadata.formatVersion == 0 || wrapper.metadata.formatVersion > SaveGameData::CurrentFormatVersion)
        {
            return false;
        }

        outMetadata.slotName = std::move(wrapper.metadata.slotName);
        outMetadata.sceneUri = std::move(wrapper.metadata.sceneUri);
        outMetadata.checkpointId = std::move(wrapper.metadata.checkpointId);
        outMetadata.timestampIso8601 = std::move(wrapper.metadata.timestampIso8601);
        outMetadata.playTimeSeconds = wrapper.metadata.playTimeSeconds;
        outMetadata.formatVersion = wrapper.metadata.formatVersion;
        return true;
    }

    archive::ArchiveResult WriteSaveGameArchive(archive::IArchive& ar, const SaveGameData& data)
    {
        auto metaResult = ar.WriteObject("metadata", data.metadata,
            [](archive::IArchive& metaAr, const SaveSlotMetadata& meta) -> archive::ArchiveResult {
                if (auto r = metaAr.Write("slotName", meta.slotName); !r) return r;
                if (auto r = metaAr.Write("sceneUri", meta.sceneUri); !r) return r;
                if (auto r = metaAr.Write("checkpointId", meta.checkpointId); !r) return r;
                if (auto r = metaAr.Write("timestampIso8601", meta.timestampIso8601); !r) return r;
                if (auto r = metaAr.Write("playTimeSeconds", meta.playTimeSeconds); !r) return r;
                return metaAr.Write("formatVersion", meta.formatVersion);
            });
        if (!metaResult) return metaResult;

        if (auto r = archive_glm::WriteVec3(ar, "playerPosition", data.playerPosition); !r) return r;
        if (auto r = archive_glm::WriteVec3(ar, "playerEulerRotation", data.playerEulerRotation); !r) return r;
        if (auto r = ar.Write("playerHealth", data.playerHealth); !r) return r;
        if (auto r = ar.Write("playerMaxHealth", data.playerMaxHealth); !r) return r;

        if (auto r = ar.WriteMap("intFlags", data.intFlags); !r) return r;
        if (auto r = ar.WriteMap("floatFlags", data.floatFlags); !r) return r;
        if (auto r = ar.WriteMap("boolFlags", data.boolFlags); !r) return r;
        if (auto r = ar.WriteMap("stringFlags", data.stringFlags); !r) return r;

        return {};
    }

    archive::ArchiveResult ReadSaveGameArchive(const archive::IArchive& ar, SaveGameData& outData)
    {
        bool hasMeta = false;
        if (auto r = ar.HasField("metadata", hasMeta); !r) return r;
        if (hasMeta)
        {
            auto metaResult = ar.ReadObject("metadata", outData.metadata,
                [](archive::IArchive& metaAr, SaveSlotMetadata& meta) -> archive::ArchiveResult {
                    bool hasField = false;
                    if ((void)metaAr.HasField("slotName", hasField); hasField) (void)metaAr.Read("slotName", meta.slotName);
                    if ((void)metaAr.HasField("sceneUri", hasField); hasField) (void)metaAr.Read("sceneUri", meta.sceneUri);
                    if ((void)metaAr.HasField("checkpointId", hasField); hasField) (void)metaAr.Read("checkpointId", meta.checkpointId);
                    if ((void)metaAr.HasField("timestampIso8601", hasField); hasField) (void)metaAr.Read("timestampIso8601", meta.timestampIso8601);
                    if ((void)metaAr.HasField("playTimeSeconds", hasField); hasField) (void)metaAr.Read("playTimeSeconds", meta.playTimeSeconds);
                    if ((void)metaAr.HasField("formatVersion", hasField); hasField) (void)metaAr.Read("formatVersion", meta.formatVersion);
                    return {};
                });
            if (!metaResult) return metaResult;
        }

        if (outData.metadata.formatVersion == 0 || outData.metadata.formatVersion > SaveGameData::CurrentFormatVersion)
        {
            return { archive::ArchiveErrorCode::InvalidDocument, "formatVersion" };
        }

        bool hasField = false;
        if ((void)ar.HasField("playerPosition", hasField); hasField) (void)archive_glm::ReadVec3(ar, "playerPosition", outData.playerPosition);
        if ((void)ar.HasField("playerEulerRotation", hasField); hasField) (void)archive_glm::ReadVec3(ar, "playerEulerRotation", outData.playerEulerRotation);
        if ((void)ar.HasField("playerHealth", hasField); hasField) (void)ar.Read("playerHealth", outData.playerHealth);
        if ((void)ar.HasField("playerMaxHealth", hasField); hasField) (void)ar.Read("playerMaxHealth", outData.playerMaxHealth);

        if ((void)ar.HasField("intFlags", hasField); hasField) (void)ar.ReadMap("intFlags", outData.intFlags);
        if ((void)ar.HasField("floatFlags", hasField); hasField) (void)ar.ReadMap("floatFlags", outData.floatFlags);
        if ((void)ar.HasField("boolFlags", hasField); hasField) (void)ar.ReadMap("boolFlags", outData.boolFlags);
        if ((void)ar.HasField("stringFlags", hasField); hasField) (void)ar.ReadMap("stringFlags", outData.stringFlags);

        return {};
    }

    SaveGameManager::SaveGameManager()
    {
        const auto& projectRoot = PlatformPath::GetProjectRoot();
        if (!projectRoot.empty())
        {
            m_SaveDirectory = projectRoot / "Saved" / "SaveGames";
        }
        else
        {
            m_SaveDirectory = std::filesystem::current_path() / "Saved" / "SaveGames";
        }
    }

    SaveGameManager::SaveGameManager(std::filesystem::path saveDirectory)
        : m_SaveDirectory(std::move(saveDirectory))
    {
    }

    SaveGameManager& SaveGameManager::Get()
    {
        static SaveGameManager s_Instance;
        return s_Instance;
    }

    void SaveGameManager::SetSaveDirectory(const std::filesystem::path& path)
    {
        m_SaveDirectory = path;
    }

    const std::filesystem::path& SaveGameManager::GetSaveDirectory() const noexcept
    {
        return m_SaveDirectory;
    }

    bool SaveGameManager::IsValidSlotName(std::string_view slotName)
    {
        if (slotName.empty() || slotName.length() > 128)
        {
            return false;
        }
        if (slotName == "." || slotName == "..")
        {
            return false;
        }
        if (slotName.find("..") != std::string_view::npos)
        {
            return false;
        }
        for (char c : slotName)
        {
            if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
                c == '"' || c == '<' || c == '>' || c == '|' ||
                static_cast<unsigned char>(c) < 32)
            {
                return false;
            }
        }
        return true;
    }

    std::filesystem::path SaveGameManager::GetSlotFilePath(std::string_view slotName) const
    {
        return m_SaveDirectory / (std::string(slotName) + ".json");
    }

    bool SaveGameManager::WriteSlot(std::string_view slotName, const SaveGameData& data)
    {
        if (!IsValidSlotName(slotName))
        {
            BAM_LOG(Error, "Gameplay", "SaveGame", "Invalid save slot name: {}", slotName);
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(m_SaveDirectory, ec);
        if (ec)
        {
            BAM_LOG(Error, "Gameplay", "SaveGame", "Failed to create save directory {}: {}",
                m_SaveDirectory.string(), ec.message());
            return false;
        }

        const auto targetPath = GetSlotFilePath(slotName);
        const auto tempPath = m_SaveDirectory / (std::string(slotName) + ".tmp");

        SaveGameData saveCopy = data;
        saveCopy.metadata.slotName = std::string(slotName);
        if (saveCopy.metadata.timestampIso8601.empty())
        {
            saveCopy.metadata.timestampIso8601 = GetCurrentTimestampIso8601();
        }
        if (saveCopy.metadata.formatVersion == 0)
        {
            saveCopy.metadata.formatVersion = SaveGameData::CurrentFormatVersion;
        }

        std::string jsonStr;
        if (!SerializeToJson(saveCopy, jsonStr))
        {
            BAM_LOG(Error, "Gameplay", "SaveGame", "Failed to serialize save game data for slot {}", slotName);
            return false;
        }

        {
            std::ofstream file(tempPath, std::ios::out | std::ios::trunc | std::ios::binary);
            if (!file.is_open())
            {
                BAM_LOG(Error, "Gameplay", "SaveGame", "Failed to open temporary file {} for writing", tempPath.string());
                return false;
            }
            file.write(jsonStr.data(), static_cast<std::streamsize>(jsonStr.size()));
            if (!file.good())
            {
                file.close();
                std::filesystem::remove(tempPath, ec);
                BAM_LOG(Error, "Gameplay", "SaveGame", "Failed writing data to temporary file {}", tempPath.string());
                return false;
            }
            file.flush();
        }

#if defined(_WIN32)
        if (!::MoveFileExW(tempPath.wstring().c_str(), targetPath.wstring().c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH))
        {
            std::filesystem::remove(targetPath, ec);
            std::filesystem::rename(tempPath, targetPath, ec);
            if (ec)
            {
                std::filesystem::remove(tempPath, ec);
                BAM_LOG(Error, "Gameplay", "SaveGame", "Failed to atomically replace slot file {}: {}",
                    targetPath.string(), ec.message());
                return false;
            }
        }
#else
        std::filesystem::rename(tempPath, targetPath, ec);
        if (ec)
        {
            std::filesystem::remove(targetPath, ec);
            std::filesystem::rename(tempPath, targetPath, ec);
            if (ec)
            {
                std::filesystem::remove(tempPath, ec);
                BAM_LOG(Error, "Gameplay", "SaveGame", "Failed to replace slot file {}: {}",
                    targetPath.string(), ec.message());
                return false;
            }
        }
#endif

        BAM_LOG(Info, "Gameplay", "SaveGame", "Successfully saved game slot '{}' to {}", slotName, targetPath.string());
        return true;
    }

    bool SaveGameManager::ReadSlot(std::string_view slotName, SaveGameData& outData) const
    {
        if (!IsValidSlotName(slotName))
        {
            return false;
        }

        const auto targetPath = GetSlotFilePath(slotName);
        std::error_code ec;
        if (!std::filesystem::is_regular_file(targetPath, ec))
        {
            return false;
        }

        std::ifstream file(targetPath, std::ios::in | std::ios::binary);
        if (!file.is_open())
        {
            return false;
        }

        std::string jsonStr((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        return DeserializeFromJson(jsonStr, outData);
    }

    bool SaveGameManager::ReadSlotMetadata(std::string_view slotName, SaveSlotMetadata& outMetadata) const
    {
        if (!IsValidSlotName(slotName))
        {
            return false;
        }

        const auto targetPath = GetSlotFilePath(slotName);
        std::error_code ec;
        if (!std::filesystem::is_regular_file(targetPath, ec))
        {
            return false;
        }

        std::ifstream file(targetPath, std::ios::in | std::ios::binary);
        if (!file.is_open())
        {
            return false;
        }

        std::string jsonStr((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        return DeserializeMetadataFromJson(jsonStr, outMetadata);
    }

    bool SaveGameManager::HasSlot(std::string_view slotName) const
    {
        if (!IsValidSlotName(slotName))
        {
            return false;
        }

        std::error_code ec;
        return std::filesystem::is_regular_file(GetSlotFilePath(slotName), ec);
    }

    bool SaveGameManager::DeleteSlot(std::string_view slotName)
    {
        if (!IsValidSlotName(slotName))
        {
            return false;
        }

        std::error_code ec;
        const auto targetPath = GetSlotFilePath(slotName);
        return std::filesystem::remove(targetPath, ec);
    }

    std::vector<SaveSlotMetadata> SaveGameManager::ListSlots() const
    {
        std::vector<SaveSlotMetadata> result;
        std::error_code ec;
        if (!std::filesystem::exists(m_SaveDirectory, ec) || !std::filesystem::is_directory(m_SaveDirectory, ec))
        {
            return result;
        }

        for (const auto& entry : std::filesystem::directory_iterator(m_SaveDirectory, ec))
        {
            if (!entry.is_regular_file(ec))
            {
                continue;
            }

            const auto path = entry.path();
            if (path.extension() != ".json")
            {
                continue;
            }

            const auto slotName = path.stem().string();
            if (!IsValidSlotName(slotName))
            {
                continue;
            }

            SaveSlotMetadata metadata;
            if (ReadSlotMetadata(slotName, metadata))
            {
                result.push_back(std::move(metadata));
            }
        }

        std::sort(result.begin(), result.end(), [](const SaveSlotMetadata& a, const SaveSlotMetadata& b) {
            return a.slotName < b.slotName;
        });

        return result;
    }

    bool SaveGameManager::CaptureCheckpointFromEntity(const Scene& scene, entt::entity playerEntity,
        std::string_view sceneUri, std::string_view checkpointId, SaveGameData& outData)
    {
        const auto& registry = scene.GetRegistry();
        if (!registry.valid(playerEntity))
        {
            return false;
        }

        outData.metadata.sceneUri = std::string(sceneUri);
        outData.metadata.checkpointId = std::string(checkpointId);
        outData.metadata.timestampIso8601 = GetCurrentTimestampIso8601();
        outData.metadata.formatVersion = SaveGameData::CurrentFormatVersion;

        if (const auto* transform = registry.try_get<TransformComponent>(playerEntity))
        {
            outData.playerPosition = transform->position;
            outData.playerEulerRotation = transform->eulerRotation;
        }

        if (const auto* health = registry.try_get<HealthComponent>(playerEntity))
        {
            outData.playerHealth = health->currentHealth;
            outData.playerMaxHealth = health->maxHealth;
        }

        return true;
    }

    bool SaveGameManager::ApplyCheckpointToEntity(Scene& scene, entt::entity playerEntity,
        const SaveGameData& data)
    {
        auto& registry = scene.GetRegistry();
        if (!registry.valid(playerEntity))
        {
            return false;
        }

        if (auto* transform = registry.try_get<TransformComponent>(playerEntity))
        {
            (void)transform->SetLocalPosition(data.playerPosition);
            (void)transform->SetEulerRotation(data.playerEulerRotation);
        }

        if (auto* health = registry.try_get<HealthComponent>(playerEntity))
        {
            health->maxHealth = data.playerMaxHealth;
            health->currentHealth = data.playerHealth;
            health->isDead = (health->currentHealth <= 0.0f);
        }

        return true;
    }
}
