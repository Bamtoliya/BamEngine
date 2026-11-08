#pragma once

#include "ResourceEditorInterface.h"
#include "CompositeEffect.h"
#include "Components/Render/CompositeVFXComponent.h"
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
	Sound = 4
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
	void Draw() override;
	virtual void Update(f32 dt) override;
	virtual void CaptureState(EditorWorkspace& workspace) const override;
	virtual void RestoreState(const EditorWorkspace& workspace) override;

private:
	void SetupPreview();
	void DrawToolbar();
	void DrawTrackList();
	void DrawViewport();
	void DrawInspector();
	void DrawTimeline(float availWidth, float height);

	// 파일 관리
	void NewEffect();
	void OpenEffect();
	void SaveEffect();
	void SaveEffectAs();
	void RestartSimulation();
	void ScrubTime(f32 targetTime);

private:
	Engine::ResourceHandle<Engine::CompositeEffect> m_EffectHandle;
	bool m_IsDirty = false;
	std::string m_StatusMessage;

	// 선택된 트랙
	SelectedVFXTrack m_SelectedTrack;

	// 재생 제어 상태
	bool m_IsPlaying = true;
	bool m_IsLooping = false;
	f32 m_PlayRate = 1.0f;
	f32 m_CurrentTime = 0.0f;

	// 3D 프리뷰 뷰포트 & 프리뷰 엔티티
	PreviewViewportWidget m_PreviewWidget;
	Engine::Entity* m_PreviewEntity = nullptr;

	// 레이아웃 분할
	float m_LeftPaneWidth = 280.0f;
	float m_RightPaneWidth = 360.0f;
	float m_TimelineHeight = 180.0f;
	float m_TimelineHeaderWidth = 200.0f;

	// 타임라인 스크러빙
	bool m_IsScrubbing = false;
};

END
