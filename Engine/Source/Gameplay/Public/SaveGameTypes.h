#pragma once

#include "Engine_API.h"
#include "Types.h"
#include <archive/ArchiveInterface.h>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <cstdint>

namespace Engine
{
    struct SaveSlotMetadata
    {
        std::string slotName{};
        std::string sceneUri{};
        std::string checkpointId{};
        std::string timestampIso8601{};
        double playTimeSeconds = 0.0;
        uint32 formatVersion = 1;
        uint32 schemaVersion = 1;

        [[nodiscard]] bool operator==(const SaveSlotMetadata& other) const noexcept = default;
    };

    struct SaveGameData
    {
        static constexpr uint32 CurrentFormatVersion = 1;
        static constexpr uint32 CurrentSchemaVersion = 1;

        SaveSlotMetadata metadata{};
        vec3 playerPosition{ 0.0f };
        vec3 playerEulerRotation{ 0.0f };
        float playerHealth = 100.0f;
        float playerMaxHealth = 100.0f;
        std::unordered_map<std::string, int64_t> intFlags{};
        std::unordered_map<std::string, float> floatFlags{};
        std::unordered_map<std::string, bool> boolFlags{};
        std::unordered_map<std::string, std::string> stringFlags{};

        [[nodiscard]] bool operator==(const SaveGameData& other) const noexcept
        {
            const auto sameVec3 = [](const vec3& a, const vec3& b) noexcept {
                return a.x == b.x && a.y == b.y && a.z == b.z;
            };
            return metadata == other.metadata &&
                sameVec3(playerPosition, other.playerPosition) &&
                sameVec3(playerEulerRotation, other.playerEulerRotation) &&
                playerHealth == other.playerHealth &&
                playerMaxHealth == other.playerMaxHealth &&
                intFlags == other.intFlags &&
                floatFlags == other.floatFlags &&
                boolFlags == other.boolFlags &&
                stringFlags == other.stringFlags;
        }
    };

    // JSON 문자열 직렬화/역직렬화 (Glaze 기반 안전 인코딩/디코딩, 버전 검증, 손상 JSON 및 필드 누락 하위 호환)
    [[nodiscard]] ENGINE_API bool SerializeToJson(const SaveGameData& data, std::string& outJson);
    [[nodiscard]] ENGINE_API bool DeserializeFromJson(std::string_view json, SaveGameData& outData);
    [[nodiscard]] ENGINE_API bool DeserializeMetadataFromJson(std::string_view json, SaveSlotMetadata& outMetadata);

    // archive::IArchive 기반 직렬화/역직렬화 (GlmArchiveAdapter 연동)
    [[nodiscard]] ENGINE_API archive::ArchiveResult WriteSaveGameArchive(archive::IArchive& ar, const SaveGameData& data);
    [[nodiscard]] ENGINE_API archive::ArchiveResult ReadSaveGameArchive(const archive::IArchive& ar, SaveGameData& outData);
}
