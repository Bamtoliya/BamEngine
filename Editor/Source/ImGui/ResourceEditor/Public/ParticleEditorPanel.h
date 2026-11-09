#pragma once

#include "ResourceEditorInterface.h"
#include "EffectTimelineView.h"
#include "ParticleEffect.h"
#include "ParticleEffectComponent.h"
#include "PreviewViewportWidget.h"
#include "CommandHistory.h"

BEGIN(Engine)
class Entity;
END

BEGIN(Editor)

class ParticleEditorPanel final : public ResourceEditorInterface
{
public:
	ParticleEditorPanel();
	virtual ~ParticleEditorPanel() override;

public:
	bool IsSupported(const std::filesystem::path& assetPath) const override;
	bool HasUnsavedResourceChanges() const override { return m_IsDirty; }
	void SetTargetResource(const std::filesystem::path& assetPath) override;
	EShortcutContext GetShortcutContext() const override { return EShortcutContext::ParticleTimeline; }
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
	void OnResourceWindowVisibility(bool visible) override { if (!visible) m_Timeline.Drag.reset(); }
	float GetTimelineDuration() const;
	void DrawEmitterList();
	void DrawViewport();
	void DrawEmitterInspector();
	void DrawTimeline();
	void ScrubTime(f32 targetTime);
	void DeleteSelectedEmitter();

	// 파일 관리
	void NewEffect();
	void OpenEffect();
	void SaveEffect();
	void SaveEffectAs();
	void RestartSimulation();
	void InvalidateTimelinePreview();

	// 실행 취소 / 다시 실행 (Undo / Redo)
	void Undo();
	void Redo();
	void PushSnapshot(const std::string& label);
	std::vector<Engine::ParticleEmitterData> CaptureSnapshot() const;
	void RestoreSnapshot(const std::vector<Engine::ParticleEmitterData>& snapshot);

private:
	Engine::ResourceHandle<Engine::ParticleEffect> m_EffectHandle;
	bool m_IsDirty = false;
	string m_StatusMessage;
	int m_SelectedEmitterIndex = 0;

	// 재생 제어 상태
	bool m_IsPlaying = true;
	f32 m_PlayRate = 1.0f;
	f32 m_SimulatedTime = 0.0f;

	EditorUI::EffectTimelineState m_Timeline;
	std::optional<float> m_TimelineSeek;
	bool m_FixedPreview = false;

	// 3D 프리뷰 뷰포트 & 프리뷰 엔티티
	PreviewViewportWidget m_PreviewWidget;
	Engine::Entity* m_PreviewEntity = nullptr;

	// 레이아웃 분할 너비
	float m_LeftPaneWidth = 260.0f;
	float m_RightPaneWidth = 380.0f;
	float m_TimelineHeight = 200.0f;
	bool m_IsLooping = false;

	// 편집 이력 관리
	CommandHistory m_History;
	std::vector<Engine::ParticleEmitterData> m_PreEditSnapshot;
	bool m_HasPreEditSnapshot = false;
};

END
