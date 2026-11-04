#pragma once

#include "ResourceEditorInterface.h"
#include "ShortcutTypes.h"
#include "Model.h"
#include "ActionTimelineAsset.h"
#include <string>
#include <vector>
#include <filesystem>
#include "ViewportDeferredPipeline.h"
#include "PostProcess.h"

BEGIN(Engine)
class Scene;
class Animation;
END

BEGIN(Editor)

// 3D 뷰포트 실시간 프리뷰 파티클
struct TimelinePreviewParticle
{
	vec3 Position;
	vec3 Velocity;
	vec4 Color;
	f32 Size;
	f32 Age = 0.0f;
	f32 Lifetime = 0.6f;

	bool IsAlive() const { return Age < Lifetime; }
};

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

class AnimationTimelinePanel final : public ResourceEditorInterface
{
public:
	AnimationTimelinePanel();
	virtual ~AnimationTimelinePanel() override;
	virtual void Update(f32 dt) override;
	virtual void Draw() override;
	virtual EShortcutContext GetShortcutContext() const override { return EShortcutContext::AnimationTimeline; }
	// ── ResourceEditorInterface 구현 (콘텐츠 브라우저 더블클릭 연동) ──
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override
	{
		const std::string ext = assetPath.extension().string();
		const std::string name = assetPath.filename().string();
		return (ext == ".actiontimeline" || name.ends_with(".actiontimeline.json"));
	}
	virtual void SetTargetResource(const std::filesystem::path& assetPath) override
	{
		ResourceEditorInterface::SetTargetResource(assetPath);
		LoadFromFile(assetPath.string());
	}
	void SetTargetModel(const Engine::ResourceHandle<Engine::Model>& model);
	void SyncFromSelectedEntity(); // 씬에서 선택된 엔티티 동기화 함수
	bool SaveToFile(const std::string& filePath);
	bool LoadFromFile(const std::string& filePath);

public:
	static AnimationTimelinePanel* GetActivePanel() { return s_ActivePanel; }
	// ── 단축키 조작 인터페이스 ──
	void TogglePlayPause() { m_IsPlaying = !m_IsPlaying; }
	void StepFrame(int step);
	void GoToFirstFrame() { m_CurrentTime = 0.0f; }
	void GoToLastFrame() { m_CurrentTime = m_Duration; m_IsPlaying = false; }
	void ToggleLoop() { m_IsLooping = !m_IsLooping; }
	void ResetZoom() { m_TimelineZoom = 1.0f; m_ViewStartTime = 0.0f; }
	void DeleteSelected();

private:
	void DrawToolbar();
	void DrawPreviewArea();
	void DrawTimelineTrackArea();
	void DrawInspector();
	void DrawNotifyInspector(Engine::AnimNotifyData& notify);
	void DrawCameraShotInspector(Engine::TimelineCameraTrack& camShot);

	void SetupPreviewScene();
	void FreePreviewScene();
	void RenderPreview(f32 dt);
	void DrawRuler(struct ImDrawList* drawList, const struct ImVec2& rulerMin, const struct ImVec2& rulerMax, float totalFrames);
	void DrawPlayhead(struct ImDrawList* drawList, const struct ImVec2& trackMin, const struct ImVec2& trackMax, float curX);

	// 다중 트랙 렌더링 서브루틴
	void DrawTrackGridLines(struct ImDrawList* drawList, const struct ImVec2& trackMin, const struct ImVec2& trackMax, float timelineWidth);
	void DrawAnimPoseTrack(struct ImDrawList* drawList, const struct ImVec2& trackMin, const struct ImVec2& trackMax, float timelineWidth);
	void DrawNotifyTrackRow(struct ImDrawList* drawList, int trackIdx, Engine::TimelineNotifyTrack& track, const struct ImVec2& trackMin, const struct ImVec2& trackMax, float timelineWidth);
	void DrawCameraTrackRow(struct ImDrawList* drawList, int camIdx, Engine::TimelineCameraTrack& camTrack, const struct ImVec2& trackMin, const struct ImVec2& trackMax, float timelineWidth);

	float TimeToScreenX(float time, float timelineMinX, float timelineWidth) const;
	float ScreenXToTime(float screenX, float timelineMinX, float timelineWidth) const;

	void TriggerNotify(const Engine::AnimNotifyData& notify);
	vec3 GetSocketWorldPosition(const std::string& boneName, const vec3& offset) const;

private:
	// ── 타임라인 재생 상태 ──
	bool m_IsPlaying = false;
	bool m_IsLooping = true;
	bool m_IsScrubbing = false;
	f32 m_CurrentTime = 0.0f;
	f32 m_Duration = 2.0f;
	f32 m_FPS = 30.0f;
	f32 m_PlaybackSpeed = 1.0f;
	f32 m_PrevTime = 0.0f;

	// ── 타임라인 줌 & 팬 & 분할 ──
	float m_TimelineZoom = 1.0f;
	float m_ViewStartTime = 0.0f;

	f32 m_TrackHeaderWidth = 180.0f; // 기본 너비를 180으로 여유 있게 상향
	const f32 m_TrackHeight = 30.0f;
	float m_InspectorWidth = 280.0f;
	float m_TimelineHeight = 220.0f;
	bool m_IsDraggingHeaderSplitter = false;

	// ── 선택 및 상호작용 상태 ──
	TimelineSelection m_Selection;
	bool m_IsDraggingItem = false;
	float m_DragTimeOffset = 0.0f; // 클릭한 위치와 아이템 시작점 사이의 시간 차이 (커서 점프 방지)
	int m_RenamingTrackIndex = -1;
	char m_RenameBuf[64] = {};

	// ── 오프스크린 3D 프리뷰 ──
	uint32 m_PreviewWidth = 512;
	uint32 m_PreviewHeight = 512;
	ViewportDeferredPipeline m_Pipeline;
	PostProcessChain m_PostProcessChain;
	std::wstring m_DisplayRTName;
	Engine::Scene* m_PreviewScene = nullptr;
	bool m_PipelineInitialized = false;

	// ── 프리뷰 카메라 & 연출 ──
	vec3 m_CamTarget = vec3(0.0f, 0.9f, 0.0f);
	float m_CamYaw = 0.5f;
	float m_CamPitch = 0.25f;
	float m_CamDistance = 3.5f;
	float m_BaseFOV = 45.0f;
	float m_CurrentFOV = 45.0f;

	mat4 m_CachedView = mat4(1.0f);
	mat4 m_CachedProj = mat4(1.0f);
	ImVec2 m_ViewportScreenMin = ImVec2(0.0f, 0.0f);
	ImVec2 m_ViewportSize = ImVec2(512.0f, 512.0f);

	// ── 편집 대상 에셋 ──
	Engine::ResourceHandle<Engine::Model> m_TargetModel;
	int m_SelectedClipIndex = 0;
	std::string m_CurrentClipName = "No Animation";
	std::vector<mat4> m_BoneMatrices;

	// ── 엔진 ActionTimelineAsset 데이터 컨테이너 ──
	std::vector<Engine::TimelineNotifyTrack> m_NotifyTracks;
	std::vector<Engine::TimelineCameraTrack> m_CameraTracks;

	// ── 실시간 프리뷰 파티클 시뮬레이터 ──
	std::vector<TimelinePreviewParticle> m_PreviewParticles;
	std::string m_LastTriggeredNotifyName;
	f32 m_TriggerToastTimer = 0.0f;
	vec3 m_LastTriggeredWorldPos = vec3(0.0f);

	std::string m_CurrentAssetPath;
	bool m_IsDirty = false;
private:
	static inline AnimationTimelinePanel* s_ActivePanel = nullptr;	
};

END