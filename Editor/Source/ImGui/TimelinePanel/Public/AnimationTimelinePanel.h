#pragma once

#include "ResourceEditorInterface.h"
#include "ShortcutTypes.h"
#include "Model.h"
#include "ActionTimelineAsset.h"
#include <string>
#include <vector>
#include <filesystem>
#include "PreviewViewportWidget.h"
#include "CommandHistory.h"

BEGIN(Engine)
class Scene;
class Animation;
class Entity;
class Skeleton;
END

namespace EditorUI { class ToolbarLayout; }

BEGIN(Editor)

// 트랙 요소 선택 구분
enum class ETimelineSelectionType : uint8
{
	None,
	NotifyMarker,
	CameraShot,
	CameraCut,
	CameraSpline
};

// 트랙 항목 드래그 방식: 전체 이동 또는 지속형 노티파이/카메라 구간의 시작/끝점 조절
enum class ETimelineItemDrag : uint8 { Move, ResizeStart, ResizeEnd };

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
	bool PrepareAssetPathRelocation(AssetPathRelocation& relocation) override;
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
		LoadFromFile(WStrToStr(assetPath.wstring()));
	}
	void SetTargetModel(const Engine::ResourceHandle<Engine::Model>& model);
	void SyncFromSelectedEntity();
	bool SaveToFile(const std::string& filePath);
	bool LoadFromFile(const std::string& filePath);
	virtual void CaptureState(EditorWorkspace& workspace) const override;
	virtual void RestoreState(const EditorWorkspace& workspace) override;

public:
	static AnimationTimelinePanel* GetActivePanel() { return s_ActivePanel; }
	void RequestPreviewPlayback(const std::filesystem::path& assetPath);
	void TogglePlayPause();
	void StepFrame(int step);
	void GoToFirstFrame() { m_CurrentTime = 0.0f; }
	void GoToLastFrame() { m_CurrentTime = GetPlaybackDuration(); m_IsPlaying = false; }
	void ToggleLoop() { m_IsLooping = !m_IsLooping; }
	void ResetZoom() { m_TimelineZoom = 1.0f; m_ViewStartTime = 0.0f; }
	bool CanDeleteSelected() const;
	void DeleteSelected();

	// 편집 이력 (AnimationTimeline.Undo/Redo, 편집 메뉴, 퀵액세스)
	bool CanUndo() const;
	bool CanRedo() const;
	void Undo();
	void Redo();

private:
	// 이력과 저장 기준점의 단위는 저장되는 문서 내용 전체다(트랙, 작성 모델, 클립, 길이, 블렌드 아웃).
	struct DocumentSnapshot
	{
		std::vector<Engine::TimelineNotifyTrack> NotifyTracks;
		std::vector<Engine::TimelineCameraTrack> CameraTracks;
		std::vector<Engine::TimelineCameraCutSection> CameraCutTracks;
		std::vector<Engine::TimelineCameraSplineSection> CameraSplineTracks;
		std::string ModelPath;
		std::string ClipName;
		float Duration = 0.0f;
		float BlendOutTime = 0.0f;
		Engine::ETimelineEndPolicy EndPolicy = Engine::ETimelineEndPolicy::Fixed;
		bool operator==(const DocumentSnapshot& other) const = default;
	};
	DocumentSnapshot CaptureDocument() const;
	bool HasUncommittedEdit() const;
	bool IsEditInProgress() const;
	void CommitDocumentEdit();
	void ApplyDocumentSnapshot(const DocumentSnapshot& snapshot);
	void ResetEditHistory();
	void MarkDocumentSaved();
	bool IsSelectionValid() const;

	// 클립 선택: 문서 길이를 클립에 맞춘다. 범위 밖이면 클립 없음(길이 수동 입력)으로 둔다.
	void SelectClip(int index);
	// 런타임과 같은 종료 정책으로 계산한 재생 길이. m_Duration은 편집 범위(눈금자 길이)이며 Fixed 정책에서만 재생 길이와 같다.
	float GetPlaybackDuration() const;
	float GetSelectedClipLength() const;
	void DrawEndPolicyControls(EditorUI::ToolbarLayout& toolbar);
	int FindClipIndex(const std::string& clipName) const;

	// 모델·소켓 검증
	bool IsMissingSocketBone(const std::string& boneName) const;
	void NotifySaveWarnings() const;
	void DrawNoModelHint() const;

private:
	void DrawCustomQuickAccess() override;
	void OnResourceWindowVisibility(bool visible) override;
	void ReleaseResourceDocument() override;
	const char* GetSaveActionId() const override { return "AnimationTimeline.Save"; }
	void DrawCustomContent() override;
	ImVec2 GetInitialResourceWindowSize() const override { return ImVec2(1060.0f, 660.0f); }
	void DrawCustomToolbar() override;
	ImGuiWindowFlags GetResourceWindowFlags() const override;
	void OpenTimelineFile();
	void SaveTimelineFile();
	void SaveTimelineFileAs();
	void DrawTimelineTrackArea();
	void DrawInspector();
	void DrawNotifyInspector(Engine::AnimNotifyData& notify);
	void DrawCameraShotInspector(Engine::TimelineCameraTrack& camShot);
	void DrawCameraCutInspector(Engine::TimelineCameraCutSection& cutSection);
	void DrawCameraSplineInspector(Engine::TimelineCameraSplineSection& splineSection);

	void DrawAnimPoseTrack(struct ImDrawList* drawList, const struct ImVec2& trackMin, const struct ImVec2& trackMax, float timelineWidth);
	void DrawNotifyTrackRow(struct ImDrawList* drawList, int trackIdx, Engine::TimelineNotifyTrack& track, const struct ImVec2& trackMin, const struct ImVec2& trackMax, float timelineWidth);
	void DrawCameraTrackRow(struct ImDrawList* drawList, int camIdx, Engine::TimelineCameraTrack& camTrack, const struct ImVec2& trackMin, const struct ImVec2& trackMax, float timelineWidth);
	void DrawCameraCutTrackRow(struct ImDrawList* drawList, int cutIdx, Engine::TimelineCameraCutSection& cutSection, const struct ImVec2& trackMin, const struct ImVec2& trackMax, float timelineWidth);
	void DrawCameraSplineTrackRow(struct ImDrawList* drawList, int splineIdx, Engine::TimelineCameraSplineSection& splineSection, const struct ImVec2& trackMin, const struct ImVec2& trackMax, float timelineWidth);

	float TimeToScreenX(float time, float timelineMinX, float timelineWidth) const;
	float ScreenXToTime(float screenX, float timelineMinX, float timelineWidth) const;

	void TriggerNotify(const Engine::AnimNotifyData& notify, bool isTest = false);
	vec3 GetSocketWorldPosition(const std::string& boneName, const vec3& offset) const;
	TimelineSocketTransform GetSocketWorldTransform(const std::string& boneName, const vec3& offsetPos, const vec3& offsetRot) const;
	void UpdatePreviewHeldVFX(f32 dt);
	void ClearPreviewVFX();
	void StartPreviewFromBeginning();

	void AddNotifyAt(int trackIdx, Engine::EAnimNotifyType type, float time);

	// 신규 뷰포트 & Transport 바 & 본 계층 트리
	void DrawTransportBar();
	void DrawBoneHierarchyTree();
	void HandleModelDragDrop();
	void RebuildBoneCache(const Engine::Skeleton* skeleton);
	void DrawBoneTreeNode(const Engine::Skeleton& skeleton, uint32 boneIndex, bool flat);
	std::string BuildBonePath(const Engine::Skeleton& skeleton, uint32 boneIndex) const;
	bool DrawBonePickerCombo(const char* label, std::string& boneName);
	Engine::AnimNotifyData* GetSelectedNotify();
	bool TrySetTargetModelFromPath(const std::wstring& path);
	void SetupPreviewModel();

private:
	// 타임라인 재생 상태
	bool m_IsPlaying = false;
	bool m_IsLooping = true;
	bool m_IsScrubbing = false;
	f32 m_CurrentTime = 0.0f;
	f32 m_Duration = 2.0f;
	f32 m_BlendOutTime = Engine::ActionTimelineAsset::DefaultBlendOutTime; // 저장되는 문서 값. 런타임 종료 시 원래 애니메이션으로 섞는 시간
	Engine::ETimelineEndPolicy m_EndPolicy = Engine::ETimelineEndPolicy::Fixed; // 저장되는 문서 값. 재생이 끝나는 시점 규칙
	f32 m_FPS = 30.0f;
	f32 m_PlaybackSpeed = 1.0f;
	bool m_NotifyStartPending = true;

	// 분할 패널 폭
	float m_TimelineZoom = 1.0f;
	float m_ViewStartTime = 0.0f;
	f32 m_TrackHeaderWidth = 180.0f;
	f32 m_TrackHeight = 30.0f;    // 매 프레임 EditorUI::GetTimelineRowHeight()로 갱신 (글꼴 기준)
	f32 m_TrackAreaLeft = 0.0f;   // 이번 프레임 트랙 이름 칸의 왼쪽 x (DrawTimelineTrackArea가 설정)
	f32 m_AddTrackTime = 0.0f;    // 트랙 추가·노티파이 추가 메뉴를 연 시점의 시간
	bool m_NotifyContextOnItem = false; // 트랙 몸통 메뉴를 노티파이 마커 위에서 열었는지
	float m_LeftPanelWidth = 220.0f;
	float m_InspectorWidth = 280.0f;
	float m_TimelineHeight = 220.0f;
	bool m_IsDraggingHeaderSplitter = false;

	// 본 계층 캐시: 스켈레톤 시그니처가 바뀔 때만 다시 만든다.
	Engine::uint64 m_CachedSkeletonSignature = 0;
	std::vector<std::vector<uint32>> m_BoneChildren;
	std::vector<uint32> m_RootBones;
	std::vector<std::string> m_BoneNames;
	std::vector<std::string> m_BoneNamesLower;
	Engine::int32 m_SelectedBoneIndex = -1;
	char m_BoneFilter[64] = {};
	char m_BonePickerFilter[64] = {};

	// 선택 및 인터랙션
	TimelineSelection m_Selection;
	bool m_IsDraggingItem = false;
	float m_DragTimeOffset = 0.0f;
	float m_DragItemDuration = 0.0f;
	ETimelineItemDrag m_ItemDragMode = ETimelineItemDrag::Move;
	int m_RenamingTrackIndex = -1;
	char m_RenameBuf[64] = {};

	// 엔진 표준 뷰포트 위젯
	PreviewViewportWidget m_PreviewWidget;
	Engine::Entity* m_PreviewModelEntity = nullptr;

	// 편집 대상 에셋
	Engine::ResourceHandle<Engine::Model> m_TargetModel;
	int m_SelectedClipIndex = -1;
	std::string m_CurrentClipName; // 저장되는 클립 이름. 클립이 없으면 비워 두고 화면에서만 안내 문구를 쓴다.
	std::vector<mat4> m_BoneMatrices;

	// 트랙 컨테이너
	std::vector<Engine::TimelineNotifyTrack> m_NotifyTracks;
	std::vector<Engine::TimelineCameraTrack> m_CameraTracks;
	std::vector<Engine::TimelineCameraCutSection> m_CameraCutTracks;
	std::vector<Engine::TimelineCameraSplineSection> m_CameraSplineTracks;

	// 알림 토스트 & 복합 VFX 프리뷰
	std::string m_LastTriggeredNotifyName;
	f32 m_TriggerToastTimer = 0.0f;
	vec3 m_LastTriggeredWorldPos = vec3(0.0f);
	struct PreviewVFX
	{
		entt::entity Entity = entt::null;
		bool IsTest = false;
	};
	std::vector<PreviewVFX> m_PreviewVFXEntities;

	// 지속형 VFX 노티파이로 생성되어 해제를 기다리는 엔티티
	struct PreviewHeldVFX
	{
		entt::entity Entity = entt::null;
		float ReleaseTime = 0.0f;    // 재생 중 발동: 이 타임라인 시간을 지나면 해제
		float TestRemaining = -1.0f; // 즉시 발동 테스트: 남은 실제 시간(초). 음수면 타임라인 기준
	};
	std::vector<PreviewHeldVFX> m_PreviewHeldVFX;


	std::string m_CurrentAssetPath;
	std::string m_ModelAssetPath;
	bool m_IsDirty = false;

	// 편집 이력: 마지막으로 이력에 확정한 문서 상태와 비교해, 입력이 끝난 프레임에 바뀐 만큼을 한 건으로 기록한다.
	// 저장 기준점과 같아지면(되돌리기 포함) 미저장 표시를 지운다.
	CommandHistory m_History;
	DocumentSnapshot m_CommittedDocument;
	DocumentSnapshot m_SavedDocument;

private:
	static inline AnimationTimelinePanel* s_ActivePanel = nullptr;
};

END