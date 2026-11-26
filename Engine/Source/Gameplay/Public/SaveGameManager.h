#pragma once

#include "Engine_API.h"
#include "SaveGameTypes.h"
#include <entt/entity/entity.hpp>
#include <filesystem>
#include <functional>
#include <string_view>
#include <vector>

namespace Engine
{
    class Scene;

    class ENGINE_API SaveGameManager
    {
    public:
        SaveGameManager();
        explicit SaveGameManager(std::filesystem::path saveDirectory);
        ~SaveGameManager() = default;

        // 싱글턴 인스턴스 조회
        static SaveGameManager& Get();

        // 저장 디렉터리 경로 설정 및 조회 (단위 테스트 및 프로젝트 경로 재정의 지원)
        void SetSaveDirectory(const std::filesystem::path& path);
        [[nodiscard]] const std::filesystem::path& GetSaveDirectory() const noexcept;

        // 슬롯 이름 유효성 검사 (경로 순회 '..', '/', '\', ':' 차단)
        [[nodiscard]] static bool IsValidSlotName(std::string_view slotName);

        // 원자적 슬롯 저장 (.tmp 임시 파일 생성 후 안전 교체)
        [[nodiscard]] bool WriteSlot(std::string_view slotName, const SaveGameData& data);

        // 슬롯 읽기
        [[nodiscard]] bool ReadSlot(std::string_view slotName, SaveGameData& outData) const;

        // 슬롯 메타데이터만 읽기
        [[nodiscard]] bool ReadSlotMetadata(std::string_view slotName, SaveSlotMetadata& outMetadata) const;

        // 슬롯 존재 여부 확인
        [[nodiscard]] bool HasSlot(std::string_view slotName) const;

        // 슬롯 삭제
        [[nodiscard]] bool DeleteSlot(std::string_view slotName);

        // 전체 저장 슬롯 메타데이터 목록 조회
        [[nodiscard]] std::vector<SaveSlotMetadata> ListSlots() const;

        // 스키마 버전 마이그레이션 콜백 등록 및 실행
        using SaveMigrationFn = std::function<bool(SaveGameData&)>;

        void RegisterMigration(uint32 fromVersion, uint32 toVersion, SaveMigrationFn fn);
        void ClearMigrations();
        [[nodiscard]] bool HasMigration(uint32 fromVersion, uint32 toVersion) const;
        [[nodiscard]] bool MigrateSaveData(SaveGameData& data,
            uint32 targetVersion = SaveGameData::CurrentSchemaVersion) const;

        // 씬 체크포인트 헬퍼: 엔티티의 TransformComponent::position / HealthComponent 자동 캡처
        [[nodiscard]] static bool CaptureCheckpointFromEntity(const Scene& scene, entt::entity playerEntity,
            std::string_view sceneUri, std::string_view checkpointId, SaveGameData& outData);

        // 씬 체크포인트 헬퍼: 저장된 세이브 데이터로 엔티티 복원
        [[nodiscard]] static bool ApplyCheckpointToEntity(Scene& scene, entt::entity playerEntity,
            const SaveGameData& data);

    private:
        struct SaveMigrationEntry
        {
            uint32 fromVersion = 0;
            uint32 toVersion = 0;
            SaveMigrationFn migrate;
        };

        [[nodiscard]] std::filesystem::path GetSlotFilePath(std::string_view slotName) const;

        std::filesystem::path m_SaveDirectory;
        std::vector<SaveMigrationEntry> m_Migrations;
    };
}
