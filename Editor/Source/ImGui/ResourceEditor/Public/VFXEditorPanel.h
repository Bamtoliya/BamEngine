#pragma once

#include "ResourceEditorInterface.h"
#include "EffectTimelineView.h"
#include "CompositeEffect.h"
#include "CompositeVFXComponent.h"
#include "PreviewViewportWidget.h"
#include "CommandHistory.h"
#include "GizmoHelper.h"
#include "CurveEditorModal.h"
#include <chrono>

BEGIN(Engine)
class Entity;
END

BEGIN(Editor)

enum class ESelectedVFXTrackType : int
{
	None = -1,
	Sprite = 0,
	Particle = 1,
	Light = 2,
	Mesh = 3,
	Sound = 4,
	Trail = 5,
	Decal = 6,
	CameraShake = 7
};

struct SelectedVFXTrack
{
	ESelectedVFXTrackType type = ESelectedVFXTrackType::None;
	int index = -1;

	bool IsValid() const { return type != ESelectedVFXTrackType::None && index >= 0; }
	void Clear() { type = ESelectedVFXTrackType::None; index = -1; }
};

class VFXEditorPanel final : public ResourceEditorInterface, public IGizmoHost
{
public:
	bool PrepareAssetPathRelocation(AssetPathRelocation& relocation) override
	{
		return PrepareTargetPathRelocation(relocation);
	}
	VFXEditorPanel();
	virtual ~VFXEditorPanel() override;

public:
	bool IsSupported(const std::filesystem::path& assetPath) const override;
	bool HasUnsavedResourceChanges() const override { return m_InspectorEdit.has_value() || m_UntrackedDirty || m_TimelineHistory.GetCurrentStateId() != m_SavedStateId; }
	void SetTargetResource(const std::filesystem::path& assetPath) override;
	bool BeginAsyncOpen(const std::filesystem::path& assetPath) override;
	bool StepAsyncOpen(float maxTimeSeconds = 0.002f) override;
	bool IsAsyncLoading() const override { return m_AsyncStage != EAsyncStage::None; }
	void DrawAsyncLoadingOverlay() override;
	void CancelAsyncOpen() override;
	EShortcutContext GetShortcutContext() const override
	{
		return EShortcutContext::CompositeTimeline;
	}
	bool CanExecuteTimelineAction(EditorUI::TimelineAction action) const;
	void ExecuteTimelineAction(EditorUI::TimelineAction action);
	virtual void Update(f32 dt) override;
	virtual void CaptureState(EditorWorkspace& workspace) const override;
	virtual void RestoreState(const EditorWorkspace& workspace) override;

private:
	void SetupPreview();
	void DrawCustomQuickAccess() override;
	void DrawCustomContent() override;
	ImVec2 GetInitialResourceWindowSize() const override { return ImVec2(1280.0f, 760.0f); }
	void DrawCustomToolbar() override;
	void ReleaseResourceDocument() override;
	bool RestoreSavedResource() override;
	const char* GetSaveActionId() const override { return "CompositeTimeline.Save"; }
	ImGuiWindowFlags GetResourceWindowFlags() const override
	{
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
		if (m_IsGizmoUsing || ImGuizmo::IsUsing())
			flags |= ImGuiWindowFlags_NoMove;
		return flags;
	}
	void OnResourceWindowVisibility(bool visible) override
	{
		if (!visible)
		{
			m_Timeline.Drag.reset();
			m_SoundStartEdit.reset();
			if (m_InspectorEdit) CommitInspectorEdit();
		}

		if (!m_Open)
			StopPreviewSounds();
	}
	float GetTimelineDuration() const;
	void DrawTrackList();
	void DrawAddTrackMenuItems(Engine::CompositeEffect& effect); // 트랙 목록(버튼·우클릭)과 타임라인 + 버튼이 함께 쓰는 추가 메뉴
	void DrawViewport();
	void DrawInspector();
	bool DrawSelectedTrackInspector(Engine::CompositeEffect* effect); // 이번 프레임에 값이 바뀌면 true
	void CommitInspectorEdit();
	std::function<void()> CaptureCurveHistory();
	void DrawTransportBar();
	void DrawTimeline();
	void SelectTrack(SelectedVFXTrack selection);
	void DeleteSelectedTrack();
	void ApplyTimelineEdit(const EditorUI::TimelineEdit& edit);
	void StopPreviewSounds();

	// 파일 관리
	void NewEffect();
	void OpenEffect();
	void SaveEffect();
	void SaveEffectAs();
	void SaveEffectTo(const std::filesystem::path& path);
	void RestartSimulation();
	void InvalidateTimelinePreview();
	void OffsetTrailRuntimePoints(size_t trackIndex, const Engine::vec3& localDeltaA, const Engine::vec3& localDeltaB);
	void MarkSaved(); // 현재 문서 상태를 저장 기준점으로 기록한다 (저장 성공·로드 시)
	void ScrubTime(f32 targetTime);

	void ApplyBackgroundPreset();
	void ApplyGroundBackdrop();

	void DrawGizmo();
	void DrawVisualHelpers(const Engine::mat4& viewProj, const ImVec2& imageScreenPos, const ImVec2& imageSize);
	void DrawViewportHUD(const ImVec2& viewportPos, const ImVec2& viewportSize);
	void StepFrame(int step);

	void ToggleAirMode();
private:
	Engine::ResourceHandle<Engine::CompositeEffect> m_EffectHandle;
	bool m_OwnsTemporaryEffect = false;
	bool m_UntrackedDirty = false; // 이력에 남지 않는 변경: 반복 토글, 트랙 목록 활성 체크, 새 이펙트
	uint64 m_SavedStateId = 0;     // 저장·로드 시점의 m_TimelineHistory 상태 ID
	std::string m_StatusMessage;
	char m_TrackSearch[64] = {};

	// 선택된 트랙
	SelectedVFXTrack m_SelectedTrack;
	std::optional<EditorUI::TimelineEdit> m_SoundStartEdit;

	// 재생 제어 상태
	bool m_IsPlaying = true;
	bool m_IsLooping = false;
	f32 m_PlayRate = 1.0f;
	f32 m_CurrentTime = 0.0f;

	EditorUI::EffectTimelineState m_Timeline;
	std::optional<float> m_TimelineSeek;
	bool m_FixedPreview = false;

	// 3D 프리뷰 뷰포트 & 프리뷰 엔티티
	PreviewViewportWidget m_PreviewWidget;
	Engine::Entity* m_PreviewEntity = nullptr;
	Engine::Entity* m_PreviewGroundEntity = nullptr;

	// ── 프리뷰 제어 상태 ──
	enum class EVFXBackgroundMode : uint8 { StudioGray = 0, Dark = 1, Bright = 2 };
	EVFXBackgroundMode m_BackgroundMode = EVFXBackgroundMode::StudioGray;
	bool m_PreviewMotion = false;
	bool m_ShowGroundPlane = true;
	bool m_ShowCheckerBackdrop = true;
	bool m_BackdropDirty = true;
	bool m_AirMode = false;
	bool m_SavedGroundPlane = true;
	bool m_SavedGrid = true;

	// ── Phase 5 환경 장애물 프리셋 ──
	enum class EVFXObstaclePreset : uint8 { FloorOnly = 0, CornerWall = 1, Obstacles = 2, DummyCharacter = 3 };
	EVFXObstaclePreset m_ObstaclePreset = EVFXObstaclePreset::FloorOnly;
	std::vector<Engine::Entity*> m_ObstacleEntities;
	// ── Phase 5 환경 외력 시뮬레이터 ──
	Engine::vec3 m_SimWind{ 0.0f };
	float m_SimGravityMultiplier = 1.0f;

	// 레이아웃 분할
	float m_LeftPaneWidth = 280.0f;
	float m_RightPaneWidth = 360.0f;
	float m_TimelineHeight = 180.0f;
	Editor::CurveEditorModal m_CurveModal;

	CommandHistory m_TimelineHistory;

	// ── 3D 기즈모 상태 ──
	struct VFXGizmoSnapshot
	{
		SelectedVFXTrack track;
		Engine::vec3 position{ 0.0f };
		Engine::vec3 rotation{ 0.0f };
		Engine::vec3 scale{ 1.0f };
		Engine::vec2 decalSize{ 1.0f };
		Engine::vec3 trailOffsetA{ 0.0f };
		Engine::vec3 trailOffsetB{ 0.0f };
		bool valid = false;
	};
	VFXGizmoSnapshot m_GizmoSnapshot;
	struct VFXInspectorEdit
	{
		Engine::CompositeEffect* effect = nullptr;
		SelectedVFXTrack track;
		std::any before;
	};
	std::optional<VFXInspectorEdit> m_InspectorEdit;
	bool m_IsGizmoUsing = false;

	// 비동기 열기 상태
	enum class EAsyncStage
	{
		None,
		WaitingBackground,
		LoadingResource,
		SettingUpPreview,
		Failed
	};
	EAsyncStage m_AsyncStage = EAsyncStage::None;
	float m_AsyncProgress = 0.0f;
	std::filesystem::path m_AsyncTargetPath;
	Engine::ResourceHandle<Engine::CompositeEffect> m_AsyncPendingEffect;
	std::chrono::steady_clock::time_point m_AsyncStartTime{};
	std::chrono::steady_clock::time_point m_AsyncLoadStart{};
	double m_AsyncLoadDurationMs = 0.0;
};

END
