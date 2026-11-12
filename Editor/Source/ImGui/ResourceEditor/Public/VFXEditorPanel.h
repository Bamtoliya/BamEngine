#pragma once

#include "ResourceEditorInterface.h"
#include "EffectTimelineView.h"
#include "CompositeEffect.h"
#include "CompositeVFXComponent.h"
#include "PreviewViewportWidget.h"
#include "CommandHistory.h"

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

class VFXEditorPanel final : public ResourceEditorInterface
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

private:
	Engine::ResourceHandle<Engine::CompositeEffect> m_EffectHandle;
	bool m_IsDirty = false;
	std::string m_StatusMessage;

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

	// 레이아웃 분할
	float m_LeftPaneWidth = 280.0f;
	float m_RightPaneWidth = 360.0f;
	float m_TimelineHeight = 180.0f;

	CommandHistory m_TimelineHistory;
};

END
