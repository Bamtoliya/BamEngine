#pragma once

#include "ResourceEditorInterface.h"
#include "EffectTimelineView.h"
#include "CompositeEffect.h"
#include "CompositeVFXComponent.h"
#include "PreviewViewportWidget.h"
#include "CommandHistory.h"
#include "GizmoHelper.h"

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
	VFXEditorPanel();
	virtual ~VFXEditorPanel() override;

public:
	bool IsSupported(const std::filesystem::path& assetPath) const override;
	bool HasUnsavedResourceChanges() const override { return m_IsDirty; }
	void SetTargetResource(const std::filesystem::path& assetPath) override;
	EShortcutContext GetShortcutContext() const override { return EShortcutContext::CompositeTimeline; }
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
	ImGuiWindowFlags GetResourceWindowFlags() const override { return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse; }
	void OnResourceWindowVisibility(bool visible) override
	{
		if (!visible)
		{
			m_Timeline.Drag.reset();
			m_SoundStartEdit.reset();
		}

		if (!m_Open)
			StopPreviewSounds();
	}
	float GetTimelineDuration() const;
	void DrawTrackList();
	void DrawViewport();
	void DrawInspector();
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
	void RestartSimulation();
	void InvalidateTimelinePreview();
	void ScrubTime(f32 targetTime);

	void ApplyBackgroundPreset();

	void DrawGizmo();
	void DrawVisualHelpers(const Engine::mat4& viewProj, const ImVec2& imageScreenPos, const ImVec2& imageSize);
	// IGizmoHost 구현
	void SetGizmoOperation(ImGuizmo::OPERATION op) override { m_GizmoOperation = op; }
	ImGuizmo::OPERATION GetGizmoOperation() const override { return m_GizmoOperation; }
	void ToggleGizmoMode() override { m_GizmoMode = (m_GizmoMode == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL; }
	ImGuizmo::MODE GetGizmoMode() const override { return m_GizmoMode; }
	void ToggleGizmoSnap() override { m_GizmoUseSnap = !m_GizmoUseSnap; }
	bool GetGizmoUseSnap() const override { return m_GizmoUseSnap; }
private:
	Engine::ResourceHandle<Engine::CompositeEffect> m_EffectHandle;
	bool m_IsDirty = false;
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
	ImGuizmo::OPERATION m_GizmoOperation = ImGuizmo::TRANSLATE;
	ImGuizmo::MODE m_GizmoMode = ImGuizmo::LOCAL;
	bool m_GizmoUseSnap = false;
	VFXGizmoSnapshot m_GizmoSnapshot;
	bool m_IsGizmoUsing = false;
};

END
