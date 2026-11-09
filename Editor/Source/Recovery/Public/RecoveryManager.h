#pragma once

#include "Base.h"
#include <filesystem>
#include <string>

BEGIN(Editor)

struct RecoverySnapshotInfo
{
	bool IsValid = false;
	std::filesystem::path SnapshotFilePath;
	std::filesystem::path OriginalScenePath;
	std::string Timestamp;
	std::string SceneName;
	size_t EntityCount = 0;
	std::string SaveReason; // "AutoSave" or "EmergencyCrash"
};

struct RecoveryManagerDesc
{
	std::filesystem::path ProjectRoot;
};

class RecoveryManager final : public Engine::Base
{
	DECLARE_SINGLETON(RecoveryManager)
	using DESC = RecoveryManagerDesc;
private:
	RecoveryManager() = default;
	virtual ~RecoveryManager() = default;

public:
	Engine::EResult Initialize(void* arg = nullptr);
	void Free() override;

	void Update(float dt);

	// 세션 라이프사이클
	void OnStartup();
	void OnShutdown();

	// 스냅샷 저장
	bool ExecuteAutoSave();
	void ExecuteEmergencySnapshot();

	// 작업 복원 및 폐기
	bool RestoreSnapshot();
	void DiscardSnapshot();

	// ImGui 모달 다이얼로그 렌더링
	void DrawRecoveryModal();

	// 설정 접근자
	[[nodiscard]] bool IsAutoSaveEnabled() const { return m_AutoSaveEnabled; }
	void SetAutoSaveEnabled(bool enabled) { m_AutoSaveEnabled = enabled; }

	[[nodiscard]] float GetAutoSaveInterval() const { return m_AutoSaveIntervalSeconds; }
	void SetAutoSaveInterval(float seconds) { m_AutoSaveIntervalSeconds = seconds; }

	[[nodiscard]] bool HasPendingRecovery() const { return m_RecoveryInfo.IsValid; }
	[[nodiscard]] const RecoverySnapshotInfo& GetRecoveryInfo() const { return m_RecoveryInfo; }

private:
	void CheckPreviousSessionAndRecovery();
	void CreateSessionLock();
	void RemoveSessionLock();
	void MarkSessionCrashed();

	bool WriteSnapshotInternal(const std::filesystem::path& targetPath, const std::string& reason);
	void SaveRecoveryMetadata(const RecoverySnapshotInfo& info);
	bool LoadRecoveryMetadata(RecoverySnapshotInfo& outInfo);

private:
	std::filesystem::path m_ProjectRoot;
	std::filesystem::path m_RecoveryDir;
	std::filesystem::path m_SessionLockPath;
	std::filesystem::path m_MetadataPath;

	RecoverySnapshotInfo m_RecoveryInfo;

	bool m_AutoSaveEnabled = true;
	float m_AutoSaveIntervalSeconds = 300.0f; // 기본 5분
	float m_ElapsedTime = 0.0f;

	bool m_ShowRecoveryModal = false;
	bool m_ModalTriggered = false;
};

END
