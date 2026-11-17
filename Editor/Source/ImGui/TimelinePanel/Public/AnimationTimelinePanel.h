#pragma once

#include "ResourceEditorInterface.h"
#include "ShortcutTypes.h"
#include "Model.h"
#include "ActionTimelineAsset.h"
#include <string>
#include <vector>
#include <filesystem>
#include "PreviewViewportWidget.h"

BEGIN(Engine)
class Scene;
class Animation;
class Entity;
END

BEGIN(Editor)

// 트랙 요소 선택 구분
enum class ETimelineSelectionType : uint8
{
	None,
	NotifyMarker,
	CameraShot
};

struct TimelineSelection
{
	ETimelineSelectionType Type = ETimelineSelectionType::None;
	int TrackIndex = -1;
	int ItemIndex = -1;
};

struct TimelineSocketTransform
{
	vec3 position = vec3(0.0f);
	quat rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
};

class AnimationTimelinePanel final : public ResourceEditorInterface
{
public:
	AnimationTimelinePanel();
	virtual ~AnimationTimelinePanel() override;
	virtual void Update(f32 dt) override;
	virtual EShortcutContext GetShortcutContext() const override { return EShortcutContext::AnimationTimeline; }

	EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::GraphAnimation; }
	bool HasUnsavedResourceChanges() const override { return m_IsDirty; }
	virtual void SaveResource() override { SaveTimelineFile(); }
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override
	{
		const std::string ext = assetPath.extension().string();
		const std::string name = WStrToStr(assetPath.filename().wstring());
		return (ext == ".bamtimeline" || ext == ".actiontimeline" || name.ends_with(".bamtimeline.json") || name.ends_with(".actiontimeline.json"));
	}
	virtual void SetTargetResource(const std::filesystem::path& assetPath) override
	{
		ResourceEditorInterface::SetTargetResource(assetPath);
		LoadFromFile(assetPath.string());
	}
	void SetTargetModel(const Engine::ResourceHandle<Engine::Model>& model);
	void SyncFromSelectedEntity();
	bool SaveToFile(const std::string& filePath);
	bool LoadFromFile(const std::string& filePath);
	virtual void CaptureState(EditorWorkspace& workspace) const override;
	virtual void RestoreState(const EditorWorkspace& workspace) override;

public:
	static AnimationTimelinePanel* GetActivePanel() { return s_ActivePanel; }
	void TogglePlayPause() { m_IsPlaying = !m_IsPlaying; }
	void StepFrame(int step);
	void GoToFirstFrame() { m_CurrentTime = 0.0f; }
	void GoToLastFrame() { m_CurrentTime = m_Duration; m_IsPlaying = false; }
	void ToggleLoop() { m_IsLooping = !m_IsLooping; }
	void ResetZoom() { m_TimelineZoom = 1.0f; m_ViewStartTime = 0.0f; }
	bool CanDeleteSelected() const;
	void DeleteSelected();

private:
	void DrawCustomQuickAccess() override;
	void DrawCustomContent() override;
	ImVec2 GetInitialResourceWindowSize() const override { return ImVec2(1060.0f, 660.0f); }
	void DrawCustomToolbar() override;
	ImGuiWindowFlags GetResourceWindowFlags() const override;
	void OpenTimelineFile();
	void SaveTimelineFile();
	void DrawTimelineTrackArea();
	void DrawInspector();
	void DrawNotifyInspector(Engine::AnimNotifyData& notify);
	void DrawCameraShotInspector(Engine::TimelineCameraTrack& camShot);

	void DrawTrackGridLines(struct ImDrawList* drawList, const struct ImVec2& trackMin, const struct ImVec2& trackMax, float timelineWidth);
	void DrawAnimPoseTrack(struct ImDrawList* drawList, const struct ImVec2& trackMin, const struct ImVec2& trackMax, float timelineWidth);
	void DrawNotifyTrackRow(struct ImDrawList* drawList, int trackIdx, Engine::TimelineNotifyTrack& track, const struct ImVec2& trackMin, const struct ImVec2& trackMax, float timelineWidth);
	void DrawCameraTrackRow(struct ImDrawList* drawList, int camIdx, Engine::TimelineCameraTrack& camTrack, const struct ImVec2& trackMin, const struct ImVec2& trackMax, float timelineWidth);

	float TimeToScreenX(float time, float timelineMinX, float timelineWidth) const;
	float ScreenXToTime(float screenX, float timelineMinX, float timelineWidth) const;

	void TriggerNotify(const Engine::AnimNotifyData& notify);
	vec3 GetSocketWorldPosition(const std::string& boneName, const vec3& offset) const;
	TimelineSocketTransform GetSocketWorldTransform(const std::string& boneName, const vec3& offsetPos, const vec3& offsetRot) const;
	void ClearPreviewVFX();

	// 신규 뷰포트 & Transport 바 & 본 계층 트리
	void DrawTransportBar();
	void DrawBoneHierarchyTree();
	void HandleModelDragDrop();
	void SetupPreviewModel();

private:
	// 타임라인 재생 상태
	bool m_IsPlaying = false;
	bool m_IsLooping = true;
	bool m_IsScrubbing = false;
	f32 m_CurrentTime = 0.0f;
	f32 m_Duration = 2.0f;
	f32 m_FPS = 30.0f;
	f32 m_PlaybackSpeed = 1.0f;
	f32 m_PrevTime = 0.0f;

	// 분할 패널 폭
	float m_TimelineZoom = 1.0f;
	float m_ViewStartTime = 0.0f;
	f32 m_TrackHeaderWidth = 180.0f;
	const f32 m_TrackHeight = 30.0f;
	float m_LeftPanelWidth = 220.0f;
	float m_InspectorWidth = 280.0f;
	float m_TimelineHeight = 220.0f;
	bool m_IsDraggingHeaderSplitter = false;

	// 선택 및 인터랙션
	TimelineSelection m_Selection;
	bool m_IsDraggingItem = false;
	float m_DragTimeOffset = 0.0f;
	int m_RenamingTrackIndex = -1;
	char m_RenameBuf[64] = {};

	// 엔진 표준 뷰포트 위젯
	PreviewViewportWidget m_PreviewWidget;
	Engine::Entity* m_PreviewModelEntity = nullptr;

	// 편집 대상 에셋
	Engine::ResourceHandle<Engine::Model> m_TargetModel;
	int m_SelectedClipIndex = 0;
	std::string m_CurrentClipName = "No Animation";
	std::vector<mat4> m_BoneMatrices;

	// 트랙 컨테이너
	std::vector<Engine::TimelineNotifyTrack> m_NotifyTracks;
	std::vector<Engine::TimelineCameraTrack> m_CameraTracks;

	// 알림 토스트 & 복합 VFX 프리뷰
	std::string m_LastTriggeredNotifyName;
	f32 m_TriggerToastTimer = 0.0f;
	vec3 m_LastTriggeredWorldPos = vec3(0.0f);
	std::vector<entt::entity> m_PreviewVFXEntities;

	std::string m_CurrentAssetPath;
	bool m_IsDirty = false;

private:
	static inline AnimationTimelinePanel* s_ActivePanel = nullptr;
};

END