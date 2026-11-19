#include "AnimationTimelinePanel.h"
#include "EditorUI.h"
#include "AssetPathRelocation.h"
#include "TimelineView.h"
#include "LocalizationManager.h"
#include "IconsFontAwesome7.h"
#include "ResourceManager.h"
#include "RenderTargetManager.h"
#include "Renderer.h"
#include "CameraBuffer.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "ImGuiManager.h"
#include "Scene.h"
#include "Functions.h"
#include "AnimationEvaluator.h"
#include "SelectionManager.h"
#include "Entity.h"
#include "FileDialogs.h"
#include "PlatformPath.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "FlagComponent.h"
#include "LightComponent.h"
#include "AnimatorComponent.h"
#include "SkinnedMeshRendererComponent.h"
#include "TransformSystem.h"
#include "LightSystem.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include "AudioClip.h"
#include "AudioManager.h"
#include "ActionTimelineComponent.h"
#include "NameComponent.h"
#include "ShortcutManager.h"
#include "CompositeVFXComponent.h"
#include "CompositeVFXSystem.h"
#include "CompositeEffect.h"
#include "Skeleton.h"
#include "MaterialInterface.h"
#include "ContentBrowserDragPayload.h"
#include "ReflectionPropertyDrawer.h"
#include "ReflectionRegistry.h"
#include "ReflectionEditState.h"
#include "AssetPathPicker.h"
#include "NotificationManager.h"

namespace
{
	bool IsNotifyAudioFile(const std::filesystem::path& path)
	{
		const std::string name = Engine::ToLowerStr(Engine::WStrToStr(path.filename().wstring()));
		return name.ends_with(".wav") || name.ends_with(".mp3") || name.ends_with(".ogg");
	}

	bool IsNotifyVFXFile(const std::filesystem::path& path)
	{
		const std::string name = Engine::ToLowerStr(Engine::WStrToStr(path.filename().wstring()));
		return name.ends_with(".bamvfx") || name.ends_with(".bamvfx.json");
	}

	const Editor::AssetPathPickerDesc NotifySoundPicker{ { ICON_FA_MUSIC, ImVec4(0.45f, 0.80f, 0.95f, 1.0f), "AUDIO" }, IsNotifyAudioFile };
	const Editor::AssetPathPickerDesc NotifyVFXPicker{ { ICON_FA_WAND_MAGIC_SPARKLES, ImVec4(0.95f, 0.55f, 0.25f, 1.0f), "VFX" }, IsNotifyVFXFile };

	// 상단에 둘 속성과, 리플렉션 표 대신 직접 그리는 속성(본 목록·에셋 선택이 필요함).
	constexpr std::string_view NotifyHeaderProperties[] = { "Name", "TriggerTime", "Duration", "Type" };
	constexpr std::string_view NotifyCustomProperties[] = { "SocketBoneName", "SoundPath", "VFXPath" };

	template<std::size_t N>
	bool ContainsName(const std::string_view(&names)[N], std::string_view name)
	{
		return std::find(std::begin(names), std::end(names), name) != std::end(names);
	}

	struct NotifyTrackVisual
	{
		const char* Icon;
		ImU32 Color;
		Engine::EAnimNotifyType DefaultType; // 더블클릭으로 만드는 노티파이 유형
	};

	NotifyTrackVisual GetNotifyTrackVisual(const Engine::TimelineNotifyTrack& track)
	{
		switch (Engine::ActionTimelineAsset::ResolveTrackKind(track))
		{
		case Engine::ETimelineNotifyTrackKind::Audio: return { ICON_FA_MUSIC, IM_COL32(80, 200, 255, 255), Engine::EAnimNotifyType::Sound };
		case Engine::ETimelineNotifyTrackKind::Event: return { ICON_FA_BELL, IM_COL32(200, 120, 255, 255), Engine::EAnimNotifyType::CustomEvent };
		default: return { ICON_FA_BURST, IM_COL32(255, 175, 45, 255), Engine::EAnimNotifyType::SpawnCompositeVFX };
		}
	}
}

AnimationTimelinePanel::AnimationTimelinePanel()
{
	s_ActivePanel = this;
	m_Name = L"AnimationTimelinePanel";
	m_DisplayNameKey = "Timeline.Title";
	IImGui::SetOpen(false);

	// 엔진 표준 뷰포트 위젯 초기화 (그리드, 스튜디오 조명, 카메라 컨트롤러 내장)
	m_PreviewWidget.Initialize(L"TimelinePreview", 512, 512, true);
	m_PreviewWidget.SetHostPoseControl(true);
	m_PreviewWidget.SetupDefaultStudioLighting();
	m_PreviewWidget.FocusTarget(vec3(0.0f, 0.9f, 0.0f), 3.2f);

	// 기본 다중 트랙 프리셋 구성 (FX 트랙 1개 + 카메라 샷 트랙 1개)
	Engine::TimelineNotifyTrack fxTrack{};
	fxTrack.TrackName = "FX_Weapon";
	fxTrack.Kind = Engine::ETimelineNotifyTrackKind::Effect;
	Engine::AnimNotifyData defaultNotify{};
	defaultNotify.Name = "Slash_Burst";
	defaultNotify.TriggerTime = 0.40f;
	defaultNotify.Type = Engine::EAnimNotifyType::SpawnCompositeVFX;
	defaultNotify.AttachToSocket = true; // 소켓 본은 모델을 고른 뒤 지정한다(기본값은 루트)
	defaultNotify.VFXScale = vec3(1.0f);
	fxTrack.Notifies.push_back(defaultNotify);
	m_NotifyTracks.push_back(fxTrack);

	Engine::TimelineCameraTrack camTrack{};
	camTrack.TrackName = "Cam_ActionZoom";
	camTrack.StartTime = 0.35f;
	camTrack.Duration = 0.45f;
	camTrack.TargetFOV = 32.0f;
	camTrack.ShakeIntensity = 0.4f;
	m_CameraTracks.push_back(camTrack);

	ResetEditHistory();
	MarkDocumentSaved();
}

AnimationTimelinePanel::~AnimationTimelinePanel()
{
	if (s_ActivePanel == this) s_ActivePanel = nullptr;
	ClearPreviewVFX();
	if (m_PreviewModelEntity && m_PreviewWidget.GetScene())
	{
		(void)m_PreviewWidget.GetScene()->DestroyEntity(*m_PreviewModelEntity);
		m_PreviewModelEntity = nullptr;
	}
	m_PreviewWidget.Free();
}

void AnimationTimelinePanel::StepFrame(int step)
{
	const float frameStep = 1.0f / m_FPS;
	m_CurrentTime = std::clamp(m_CurrentTime + static_cast<float>(step) * frameStep, 0.0f, m_Duration);
}

bool AnimationTimelinePanel::IsSelectionValid() const
{
	if (m_Selection.Type == ETimelineSelectionType::NotifyMarker)
	{
		if (m_Selection.TrackIndex < 0 || m_Selection.TrackIndex >= static_cast<int>(m_NotifyTracks.size())) return false;
		const auto& notifies = m_NotifyTracks[m_Selection.TrackIndex].Notifies;
		return m_Selection.ItemIndex >= 0 && m_Selection.ItemIndex < static_cast<int>(notifies.size());
	}
	if (m_Selection.Type == ETimelineSelectionType::CameraShot)
		return m_Selection.TrackIndex >= 0 && m_Selection.TrackIndex < static_cast<int>(m_CameraTracks.size());
	return false;
}

bool AnimationTimelinePanel::CanDeleteSelected() const
{
	return m_Open && !m_IsDraggingItem && !m_IsScrubbing && IsSelectionValid();
}

void AnimationTimelinePanel::DeleteSelected()
{
	if (!CanDeleteSelected()) return;

	if (m_Selection.Type == ETimelineSelectionType::NotifyMarker)
	{
		auto& notifies = m_NotifyTracks[m_Selection.TrackIndex].Notifies;
		notifies.erase(notifies.begin() + m_Selection.ItemIndex);
	}
	else
	{
		m_CameraTracks.erase(m_CameraTracks.begin() + m_Selection.TrackIndex);
	}

	m_Selection = {};
	m_IsDirty = true;
}

AnimationTimelinePanel::DocumentSnapshot AnimationTimelinePanel::CaptureDocument() const
{
	return { m_NotifyTracks, m_CameraTracks, m_ModelAssetPath, m_CurrentClipName, m_Duration, m_BlendOutTime };
}

bool AnimationTimelinePanel::HasUncommittedEdit() const
{
	// 매 프레임 호출되므로 문서를 복사하지 않고 필드별로 비교한다.
	const auto& committed = m_CommittedDocument;
	return m_NotifyTracks != committed.NotifyTracks || m_CameraTracks != committed.CameraTracks ||
		m_ModelAssetPath != committed.ModelPath || m_CurrentClipName != committed.ClipName || m_Duration != committed.Duration ||
		m_BlendOutTime != committed.BlendOutTime;
}

bool AnimationTimelinePanel::IsEditInProgress() const
{
	// 드래그·스크러빙·입력 중인 위젯이 있으면 조작이 끝난 뒤 한 건으로 묶는다.
	return m_IsDraggingItem || m_IsScrubbing || ImGui::IsAnyItemActive() ||
		Engine::InputManager::Get().IsMouseButtonPressed(Engine::EMouseButton::Left);
}

void AnimationTimelinePanel::CommitDocumentEdit()
{
	// 편집 지점마다 명령을 만들지 않고, 입력이 끝난 프레임에 마지막 확정 상태와 비교해 바뀐 만큼을 기록한다.
	if (IsEditInProgress() || !HasUncommittedEdit()) return;

	DocumentSnapshot before = std::move(m_CommittedDocument);
	m_CommittedDocument = CaptureDocument();
	DocumentSnapshot after = m_CommittedDocument;
	m_History.Record(std::make_unique<CallbackEditorCommand>(LOCAL("Timeline.HistoryEditTracks"),
		[this, after = std::move(after)] { ApplyDocumentSnapshot(after); return true; },
		[this, before = std::move(before)] { ApplyDocumentSnapshot(before); return true; }));
	m_IsDirty = !(m_CommittedDocument == m_SavedDocument);
}

void AnimationTimelinePanel::ApplyDocumentSnapshot(const DocumentSnapshot& snapshot)
{
	// 작성 모델이 다르면 다시 불러온다. SetTargetModel이 클립·길이를 초기화하므로 그 뒤에 저장된 값을 되돌린다.
	if (snapshot.ModelPath != m_ModelAssetPath)
	{
		Engine::ResourceHandle<Engine::Model> model;
		if (!snapshot.ModelPath.empty())
			model = Engine::ResourceHandle<Engine::Model>(Engine::ResourceManager::Get().LoadFile(Engine::StrToWStr(snapshot.ModelPath)));
		SetTargetModel(model);
	}
	m_ModelAssetPath = snapshot.ModelPath;

	const int clipIndex = FindClipIndex(snapshot.ClipName);
	if (clipIndex >= 0) SelectClip(clipIndex);
	else m_SelectedClipIndex = -1;
	m_CurrentClipName = snapshot.ClipName;
	m_Duration = snapshot.Duration;
	m_BlendOutTime = snapshot.BlendOutTime;
	m_CurrentTime = std::clamp(m_CurrentTime, 0.0f, m_Duration);

	m_NotifyTracks = snapshot.NotifyTracks;
	m_CameraTracks = snapshot.CameraTracks;
	m_CommittedDocument = snapshot;

	// 되돌린 상태에 없는 항목을 가리키지 않도록 선택·이름 변경을 정리하고, 이전 상태로 만든 프리뷰 VFX를 지운다.
	if (!IsSelectionValid()) m_Selection = {};
	m_RenamingTrackIndex = -1;
	m_IsDraggingItem = false;
	ClearPreviewVFX();
	m_IsDirty = !(snapshot == m_SavedDocument);
}

void AnimationTimelinePanel::ResetEditHistory()
{
	m_History.Clear();
	m_CommittedDocument = CaptureDocument();
}

void AnimationTimelinePanel::MarkDocumentSaved()
{
	m_SavedDocument = CaptureDocument();
	m_IsDirty = false;
}

bool AnimationTimelinePanel::CanUndo() const
{
	return m_Open && !IsEditInProgress() && (HasUncommittedEdit() || m_History.CanUndo());
}

bool AnimationTimelinePanel::CanRedo() const
{
	// 아직 기록하지 않은 편집이 있으면 그 편집이 다시 실행 목록을 지우므로 다시 실행할 수 없다.
	return m_Open && !IsEditInProgress() && !HasUncommittedEdit() && m_History.CanRedo();
}

void AnimationTimelinePanel::Undo()
{
	if (!CanUndo()) return;
	CommitDocumentEdit(); // 아직 기록하지 않은 편집이 있으면 먼저 한 건으로 넣어 그것부터 되돌린다.
	(void)m_History.Undo();
}

void AnimationTimelinePanel::Redo()
{
	if (!CanRedo()) return;
	(void)m_History.Redo();
}

void AnimationTimelinePanel::ReleaseResourceDocument()
{
	SetTargetModel({});
	m_NotifyTracks.clear();
	m_CameraTracks.clear();
	m_CurrentClipName.clear();
	m_CurrentAssetPath.clear();
	m_Selection = {};
	m_IsDraggingItem = false;
	m_IsPlaying = false;
	m_CurrentTime = m_PrevTime = 0.0f;
	m_Duration = 2.0f;
	m_BlendOutTime = Engine::ActionTimelineAsset::DefaultBlendOutTime;
	ResetZoom();
	ResetEditHistory();
	MarkDocumentSaved();
}

void AnimationTimelinePanel::ClearPreviewVFX()
{
	auto* scene = m_PreviewWidget.GetScene();
	if (!scene)
	{
		m_PreviewVFXEntities.clear();
		m_PreviewHeldVFX.clear();
		return;
	}
	auto& reg = scene->GetRegistry();
	for (auto e : m_PreviewVFXEntities)
	{
		if (reg.valid(e)) reg.destroy(e);
	}
	m_PreviewVFXEntities.clear();
	m_PreviewHeldVFX.clear();
}

void AnimationTimelinePanel::UpdatePreviewHeldVFX(f32 dt)
{
	auto* scene = m_PreviewWidget.GetScene();
	if (!scene)
	{
		m_PreviewHeldVFX.clear();
		return;
	}

	auto& reg = scene->GetRegistry();
	for (std::size_t i = 0; i < m_PreviewHeldVFX.size();)
	{
		auto& held = m_PreviewHeldVFX[i];
		auto* vfx = reg.valid(held.Entity) ? reg.try_get<Engine::CompositeVFXComponent>(held.Entity) : nullptr;
		bool expired = vfx == nullptr;
		if (vfx)
		{
			const bool isTestHold = held.TestRemaining > -0.5f; // 테스트 발동은 0 이상에서 시작한다
			if (isTestHold) held.TestRemaining -= dt;
			expired = isTestHold ? held.TestRemaining <= 0.0f : m_CurrentTime >= held.ReleaseTime;
			if (expired) vfx->Release();
		}
		if (expired) m_PreviewHeldVFX.erase(m_PreviewHeldVFX.begin() + static_cast<std::ptrdiff_t>(i));
		else ++i;
	}
}

void AnimationTimelinePanel::SetupPreviewModel()
{
	auto* scene = m_PreviewWidget.GetScene();
	if (!scene) return;

	if (m_PreviewModelEntity)
	{
		(void)scene->DestroyEntity(*m_PreviewModelEntity);
		m_PreviewModelEntity = nullptr;
	}

	auto* model = m_TargetModel.Get();
	if (!model) return;

	Engine::ResourceHandle<Engine::MaterialInterface> matHandle{};
	if (!model->GetMaterials().empty()) matHandle = model->GetMaterials()[0];

	if (scene->InstantiateModel(*model, matHandle, m_PreviewModelEntity) == Engine::EResult::Success)
	{
		m_PreviewWidget.SetModelFocusTarget(*m_PreviewModelEntity);
	}
}

void AnimationTimelinePanel::SetTargetModel(const Engine::ResourceHandle<Engine::Model>& model)
{
	m_TargetModel = model;
	auto* target = m_TargetModel.Get();
	m_ModelAssetPath = target ? Engine::WStrToStr(target->GetPath()) : std::string{};
	ClearPreviewVFX();
	SetupPreviewModel();

	// 모델을 불러오지 못한 경우에는 저장된 클립 이름을 지우지 않는다(파일의 바인딩을 보존).
	if (!target)
	{
		m_SelectedClipIndex = -1;
		return;
	}

	SelectClip(0); // 클립이 없는 모델이면 클립 없음으로 둔다

	m_CurrentTime = 0.0f;
	m_PrevTime = 0.0f;
	m_TimelineZoom = 1.0f;
	m_ViewStartTime = 0.0f;
	m_IsPlaying = false;
}

void AnimationTimelinePanel::SelectClip(int index)
{
	auto* model = m_TargetModel.Get();
	const auto anims = model ? model->GetAnimations() : std::vector<Engine::ResourceHandle<Engine::Animation>>{};
	auto* clip = (index >= 0 && index < static_cast<int>(anims.size())) ? anims[index].Get() : nullptr;
	if (!clip)
	{
		// 클립 없음: 저장값은 비우고 길이는 수동 입력값을 유지한다. 화면에는 안내 문구를 표시한다.
		m_SelectedClipIndex = -1;
		m_CurrentClipName.clear();
		if (m_Duration <= 0.0f) m_Duration = 2.0f;
		return;
	}

	m_SelectedClipIndex = index;
	m_CurrentClipName = Engine::WStrToStr(clip->GetName());
	const f64 tps = clip->GetTicksPerSecond() > 0.0 ? clip->GetTicksPerSecond() : 25.0;
	m_Duration = std::max(0.1f, clip->GetDuration() > 0.0 ? static_cast<f32>(clip->GetDuration() / tps) : 1.0f);
}

int AnimationTimelinePanel::FindClipIndex(const std::string& clipName) const
{
	auto* model = m_TargetModel.Get();
	if (!model || clipName.empty()) return -1;
	const auto anims = model->GetAnimations();
	for (int i = 0; i < static_cast<int>(anims.size()); ++i)
	{
		if (anims[i].IsValid() && Engine::WStrToStr(anims[i]->GetName()) == clipName) return i;
	}
	return -1;
}

bool AnimationTimelinePanel::IsMissingSocketBone(const std::string& boneName) const
{
	// 스켈레톤이 없으면 확인할 수 없으므로 누락으로 보지 않는다(모델 없음 안내가 따로 표시된다).
	return !boneName.empty() && !m_BoneNames.empty() &&
		std::find(m_BoneNames.begin(), m_BoneNames.end(), boneName) == m_BoneNames.end();
}

void AnimationTimelinePanel::NotifySaveWarnings() const
{
	// 저장은 막지 않고, 런타임에서 클립·소켓이 연결되지 않는 경우를 알린다.
	int socketNotifies = 0;
	int missingSockets = 0;
	for (const auto& track : m_NotifyTracks)
	{
		for (const auto& notify : track.Notifies)
		{
			if (notify.SocketBoneName.empty()) continue;
			++socketNotifies;
			if (IsMissingSocketBone(notify.SocketBoneName)) ++missingSockets;
		}
	}

	std::vector<std::string> warnings;
	if (m_ModelAssetPath.empty())
		warnings.push_back(socketNotifies > 0 ? LOCAL_FMT("Timeline.SaveWarnSocketsNoModel", socketNotifies) : LOCAL("Timeline.SaveWarnNoModel"));
	else if (m_CurrentClipName.empty())
		warnings.push_back(LOCAL("Timeline.SaveWarnNoClip"));
	if (!m_ModelAssetPath.empty() && m_BoneNames.empty() && socketNotifies > 0)
		warnings.push_back(LOCAL_FMT("Timeline.SaveWarnMissingSockets", socketNotifies));
	else if (missingSockets > 0)
		warnings.push_back(LOCAL_FMT("Timeline.SaveWarnMissingSockets", missingSockets));

	for (const auto& warning : warnings)
		NotificationManager::Get().AddNotification(LOCAL("Timeline.Title"), warning, ELogLevel::Warn);
}

void AnimationTimelinePanel::DrawNoModelHint() const
{
	// 모델이 없으면 소켓과 클립 타이밍을 확인할 수 없으므로 뷰포트 가운데에 안내를 띄운다(19-6절 빈 상태).
	const std::string hint = LOCAL("Timeline.NoModelHint");
	const ImVec2 origin = m_PreviewWidget.GetViewportScreenPos();
	const ImVec2 size = m_PreviewWidget.GetViewportSize();
	const float wrapWidth = std::max(ImGui::GetFontSize() * 8.0f, size.x * 0.7f);
	const ImVec2 textSize = ImGui::CalcTextSize(hint.c_str(), nullptr, false, wrapWidth);
	const ImVec2 textPos(origin.x + (size.x - textSize.x) * 0.5f, origin.y + (size.y - textSize.y) * 0.5f);
	const float pad = ImGui::GetFontSize() * 0.6f;
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(ImVec2(textPos.x - pad, textPos.y - pad), ImVec2(textPos.x + textSize.x + pad, textPos.y + textSize.y + pad), IM_COL32(0, 0, 0, 150), 4.0f);
	drawList->AddText(nullptr, 0.0f, textPos, ImGui::GetColorU32(ImGuiCol_TextDisabled), hint.c_str(), nullptr, wrapWidth);
}

void AnimationTimelinePanel::SyncFromSelectedEntity()
{
	auto* selectedEntity = SelectionManager::Get().GetPrimarySelectedEntity();
	if (!selectedEntity) return;

	auto* scene = selectedEntity->GetScene();
	const entt::entity handle = selectedEntity->GetEntityHandle();
	if (!scene || handle == entt::null || !scene->GetRegistry().valid(handle)) return;

	auto& registry = scene->GetRegistry();

	if (auto* timelineComp = registry.try_get<Engine::ActionTimelineComponent>(handle))
	{
		if (!timelineComp->timelinePath.empty())
		{
			std::string path = timelineComp->timelinePath;
			RunWithDiscardConfirmation([this, path = std::move(path)] { (void)LoadFromFile(path); });
			return;
		}
	}

	if (auto* nameComp = registry.try_get<Engine::NameComponent>(handle))
	{
		const std::filesystem::path candidate = std::filesystem::path(L"Resources/Model") / (nameComp->name + L".bammodel");
		if (std::filesystem::exists(candidate))
		{
			auto modelHandle = Engine::ResourceManager::Get().LoadFile(candidate.wstring());
			if (auto* model = dynamic_cast<Engine::Model*>(Engine::ResourceManager::Get().GetResource(modelHandle)))
			{
				SetTargetModel(Engine::ResourceHandle<Engine::Model>(modelHandle));
			}
		}
	}
}

float AnimationTimelinePanel::TimeToScreenX(float time, float timelineMinX, float timelineWidth) const
{
	return EditorUI::MakeTimelineScale(timelineMinX, timelineWidth, m_Duration, m_TimelineZoom, m_ViewStartTime).ToX(time);
}

float AnimationTimelinePanel::ScreenXToTime(float screenX, float timelineMinX, float timelineWidth) const
{
	const auto scale = EditorUI::MakeTimelineScale(timelineMinX, timelineWidth, m_Duration, m_TimelineZoom, m_ViewStartTime);
	return std::clamp(scale.ToTime(screenX), 0.0f, m_Duration);
}

bool AnimationTimelinePanel::PrepareAssetPathRelocation(AssetPathRelocation& relocation)
{
	if (!PrepareTargetPathRelocation(relocation)) return false;
	const bool modelChanged = relocation.AddUtf8(m_ModelAssetPath, true);
	const bool notifiesChanged = relocation.AddTimelineNotifyPaths(m_NotifyTracks);
	if ((modelChanged || notifiesChanged) && !m_CurrentAssetPath.empty() &&
		!Engine::ActionTimelineAsset::IsProjectJsonDocument(Engine::Utf8ToPath(m_CurrentAssetPath))) return false;
	relocation.AddUtf8(m_CurrentAssetPath);
	// 파일 이동으로 바뀐 모델·노티파이 경로는 편집이 아니다. 이전 스냅샷에는 옮기기 전 경로가 남으므로 이력 기준점을 새로 잡고,
	// 저장 기준점의 경로도 같이 옮겨 이동만으로 미저장 표시가 켜지지 않게 한다(파일 내용도 함께 갱신된다).
	if (modelChanged || notifiesChanged)
	{
		relocation.AfterCommit([this, wasDirty = m_IsDirty]
			{
				ResetEditHistory();
				if (!wasDirty) MarkDocumentSaved();
			});
	}
	return true;
}

bool AnimationTimelinePanel::SaveToFile(const std::string& filePath)
{
	auto* asset = Engine::ActionTimelineAsset::CreateEmpty();
	if (!asset) return false;

	// m_ModelAssetPath는 모델을 고를 때(SetTargetModel)·파일을 열 때·경로 이동 때 함께 갱신되므로 그대로 저장한다.
	// 저장 기준점과 같은 문자열을 써야 저장 직후 미저장 판정이 어긋나지 않는다.
	asset->SetModelPath(m_ModelAssetPath);
	asset->SetClipName(m_CurrentClipName);
	asset->SetDuration(m_Duration);
	asset->SetBlendOutTime(m_BlendOutTime);
	asset->GetNotifyTracks() = m_NotifyTracks;
	asset->GetCameraTracks() = m_CameraTracks;

	const bool success = asset->SaveToFile(filePath);
	Engine::ResourceManager::Get().DestroyResource(asset);

	if (success)
	{
		m_CurrentAssetPath = filePath;
		m_TargetResourcePath = Engine::Utf8ToPath(filePath);
		MarkDocumentSaved();
		NotifySaveWarnings();
	}
	return success;
}

bool AnimationTimelinePanel::LoadFromFile(const std::string& filePath)
{
	auto* asset = Engine::ActionTimelineAsset::CreateEmpty();
	if (!asset) return false;

	if (!asset->LoadFromFile(filePath))
	{
		Engine::ResourceManager::Get().DestroyResource(asset);
		return false;
	}

	Engine::ResourceHandle<Engine::Model> model;
	if (!asset->GetModelPath().empty())
	{
		auto& rm = Engine::ResourceManager::Get();
		model = Engine::ResourceHandle<Engine::Model>(rm.LoadFile(asset->GetModelPath()));
	}
	SetTargetModel(model);
	m_ModelAssetPath = asset->GetModelPath();

	// 저장된 클립을 고르되, 모델에서 찾지 못해도 파일의 클립 이름과 길이는 그대로 보존한다.
	const int clipIndex = FindClipIndex(asset->GetClipName());
	if (clipIndex >= 0) SelectClip(clipIndex);
	else m_SelectedClipIndex = -1;
	m_CurrentClipName = asset->GetClipName();
	m_Duration = std::max(0.1f, asset->GetDuration());
	m_BlendOutTime = asset->GetBlendOutTime();
	m_NotifyTracks = asset->GetNotifyTracks();
	m_CameraTracks = asset->GetCameraTracks();

	Engine::ResourceManager::Get().DestroyResource(asset);
	m_CurrentAssetPath = filePath;
	m_TargetResourcePath = Engine::Utf8ToPath(filePath);
	m_Selection = {};
	m_RenamingTrackIndex = -1;
	ClearPreviewVFX();
	ResetEditHistory();
	MarkDocumentSaved();
	return true;
}

TimelineSocketTransform AnimationTimelinePanel::GetSocketWorldTransform(const std::string& boneName, const vec3& offsetPos, const vec3& offsetRot) const
{
	TimelineSocketTransform result;
	mat4 entityWorldMat = mat4(1.0f);
	if (m_PreviewModelEntity && m_PreviewWidget.GetScene())
	{
		const auto& reg = m_PreviewWidget.GetScene()->GetRegistry();
		if (const auto* wt = reg.try_get<Engine::WorldTransformComponent>(m_PreviewModelEntity->GetEntityHandle()))
		{
			entityWorldMat = wt->worldMatrix;
		}
		else if (const auto* trans = reg.try_get<Engine::TransformComponent>(m_PreviewModelEntity->GetEntityHandle()))
		{
			entityWorldMat = trans->GetLocalMatrix();
		}
	}

	const mat4 rotMat = glm::mat4_cast(glm::quat(glm::radians(offsetRot)));
	const mat4 offsetMat = glm::translate(mat4(1.0f), offsetPos) * rotMat;

	auto* model = m_TargetModel.Get();
	if (!model || boneName.empty() || m_BoneMatrices.empty())
	{
		const mat4 finalMat = entityWorldMat * offsetMat;
		result.position = vec3(finalMat[3]);
		result.rotation = glm::quat_cast(finalMat);
		return result;
	}

	auto skeleton = model->GetSkeleton();
	if (!skeleton.IsValid())
	{
		const mat4 finalMat = entityWorldMat * offsetMat;
		result.position = vec3(finalMat[3]);
		result.rotation = glm::quat_cast(finalMat);
		return result;
	}

	const std::wstring boneNameW = Engine::StrToWStr(boneName);
	const auto& boneMap = skeleton->GetBoneMap();
	const auto& bones = skeleton->GetBones();
	auto it = boneMap.find(boneNameW);
	if (it != boneMap.end())
	{
		uint32 boneIdx = it->second;
		if (boneIdx < m_BoneMatrices.size() && boneIdx < bones.size())
		{
			mat4 invOffset = glm::inverse(bones[boneIdx].OffsetMatrix);
			mat4 combined = m_BoneMatrices[boneIdx] * invOffset;
			mat4 finalMat = entityWorldMat * combined * offsetMat;
			result.position = vec3(finalMat[3]);
			result.rotation = glm::quat_cast(finalMat);
			return result;
		}
	}

	const mat4 finalMat = entityWorldMat * offsetMat;
	result.position = vec3(finalMat[3]);
	result.rotation = glm::quat_cast(finalMat);
	return result;
}

vec3 AnimationTimelinePanel::GetSocketWorldPosition(const std::string& boneName, const vec3& offset) const
{
	return GetSocketWorldTransform(boneName, offset, vec3(0.0f)).position;
}

void AnimationTimelinePanel::TriggerNotify(const Engine::AnimNotifyData& notify, bool isTest)
{
	m_LastTriggeredNotifyName = notify.Name;
	m_TriggerToastTimer = 1.4f;
	m_LastTriggeredWorldPos = GetSocketWorldPosition(notify.SocketBoneName, notify.OffsetPosition);

	if (notify.Type == Engine::EAnimNotifyType::Sound && !notify.SoundPath.empty())
	{
		auto& rm = Engine::ResourceManager::Get();
		auto handle = rm.LoadFile(Engine::StrToWStr(notify.SoundPath));
		if (auto* clip = dynamic_cast<Engine::AudioClip*>(rm.GetResource(handle)))
		{
			if (clip->GetFMODSound()) Engine::AudioManager::Get().PlayOneShot(clip->GetFMODSound(), notify.SoundVolume);
		}
	}
	else if (notify.Type == Engine::EAnimNotifyType::SpawnCompositeVFX && !notify.VFXPath.empty())
	{
		if (auto* scene = m_PreviewWidget.GetScene())
		{
			auto& rm = Engine::ResourceManager::Get();
			auto handle = rm.LoadFile(Engine::StrToWStr(notify.VFXPath));
			if (handle.IsValid())
			{
				auto& reg = scene->GetRegistry();
				entt::entity e = reg.create();
				const auto xform = GetSocketWorldTransform(notify.SocketBoneName, notify.OffsetPosition, notify.OffsetRotation);

				auto& trans = reg.emplace<Engine::TransformComponent>(e);
				(void)trans.SetLocalPosition(xform.position);
				trans.rotation = xform.rotation;
				(void)trans.SetEulerRotation(glm::degrees(glm::eulerAngles(xform.rotation)));
				(void)trans.SetLocalScale(notify.VFXScale);

				reg.emplace<Engine::WorldTransformComponent>(e);
				auto& flag = reg.emplace<Engine::FlagComponent>(e);
				flag.flags = Engine::EEntityFlag::Active | Engine::EEntityFlag::Visible;

				auto& vfxComp = reg.emplace<Engine::CompositeVFXComponent>(e);
				vfxComp.effectHandle = Engine::ResourceHandle<Engine::CompositeEffect>(handle);
				vfxComp.isPlaying = true;
				vfxComp.isLooping = false;
				vfxComp.playOnce = true;
				vfxComp.holdUntilRelease = notify.Duration > 0.0f;
				vfxComp.playRate = notify.VFXPlayRate;
				vfxComp.Restart();

				if (notify.AttachToSocket)
				{
					auto& attach = reg.emplace<Engine::SocketAttachmentComponent>(e);
					attach.targetEntity = m_PreviewModelEntity ? m_PreviewModelEntity->GetEntityHandle() : entt::null;
					attach.boneName = notify.SocketBoneName;
					attach.offsetPosition = notify.OffsetPosition;
					attach.offsetRotation = notify.OffsetRotation;
					attach.scale = notify.VFXScale;
				}

				m_PreviewVFXEntities.push_back(e);
				if (vfxComp.holdUntilRelease)
					m_PreviewHeldVFX.push_back({ e, notify.TriggerTime + notify.Duration, isTest ? notify.Duration : -1.0f });
			}
		}
	}
}

void AnimationTimelinePanel::Update(f32 dt)
{
	if (!m_Open) return;

	m_PreviewWidget.BeginFrame(dt);

	const f32 oldTime = m_CurrentTime;
	const bool showBindPose = m_PreviewWidget.IsBindPoseShown();

	if (m_IsPlaying && !m_IsScrubbing && m_Duration > 0.0f)
	{
		m_CurrentTime += dt * m_PlaybackSpeed;

		for (const auto& track : m_NotifyTracks)
		{
			if (track.IsMuted || showBindPose) continue;
			for (const auto& notify : track.Notifies)
			{
				if (oldTime < notify.TriggerTime && m_CurrentTime >= notify.TriggerTime)
				{
					TriggerNotify(notify);
				}
			}
		}

		if (m_CurrentTime >= m_Duration)
		{
			if (m_IsLooping) m_CurrentTime = std::fmod(m_CurrentTime, m_Duration);
			else { m_CurrentTime = m_Duration; m_IsPlaying = false; }
		}
	}

	m_PrevTime = m_CurrentTime;
	if (m_CurrentTime < oldTime) ClearPreviewVFX();

	Engine::AnimatorComponent* animator = nullptr;
	if (m_PreviewModelEntity && m_PreviewWidget.GetScene())
		animator = m_PreviewWidget.GetRegistry().try_get<Engine::AnimatorComponent>(m_PreviewModelEntity->GetEntityHandle());

	auto* model = m_TargetModel.Get();
	auto skeleton = model ? model->GetSkeleton() : Engine::ResourceHandle<Engine::Skeleton>();
	auto anims = model ? model->GetAnimations() : std::vector<Engine::ResourceHandle<Engine::Animation>>();
	auto* skel = skeleton.IsValid() ? skeleton.Get() : nullptr;
	auto* clip = (!showBindPose && m_SelectedClipIndex >= 0 && m_SelectedClipIndex < static_cast<int>(anims.size())) ? anims[m_SelectedClipIndex].Get() : nullptr;

	if (skel && clip)
		Engine::AnimationEvaluator::EvaluateSkinningMatrices(*clip, *skel, m_CurrentTime, m_BoneMatrices, m_IsLooping);
	else if (skel)
		Engine::AnimationEvaluator::EvaluateBindPoseSkinningMatrices(*skel, m_BoneMatrices);

	if (animator)
		animator->finalBoneMatrices = m_BoneMatrices;

	if (auto* scene = m_PreviewWidget.GetScene())
	{
		auto& reg = scene->GetRegistry();
		auto attachView = reg.view<Engine::SocketAttachmentComponent, Engine::TransformComponent>();
		for (auto e : attachView)
		{
			auto& attach = attachView.get<Engine::SocketAttachmentComponent>(e);
			auto& trans = attachView.get<Engine::TransformComponent>(e);
			const auto xform = GetSocketWorldTransform(attach.boneName, attach.offsetPosition, attach.offsetRotation);
			(void)trans.SetLocalPosition(xform.position);
			trans.rotation = xform.rotation;
			(void)trans.SetEulerRotation(glm::degrees(glm::eulerAngles(xform.rotation)));
			(void)trans.SetLocalScale(attach.scale);
		}

		UpdatePreviewHeldVFX(dt);
		Engine::CompositeVFXSystem::SimulateSceneVFX(scene, dt);

		auto it = m_PreviewVFXEntities.begin();
		while (it != m_PreviewVFXEntities.end())
		{
			entt::entity e = *it;
			if (reg.valid(e))
			{
				if (const auto* vfx = reg.try_get<Engine::CompositeVFXComponent>(e))
				{
					if (vfx->isFinished && !vfx->isLooping)
					{
						reg.destroy(e);
						it = m_PreviewVFXEntities.erase(it);
						continue;
					}
				}
			}
			++it;
		}
	}

	if (m_TriggerToastTimer > 0.0f) m_TriggerToastTimer -= dt;

	m_PreviewWidget.Submit(dt);
}

ImGuiWindowFlags AnimationTimelinePanel::GetResourceWindowFlags() const
{
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar;
	if (m_IsDraggingItem || m_IsScrubbing || m_IsDraggingHeaderSplitter) flags |= ImGuiWindowFlags_NoMove;
	return flags;
}

bool AnimationTimelinePanel::TrySetTargetModelFromPath(const std::wstring& path)
{
	std::wstring ext = std::filesystem::path(path).extension().wstring();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
	if (ext != L".bammodel" && ext != L".glb" && ext != L".gltf") return false;

	auto& rm = Engine::ResourceManager::Get();
	const auto handle = rm.LoadFile(path);
	if (!dynamic_cast<Engine::Model*>(rm.GetResource(handle))) return false;

	SetTargetModel(Engine::ResourceHandle<Engine::Model>(handle));
	return true;
}

void AnimationTimelinePanel::HandleModelDragDrop()
{
	if (!ImGui::BeginDragDropTarget()) return;
	if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(Editor::ContentBrowser::SingleItemPayload))
	{
		const auto paths = Editor::ContentBrowser::DecodeDragPaths(payload->Data, payload->DataSize);
		if (paths.size() == 1) (void)TrySetTargetModelFromPath(paths.front());
	}
	ImGui::EndDragDropTarget();
}

void AnimationTimelinePanel::DrawBoneHierarchyTree()
{
	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), ICON_FA_SITEMAP " %s", LOCAL_CSTR("Timeline.BoneHierarchy"));
	ImGui::Separator();
	EditorUI::SearchBar("BoneSearch", m_BoneFilter, sizeof(m_BoneFilter), "Timeline.SearchBones");

	auto* model = m_TargetModel.Get();
	auto skeletonHandle = model ? model->GetSkeleton() : Engine::ResourceHandle<Engine::Skeleton>();
	const Engine::Skeleton* skeleton = skeletonHandle.IsValid() ? skeletonHandle.Get() : nullptr;
	const Engine::uint64 signature = skeleton ? skeleton->GetSkinBindingSignature() : 0;
	if (signature != m_CachedSkeletonSignature || (skeleton && m_BoneNames.size() != skeleton->GetBones().size()))
		RebuildBoneCache(skeleton);
	if (!skeleton || m_BoneNames.empty())
	{
		EditorUI::EmptyState("Timeline.NoSkeletonAvailable");
		return;
	}

	// 깊은 계층에서도 이름이 잘리지 않도록 가로 스크롤을 허용하고 들여쓰기를 줄인다.
	ImGui::BeginChild("##BoneTreeScroll", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 0.75f);
	const std::string filter = Engine::ToLowerStr(m_BoneFilter);
	if (filter.empty())
	{
		for (uint32 root : m_RootBones) DrawBoneTreeNode(*skeleton, root, false);
	}
	else
	{
		bool found = false;
		for (uint32 i = 0; i < static_cast<uint32>(m_BoneNames.size()); ++i)
		{
			if (m_BoneNamesLower[i].find(filter) == std::string::npos) continue;
			DrawBoneTreeNode(*skeleton, i, true);
			found = true;
		}
		if (!found) EditorUI::EmptyState("Timeline.NoMatchingBones");
	}
	ImGui::PopStyleVar();
	ImGui::EndChild();
}

void AnimationTimelinePanel::RebuildBoneCache(const Engine::Skeleton* skeleton)
{
	m_BoneChildren.clear();
	m_RootBones.clear();
	m_BoneNames.clear();
	m_BoneNamesLower.clear();
	m_SelectedBoneIndex = -1;
	m_CachedSkeletonSignature = skeleton ? skeleton->GetSkinBindingSignature() : 0;
	if (!skeleton) return;

	const auto& bones = skeleton->GetBones();
	const auto count = static_cast<Engine::int32>(bones.size());
	m_BoneChildren.resize(bones.size());
	m_BoneNames.reserve(bones.size());
	m_BoneNamesLower.reserve(bones.size());
	for (Engine::int32 i = 0; i < count; ++i)
	{
		m_BoneNames.push_back(Engine::WStrToStr(bones[i].Name));
		m_BoneNamesLower.push_back(Engine::ToLowerStr(m_BoneNames.back()));
		const Engine::int32 parent = bones[i].ParentIndex;
		if (parent >= 0 && parent < count && parent != i) m_BoneChildren[parent].push_back(static_cast<uint32>(i));
		else m_RootBones.push_back(static_cast<uint32>(i));
	}
}

void AnimationTimelinePanel::DrawBoneTreeNode(const Engine::Skeleton& skeleton, uint32 boneIndex, bool flat)
{
	const std::string& name = m_BoneNames[boneIndex];
	const bool leaf = flat || m_BoneChildren[boneIndex].empty();
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (leaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	if (m_SelectedBoneIndex == static_cast<Engine::int32>(boneIndex)) flags |= ImGuiTreeNodeFlags_Selected;

	const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(boneIndex)), flags, "%s %s", ICON_FA_BONE, name.c_str());
	// 19-5절: 단일 클릭은 선택, 더블클릭은 포커스, 우클릭은 컨텍스트 메뉴. 클릭으로 데이터를 바꾸지 않는다.
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
		m_SelectedBoneIndex = static_cast<Engine::int32>(boneIndex);
	if (ImGui::IsItemHovered())
	{
		if (Engine::InputManager::Get().IsMouseButtonDoubleClicked(Engine::EMouseButton::Left))
			m_PreviewWidget.FocusTarget(GetSocketWorldPosition(name, vec3(0.0f)), 1.2f);
		ImGui::SetTooltip("%s", BuildBonePath(skeleton, boneIndex).c_str());
	}
	if (ImGui::BeginPopupContextItem())
	{
		auto* notify = GetSelectedNotify();
		if (ImGui::MenuItem(LOCAL_CSTR("Timeline.SetAsSocket"), nullptr, false, notify != nullptr) && notify->SocketBoneName != name)
		{
			notify->SocketBoneName = name;
			m_IsDirty = true;
		}
		if (ImGui::MenuItem(LOCAL_CSTR("Timeline.FocusBone")))
			m_PreviewWidget.FocusTarget(GetSocketWorldPosition(name, vec3(0.0f)), 1.2f);
		ImGui::EndPopup();
	}
	if (open && !leaf)
	{
		for (uint32 child : m_BoneChildren[boneIndex]) DrawBoneTreeNode(skeleton, child, false);
		ImGui::TreePop();
	}
}

std::string AnimationTimelinePanel::BuildBonePath(const Engine::Skeleton& skeleton, uint32 boneIndex) const
{
	const auto& bones = skeleton.GetBones();
	const auto count = static_cast<Engine::int32>(bones.size());
	std::string path = m_BoneNames[boneIndex];
	Engine::int32 parent = bones[boneIndex].ParentIndex;
	for (int depth = 0; parent >= 0 && parent < count && depth < 128; ++depth)
	{
		path = m_BoneNames[parent] + " / " + path;
		parent = bones[parent].ParentIndex;
	}
	return path;
}

Engine::AnimNotifyData* AnimationTimelinePanel::GetSelectedNotify()
{
	if (m_Selection.Type != ETimelineSelectionType::NotifyMarker) return nullptr;
	if (m_Selection.TrackIndex < 0 || m_Selection.TrackIndex >= static_cast<int>(m_NotifyTracks.size())) return nullptr;
	auto& notifies = m_NotifyTracks[m_Selection.TrackIndex].Notifies;
	if (m_Selection.ItemIndex < 0 || m_Selection.ItemIndex >= static_cast<int>(notifies.size())) return nullptr;
	return &notifies[m_Selection.ItemIndex];
}

bool AnimationTimelinePanel::DrawBonePickerCombo(const char* label, std::string& boneName)
{
	const std::string preview = boneName.empty() ? LOCAL("Timeline.RootOrNone") : boneName;
	if (!ImGui::BeginCombo(label, preview.c_str(), ImGuiComboFlags_HeightLarge)) return false;

	const bool appearing = ImGui::IsWindowAppearing();
	if (appearing) m_BonePickerFilter[0] = '\0';
	EditorUI::DrawSearchBar("BonePickerSearch", m_BonePickerFilter, sizeof(m_BonePickerFilter), "Timeline.SearchBones", EditorUI::SearchBarOptions{ 0.0f, appearing });

	bool changed = false;
	const std::string filter = Engine::ToLowerStr(m_BonePickerFilter);
	if (filter.empty() && ImGui::Selectable(LOCAL_LABEL("Timeline.RootOrNone").c_str(), boneName.empty()) && !boneName.empty())
	{
		boneName.clear();
		changed = true;
	}
	for (uint32 i = 0; i < static_cast<uint32>(m_BoneNames.size()); ++i)
	{
		if (!filter.empty() && m_BoneNamesLower[i].find(filter) == std::string::npos) continue;
		const bool selected = boneName == m_BoneNames[i];
		ImGui::PushID(static_cast<int>(i));
		if (ImGui::Selectable(m_BoneNames[i].c_str(), selected) && !selected)
		{
			boneName = m_BoneNames[i];
			changed = true;
		}
		if (selected && appearing) ImGui::SetScrollHereY();
		ImGui::PopID();
	}
	ImGui::EndCombo();
	return changed;
}

void AnimationTimelinePanel::DrawCustomContent()
{
	const float availWidth = ImGui::GetContentRegionAvail().x;
	const float availHeight = ImGui::GetContentRegionAvail().y;
	const float spacingX = ImGui::GetStyle().ItemSpacing.x;

	const float splitterThickness = Editor::SplitterThickness;
	const float upperHeight = std::max(120.0f, availHeight - m_TimelineHeight - splitterThickness - 4.0f);
	m_LeftPanelWidth = std::clamp(m_LeftPanelWidth, 140.0f, std::max(140.0f, availWidth * 0.35f));
	m_InspectorWidth = std::clamp(m_InspectorWidth, 180.0f, std::max(180.0f, availWidth * 0.40f));
	// 한 줄에 [좌측][간격][스플리터][간격][뷰포트][간격][스플리터][간격][인스펙터]: SameLine 간격은 4번이다.
	const float previewWidth = std::max(150.0f, availWidth - m_LeftPanelWidth - m_InspectorWidth - splitterThickness * 2.0f - spacingX * 4.0f);

	// ── 상단 좌측: 스켈레톤 본 계층 트리 영역 ──
	ImGui::BeginChild("##LeftPanelRegion", ImVec2(m_LeftPanelWidth, upperHeight), true);
	DrawBoneHierarchyTree();
	ImGui::EndChild();

	ImGui::SameLine();

	// ── [스플리터 1] 좌측 패널 vs 중앙 뷰포트 (2px 슬림) ──
	if (const float drag = Editor::SplitterBar("##VerticalSplitter_Left", ImGuiAxis_X, upperHeight); drag != 0.0f)
		m_LeftPanelWidth = std::clamp(m_LeftPanelWidth + drag, 140.0f, std::max(140.0f, availWidth - m_InspectorWidth - 250.0f));

	ImGui::SameLine();

	// ── 상단 중앙: 3D 오프스크린 뷰포트 영역 ──
	ImGui::BeginChild("##ViewportRegion", ImVec2(previewWidth, upperHeight), false);
	m_PreviewWidget.Draw(ImVec2(previewWidth, upperHeight));
	if (!m_TargetModel.Get()) DrawNoModelHint();
	ImGui::EndChild();
	HandleModelDragDrop();

	ImGui::SameLine();

	// ── [스플리터 2] 중앙 뷰포트 vs 우측 인스펙터 (2px 슬림) ──
	if (const float drag = Editor::SplitterBar("##VerticalSplitter_Inspector", ImGuiAxis_X, upperHeight); drag != 0.0f)
		m_InspectorWidth = std::clamp(m_InspectorWidth - drag, 180.0f, std::max(180.0f, availWidth - m_LeftPanelWidth - 250.0f));

	ImGui::SameLine();

	// ── 상단 우측: 인스펙터 영역 ──
	ImGui::BeginChild("##InspectorRegion", ImVec2(m_InspectorWidth, upperHeight), true);
	DrawInspector();
	ImGui::EndChild();

	// ── [스플리터 3] 상단 영역 vs 하단 타임라인 (2px 슬림) ──
	if (const float drag = Editor::SplitterBar("##HorizontalSplitter_Timeline", ImGuiAxis_Y); drag != 0.0f)
		m_TimelineHeight = std::clamp(m_TimelineHeight - drag, 130.0f, std::max(130.0f, availHeight - 140.0f));

	// ── 하단: 다중 트랙 타임라인 영역 ──
	DrawTimelineTrackArea();

	// 메뉴·퀵액세스·인스펙터·트랙 행에서 이번 프레임에 끝난 편집을 이력 한 건으로 확정한다.
	CommitDocumentEdit();
}

void AnimationTimelinePanel::DrawCustomToolbar()
{
	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.File").c_str()))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("EditorCommon.Open").c_str())) RunWithDiscardConfirmation([this] { OpenTimelineFile(); });
		DrawSaveMenuItem();
		if (ImGui::MenuItem(LOCAL_LABEL("EditorCommon.SaveAs").c_str())) SaveTimelineFileAs();
		ImGui::Separator();
		// 기본 구현의 닫기를 대신하므로 직접 둔다(19-7절).
		DrawCloseMenuItem();
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.Edit").c_str()))
	{
		DrawHistoryMenuItems("AnimationTimeline");
		ImGui::EndMenu();
	}
}

void AnimationTimelinePanel::OpenTimelineFile()
{
	std::wstring selectedPath;
	std::vector<FileDialogFilter> filters = {
		{ Engine::StrToWStr(LOCAL("FileDialog.ActionTimelineFiles")), L"*.bamtimeline.json;*.actiontimeline.json" },
		{ Engine::StrToWStr(LOCAL("FileDialog.AllFiles")), L"*.*" }
	};
	if (FileDialogs::OpenFileDialog(selectedPath, filters, L"", EFileDialogContext::TimelineOpen))
	{
		(void)LoadFromFile(Engine::WStrToStr(selectedPath));
	}
}

void AnimationTimelinePanel::SaveTimelineFile()
{
	if (m_CurrentAssetPath.empty()) SaveTimelineFileAs();
	else (void)SaveToFile(m_CurrentAssetPath);
}

void AnimationTimelinePanel::SaveTimelineFileAs()
{
	std::wstring savePath;
	std::vector<FileDialogFilter> filters = {
		{ Engine::StrToWStr(LOCAL("FileDialog.ActionTimelineFiles")), L"*.bamtimeline.json;*.actiontimeline.json" }
	};
	// 열린 문서가 있으면 그 파일 이름을, 없으면 클립 이름을 기본값으로 쓴다.
	const std::string baseName = m_CurrentClipName.empty() ? std::string("NewTimeline") : m_CurrentClipName;
	const std::wstring defaultName = m_CurrentAssetPath.empty()
		? Engine::StrToWStr(baseName) + L".bamtimeline.json"
		: Engine::Utf8ToPath(m_CurrentAssetPath).filename().wstring();
	if (FileDialogs::SaveFileDialog(savePath, filters, defaultName, L"bamtimeline.json", EFileDialogContext::TimelineSave))
		(void)SaveToFile(Engine::WStrToStr(savePath));
}

void AnimationTimelinePanel::DrawCustomQuickAccess()
{
	EditorUI::ToolbarLayout toolbar;
	if (toolbar.Icon("Open", ICON_FA_FOLDER_OPEN, "Timeline.OpenTimeline")) RunWithDiscardConfirmation([this] { OpenTimelineFile(); });
	// 저장·실행 취소·다시 실행은 등록된 액션을 실행해 툴팁에 단축키 힌트가 함께 표시된다(19-7절 3항).
	DrawSaveButton(toolbar);
	DrawHistoryButtons(toolbar, "AnimationTimeline");

	if (toolbar.Button("Sync", ICON_FA_ARROWS_ROTATE, "Timeline.SyncEntity"))
	{
		SyncFromSelectedEntity();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.SyncTooltip"));

	if (toolbar.Button("SelectModel", ICON_FA_FOLDER_OPEN, "Timeline.SelectModel"))
	{
		std::wstring selectedPath;
		std::vector<FileDialogFilter> filters = {
			{ Engine::StrToWStr(LOCAL("FileDialog.ModelFiles")), L"*.bammodel;*.glb;*.gltf" },
			{ Engine::StrToWStr(LOCAL("FileDialog.AllFiles")), L"*.*" }
		};
		if (FileDialogs::OpenFileDialog(selectedPath, filters, L"Resources/Model"))
			(void)TrySetTargetModelFromPath(selectedPath);
	}

	auto* model = m_TargetModel.Get();
	if (model)
	{
		auto anims = model->GetAnimations();
		if (!anims.empty())
		{
			toolbar.SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
			const std::string clipPreview = m_CurrentClipName.empty() ? LOCAL("EditorCommon.None") : m_CurrentClipName;
			if (ImGui::BeginCombo("##ClipSelect", clipPreview.c_str()))
			{
				for (int i = 0; i < static_cast<int>(anims.size()); ++i)
				{
					auto* clip = anims[i].Get();
					if (!clip) continue;

					const std::string clipNameStr = Engine::WStrToStr(clip->GetName());
					const bool isSelected = (i == m_SelectedClipIndex);
					if (ImGui::Selectable(clipNameStr.c_str(), isSelected) && !isSelected)
					{
						// 클립은 저장되는 문서 바인딩이므로 편집 이력과 미저장 표시에 포함된다.
						SelectClip(i);
						m_CurrentTime = 0.0f;
						m_TimelineZoom = 1.0f;
						m_ViewStartTime = 0.0f;
						m_IsDirty = true;
					}
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}
		else
		{
			toolbar.Text(LOCAL_CSTR("Timeline.NoEmbeddedAnimation"), false);
		}
	}
}

void AnimationTimelinePanel::DrawTransportBar()
{
	EditorUI::ToolbarLayout toolbar;
	EditorUI::DrawTimelineAction(toolbar, "Timeline", "FirstFrame", ICON_FA_BACKWARD_FAST, "Action.Timeline.FirstFrame");
	EditorUI::DrawTimelineAction(toolbar, "Timeline", "PrevFrame", ICON_FA_BACKWARD_STEP, "Action.Timeline.PrevFrame");
	EditorUI::DrawTimelineAction(toolbar, "Timeline", "PlayPause", m_IsPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY, "Action.Timeline.PlayPause");
	EditorUI::DrawTimelineAction(toolbar, "Timeline", "NextFrame", ICON_FA_FORWARD_STEP, "Action.Timeline.NextFrame");
	EditorUI::DrawTimelineAction(toolbar, "Timeline", "LastFrame", ICON_FA_FORWARD_FAST, "Action.Timeline.LastFrame");
	EditorUI::DrawTimelineAction(toolbar, "Timeline", "ToggleLoop", ICON_FA_REPEAT, "Action.Timeline.ToggleLoop", m_IsLooping);

	const int currentFrame = static_cast<int>(m_CurrentTime * m_FPS);
	const int totalFrames = static_cast<int>(m_Duration * m_FPS);
	const std::string timeLabel = LOCAL_FMT("Timeline.TimeCounter", m_CurrentTime, m_Duration);
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.5f, 1.0f));
	toolbar.Text(timeLabel.c_str(), false);
	ImGui::PopStyleColor();

	const std::string frameLabel = LOCAL_FMT("Timeline.FrameCounter", currentFrame, totalFrames);
	toolbar.Text(frameLabel.c_str(), false);

	toolbar.SetNextItemWidth(ImGui::GetFontSize() * 4.5f);
	ImGui::SliderFloat("##TransportSpeed", &m_PlaybackSpeed, 0.1f, 2.0f, "%.1fx");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.PlaybackSpeed"));

	const std::string zoomLabel = LOCAL_FMT("Timeline.ZoomFactor", m_TimelineZoom);
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.4f, 1.0f));
	toolbar.Text(zoomLabel.c_str(), false);
	ImGui::PopStyleColor();

	EditorUI::DrawTimelineAction(toolbar, "Timeline", "ZoomReset", ICON_FA_MAXIMIZE, "Action.Timeline.ZoomReset");

	if (m_SelectedClipIndex < 0)
	{
		toolbar.Text(LOCAL_CSTR("Timeline.Duration"), false);
		toolbar.SetNextItemWidth(ImGui::GetFontSize() * 4.8f);
		if (ImGui::DragFloat("##ManualDuration", &m_Duration, 0.05f, 0.2f, 60.0f, "%.1fs"))
		{
			m_Duration = std::max(0.2f, m_Duration);
			m_IsDirty = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.DurationTooltip"));
	}

	// 런타임에서 타임라인이 끝난 뒤 원래 애니메이션으로 섞어 넘어가는 시간. 편집 이력·저장 기준점은 문서 스냅샷 비교로 처리된다.
	toolbar.Text(LOCAL_CSTR("Timeline.BlendOut"), false);
	toolbar.SetNextItemWidth(ImGui::GetFontSize() * 4.8f);
	if (ImGui::DragFloat("##BlendOutTime", &m_BlendOutTime, 0.01f, 0.0f, 2.0f, "%.2fs", ImGuiSliderFlags_AlwaysClamp))
	{
		m_BlendOutTime = std::max(0.0f, m_BlendOutTime);
		m_IsDirty = true;
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.BlendOutTooltip"));
}

void AnimationTimelinePanel::DrawInspector()
{
	ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.3f, 1.0f), ICON_FA_SLIDERS " %s", LOCAL_CSTR("Timeline.NotifyInspector"));
	ImGui::Separator();

	if (m_Selection.Type == ETimelineSelectionType::None)
	{
		ImGui::Spacing();
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextDisabled("%s", LOCAL_CSTR("Timeline.InspectorSelectionHint"));
		ImGui::PopTextWrapPos();
		return;
	}

	if (m_Selection.Type == ETimelineSelectionType::NotifyMarker)
	{
		if (m_Selection.TrackIndex >= 0 && m_Selection.TrackIndex < static_cast<int>(m_NotifyTracks.size()))
		{
			auto& track = m_NotifyTracks[m_Selection.TrackIndex];
			if (m_Selection.ItemIndex >= 0 && m_Selection.ItemIndex < static_cast<int>(track.Notifies.size()))
			{
				DrawNotifyInspector(track.Notifies[m_Selection.ItemIndex]);
			}
		}
	}
	else if (m_Selection.Type == ETimelineSelectionType::CameraShot)
	{
		if (m_Selection.TrackIndex >= 0 && m_Selection.TrackIndex < static_cast<int>(m_CameraTracks.size()))
		{
			DrawCameraShotInspector(m_CameraTracks[m_Selection.TrackIndex]);
		}
	}
}

void AnimationTimelinePanel::DrawNotifyInspector(Engine::AnimNotifyData& notify)
{
	const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
	const auto object = reflection::ObjectView::From(notify);
	const auto* type = registry.FindType(object);
	if (!type)
	{
		EditorUI::EmptyState("Inspector.ValueUnavailable");
		return;
	}

	ReflectionObjectEditSettings headerSettings;
	ReflectionObjectEditSettings detailSettings;
	for (const auto& property : type->Properties)
	{
		const bool header = ContainsName(NotifyHeaderProperties, property.Name);
		if (!header) headerSettings.HiddenProperties.insert(property.Name);
		if (header || ContainsName(NotifyCustomProperties, property.Name)) detailSettings.HiddenProperties.insert(property.Name);
	}

	bool changed = false;
	ImGui::PushID("NotifyHeader");
	changed |= ReflectionPropertyDrawer::DrawPropertyTable(*type, object, &registry, &headerSettings);
	ImGui::PopID();

	if (EditorUI::BeginPropertyTable("NotifyReferences", EditorUI::InspectorPropertyTableStyle()))
	{
		// 소켓 본은 스켈레톤이 있어야 고를 수 있다. 현재 스켈레톤에 없는 본이면 루트에서 발동하므로 경고한다.
		EditorUI::PropertyRow("PROP_SOCKET_BONE");
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::BeginDisabled(m_BoneNames.empty());
		changed |= DrawBonePickerCombo("##SocketBone", notify.SocketBoneName);
		ImGui::EndDisabled();
		if (m_BoneNames.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.SocketNeedsModel"));
		if (IsMissingSocketBone(notify.SocketBoneName))
		{
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "%s", LOCAL_FMT("Timeline.SocketBoneMissing", notify.SocketBoneName).c_str());
			ImGui::PopTextWrapPos();
		}

		const bool isSound = notify.Type == Engine::EAnimNotifyType::Sound;
		if (isSound || notify.Type == Engine::EAnimNotifyType::SpawnCompositeVFX)
		{
			std::string& path = isSound ? notify.SoundPath : notify.VFXPath;
			EditorUI::PropertyRow(isSound ? "PROP_SOUND_PATH" : "PROP_VFX_PATH");
			std::string picked;
			if (Editor::DrawInspectorAssetPathPicker(path, picked, isSound ? NotifySoundPicker : NotifyVFXPicker))
			{
				path = std::move(picked);
				changed = true;
			}
		}
		EditorUI::EndPropertyTable();
	}

	ImGui::PushID("NotifyDetail");
	changed |= ReflectionPropertyDrawer::DrawPropertyTable(*type, object, &registry, &detailSettings);
	ImGui::PopID();

	if (changed)
	{
		// 리플렉션 RANGE는 고정값이므로 클립 길이 상한은 여기서 적용한다.
		notify.TriggerTime = std::clamp(notify.TriggerTime, 0.0f, m_Duration);
		notify.Duration = std::clamp(notify.Duration, 0.0f, std::max(0.0f, m_Duration - notify.TriggerTime));
		m_IsDirty = true;
	}

	ImGui::Spacing();
	EditorUI::ToolbarLayout actions;
	if (actions.Button("TriggerTest", ICON_FA_PLAY, "Timeline.TriggerTest")) TriggerNotify(notify, true);
	ImGui::BeginDisabled(!CanDeleteSelected());
	if (actions.Button("DeleteNotify", ICON_FA_TRASH, "Timeline.DeleteNotify")) DeleteSelected();
	ImGui::EndDisabled();
}

void AnimationTimelinePanel::DrawCameraShotInspector(Engine::TimelineCameraTrack& camShot)
{
	const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
	const auto object = reflection::ObjectView::From(camShot);
	const auto* type = registry.FindType(object);
	if (!type)
	{
		EditorUI::EmptyState("Inspector.ValueUnavailable");
		return;
	}

	// 포커스 본은 본 목록 검색이 필요하므로 공용 표에서 빼고 같은 표 스타일로 직접 그린다.
	ReflectionObjectEditSettings settings;
	settings.HiddenProperties.insert("FocusBoneName");
	bool changed = ReflectionPropertyDrawer::DrawPropertyTable(*type, object, &registry, &settings);

	if (EditorUI::BeginPropertyTable("CameraShotReferences", EditorUI::InspectorPropertyTableStyle()))
	{
		EditorUI::PropertyRow("PROP_FOCUS_BONE");
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::BeginDisabled(m_BoneNames.empty());
		changed |= DrawBonePickerCombo("##FocusBone", camShot.FocusBoneName);
		ImGui::EndDisabled();
		if (m_BoneNames.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.SocketNeedsModel"));
		if (IsMissingSocketBone(camShot.FocusBoneName))
		{
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "%s", LOCAL_FMT("Timeline.SocketBoneMissing", camShot.FocusBoneName).c_str());
			ImGui::PopTextWrapPos();
		}
		EditorUI::EndPropertyTable();
	}

	if (changed)
	{
		// 리플렉션 RANGE는 고정값이므로 클립 길이 상한은 여기서 적용한다.
		camShot.StartTime = std::clamp(camShot.StartTime, 0.0f, m_Duration);
		m_IsDirty = true;
	}

	ImGui::Spacing();
	EditorUI::ToolbarLayout actions;
	ImGui::BeginDisabled(!CanDeleteSelected());
	if (actions.Button("DeleteCameraShot", ICON_FA_TRASH, "EditorCommon.Delete")) DeleteSelected();
	ImGui::EndDisabled();
}

void AnimationTimelinePanel::DrawTimelineTrackArea()
{
	DrawTransportBar();

	// 눈금자와 "트랙" 칸은 이 영역에 고정하고, 트랙 행만 아래 자식 창에서 세로로 스크롤한다 (이펙트 타임라인과 같은 구조).
	ImGui::BeginChild("##TimelineTracksArea", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 areaMin = ImGui::GetCursorScreenPos();
	const ImVec2 areaSize = ImGui::GetContentRegionAvail();
	m_TrackHeight = EditorUI::GetTimelineRowHeight();
	m_TrackAreaLeft = areaMin.x;
	auto& input = Engine::InputManager::Get();
	const auto mousePos = input.GetScreenMousePosition();
	const bool isMouseDown = input.IsMouseButtonPressed(Engine::EMouseButton::Left);
	const bool isMouseClicked = input.IsMouseButtonDown(Engine::EMouseButton::Left);

	// 행 영역은 세로 스크롤바를 항상 표시한다. 눈금자도 스크롤바를 뺀 폭을 써야 눈금과 행 그리드가 어긋나지 않는다.
	const float contentWidth = std::max(1.0f, areaSize.x - ImGui::GetStyle().ScrollbarSize);
	const float rulerHeight = std::min(areaSize.y, m_TrackHeight * 1.25f);
	const float rowsViewHeight = std::max(1.0f, areaSize.y - rulerHeight);
	const float headerWidth = std::clamp(m_TrackHeaderWidth, 1.0f, std::max(1.0f, contentWidth * 0.55f));
	const float timelineWidth = std::max(1.0f, contentWidth - headerWidth);
	const ImVec2 timelineMin(areaMin.x + headerWidth, areaMin.y);
	const ImVec2 timelineMax(timelineMin.x + timelineWidth, areaMin.y + areaSize.y);
	const ImVec2 rulerMin = timelineMin;
	const ImVec2 rulerMax(timelineMax.x, timelineMin.y + rulerHeight);

	// 고정 머리 배경과 "트랙" 칸(+ 버튼). 배경을 먼저 그리고, 버튼은 아래 히트박스들보다 먼저 제출한다.
	drawList->AddRectFilled(areaMin, ImVec2(areaMin.x + areaSize.x, rulerMax.y), EditorUI::TimelineRulerBackground);
	if (EditorUI::DrawTimelineCorner(*drawList, areaMin, ImVec2(timelineMin.x, rulerMax.y), "Timeline.AddTrack"))
	{
		m_AddTrackTime = m_CurrentTime;
		ImGui::OpenPopup("##MultiTrackContextMenu");
	}

	// 이름 칸 폭 스플리터: 눈금자부터 행 영역 끝까지 덮는다. 눈금자와 행 자식 창보다 먼저 제출해야 겹친 자리에서 우선 잡힌다.
	const float splitterHitWidth = Editor::SplitterThickness + Editor::SplitterHoverExtend * 2.0f;
	const ImRect splitterRect(ImVec2(timelineMin.x - splitterHitWidth * 0.5f, areaMin.y), ImVec2(timelineMin.x + splitterHitWidth * 0.5f, timelineMax.y));
	const auto headerSplitter = Editor::SplitterHandle("##HeaderSplitterHitbox", splitterRect, ImGuiAxis_X);
	if (headerSplitter.Held)
	{
		m_TrackHeaderWidth += input.GetMouseDelta().x;
		const float maxHeader = std::max(1.0f, std::min(450.0f, contentWidth * 0.55f));
		m_TrackHeaderWidth = std::clamp(m_TrackHeaderWidth, std::min(90.0f, maxHeader), maxHeader);
	}

	ImGui::SetCursorScreenPos(rulerMin);
	ImGui::InvisibleButton("##TimelineRulerHitbox", ImVec2(timelineWidth, rulerHeight));
	const bool isRulerHovered = ImGui::IsItemHovered();

	// ChildWindows: 아래 행 자식 창 위에 있을 때도 타임라인 영역으로 본다.
	const bool isOverTimelineArea = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
		mousePos.x >= areaMin.x && mousePos.x <= timelineMax.x && mousePos.y >= areaMin.y && mousePos.y <= timelineMax.y;

	if (isOverTimelineArea && !m_IsScrubbing && !m_IsDraggingItem && !headerSplitter.Held && mousePos.x >= timelineMin.x)
	{
		const bool panning = input.IsMouseButtonDragging(Engine::EMouseButton::Middle) ||
			(input.IsAltPressed() && input.IsMouseButtonDragging(Engine::EMouseButton::Right));
		const auto wheel = EditorUI::SplitTimelineWheel(input.GetMouseScrollDeltaY(), input.IsShiftPressed(), input.IsAltPressed());
		const auto scale = EditorUI::MakeTimelineScale(timelineMin.x, timelineWidth, m_Duration, m_TimelineZoom, m_ViewStartTime);
		EditorUI::NavigateTimeline(mousePos.x, wheel.Zoom, wheel.PanPixels + (panning ? input.GetMouseDelta().x : 0.0f),
			scale, std::max(m_Duration, EditorUI::MinimumTimelineDuration), m_TimelineZoom, m_ViewStartTime);
	}

	if (isRulerHovered && isMouseClicked && !m_IsDraggingItem)
	{
		m_IsScrubbing = true;
		m_IsPlaying = false;
	}
	if (m_IsScrubbing)
	{
		if (isMouseDown) m_CurrentTime = ScreenXToTime(mousePos.x, timelineMin.x, timelineWidth);
		else m_IsScrubbing = false;
	}

	// ── 트랙 행: 눈금자 아래에서만 세로로 스크롤한다 ──
	const float curX = TimeToScreenX(m_CurrentTime, timelineMin.x, timelineWidth);
	float rowsEndY = rulerMax.y;
	ImGui::SetCursorScreenPos(ImVec2(areaMin.x, rulerMax.y));
	if (ImGui::BeginChild("##TimelineTrackRows", ImVec2(0.0f, rowsViewHeight), ImGuiChildFlags_None,
		ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImDrawList* rowsDraw = ImGui::GetWindowDrawList();
		// 휠: 막대 칸에서는 줌, 이름 칸이나 Alt를 누른 상태에서는 세로 스크롤 (이펙트 타임라인과 같은 규칙)
		const float wheel = input.GetMouseScrollDeltaY();
		if (wheel != 0.0f && ImGui::IsWindowHovered() && (mousePos.x < timelineMin.x || input.IsAltPressed()))
			ImGui::SetScrollY(ImGui::GetScrollY() - wheel * m_TrackHeight * 3.0f);

		float currentY = ImGui::GetCursorScreenPos().y; // 스크롤만큼 위로 밀린 첫 행의 y

		// 1. 베이스 애니메이션 클립 트랙
		DrawAnimPoseTrack(rowsDraw, ImVec2(timelineMin.x, currentY), ImVec2(timelineMax.x, currentY + m_TrackHeight), timelineWidth);
		currentY += m_TrackHeight;

		// 2. 다중 노티파이 트랙 목록
		for (size_t i = 0; i < m_NotifyTracks.size(); ++i)
		{
			DrawNotifyTrackRow(rowsDraw, static_cast<int>(i), m_NotifyTracks[i], ImVec2(timelineMin.x, currentY), ImVec2(timelineMax.x, currentY + m_TrackHeight), timelineWidth);
			currentY += m_TrackHeight;
		}

		// 3. 다중 카메라 연출 트랙 목록
		for (size_t i = 0; i < m_CameraTracks.size(); ++i)
		{
			DrawCameraTrackRow(rowsDraw, static_cast<int>(i), m_CameraTracks[i], ImVec2(timelineMin.x, currentY), ImVec2(timelineMax.x, currentY + m_TrackHeight), timelineWidth);
			currentY += m_TrackHeight;
		}
		rowsEndY = currentY;

		// 행 영역의 이름 칸 경계선과 재생 헤드 선 (눈금자 높이 부분은 아래에서 고정 영역에 그린다)
		const float viewBottom = rulerMax.y + rowsViewHeight;
		Editor::DrawVerticalSplitterLine(*rowsDraw, headerSplitter, timelineMin.x, rulerMax.y, viewBottom);
		if (curX >= timelineMin.x && curX <= timelineMax.x)
			rowsDraw->AddLine(ImVec2(curX, rulerMax.y), ImVec2(curX, std::min(rowsEndY, viewBottom)), EditorUI::TimelinePlayheadColor, 2.0f);
	}
	ImGui::EndChild();

	// ── 고정 머리: "트랙" 칸, 눈금자, 재생 헤드 삼각형 ──
	EditorUI::DrawTimelineRuler(*drawList, EditorUI::MakeTimelineScale(rulerMin.x, timelineWidth, m_Duration, m_TimelineZoom, m_ViewStartTime), rulerMin.y, rulerMax.y, m_FPS);
	drawList->AddLine(ImVec2(areaMin.x, rulerMax.y), ImVec2(areaMin.x + areaSize.x, rulerMax.y), EditorUI::TimelineRowSeparator);
	Editor::DrawVerticalSplitterLine(*drawList, headerSplitter, timelineMin.x, areaMin.y, rulerMax.y);
	EditorUI::DrawTimelinePlayhead(*drawList, rulerMin, rulerMax, curX);

	const bool isRulerRightClicked = (isRulerHovered && input.IsMouseButtonDown(Engine::EMouseButton::Right));
	const bool isEmptyAreaRightClicked = isOverTimelineArea && mousePos.y > rowsEndY && input.IsMouseButtonDown(Engine::EMouseButton::Right);

	if (isRulerRightClicked || isEmptyAreaRightClicked)
	{
		m_AddTrackTime = std::clamp(ScreenXToTime(mousePos.x, timelineMin.x, timelineWidth), 0.0f, m_Duration);
		ImGui::OpenPopup("##MultiTrackContextMenu");
	}
	if (ImGui::BeginPopup("##MultiTrackContextMenu"))
	{
		if (ImGui::MenuItem((ICON_FA_BURST " " + LOCAL_LABEL("Timeline.AddFXTrack")).c_str()))
		{
			Engine::TimelineNotifyTrack newTrack{};
			newTrack.TrackName = "FX_Track_" + std::to_string(m_NotifyTracks.size() + 1);
			newTrack.Kind = Engine::ETimelineNotifyTrackKind::Effect;   // FX 항목
			m_NotifyTracks.push_back(newTrack);
			m_IsDirty = true;
		}
		if (ImGui::MenuItem((ICON_FA_MUSIC " " + LOCAL_LABEL("Timeline.AddAudioTrack")).c_str()))
		{
			Engine::TimelineNotifyTrack newTrack{};
			newTrack.TrackName = "Audio_Track_" + std::to_string(m_NotifyTracks.size() + 1);
			newTrack.Kind = Engine::ETimelineNotifyTrackKind::Audio;    // 오디오 항목
			m_NotifyTracks.push_back(newTrack);
			m_IsDirty = true;
		}
		if (ImGui::MenuItem((ICON_FA_BELL " " + LOCAL_LABEL("Timeline.AddEventTrack")).c_str()))
		{
			Engine::TimelineNotifyTrack newTrack{};
			newTrack.TrackName = "Event_Track_" + std::to_string(m_NotifyTracks.size() + 1);
			newTrack.Kind = Engine::ETimelineNotifyTrackKind::Event;    // 이벤트 항목
			m_NotifyTracks.push_back(newTrack);
			m_IsDirty = true;
		}
		if (ImGui::MenuItem((ICON_FA_VIDEO " " + LOCAL_LABEL("Timeline.AddCameraShotTrack")).c_str()))
		{
			Engine::TimelineCameraTrack newCam{};
			newCam.TrackName = "Cam_Shot_" + std::to_string(m_CameraTracks.size() + 1);
			newCam.StartTime = m_AddTrackTime;
			newCam.Duration = 0.5f;
			newCam.TargetFOV = 35.0f;
			m_CameraTracks.push_back(newCam);
			m_IsDirty = true;
		}

		ImGui::Separator();
		ACTION_MENU_ITEM((ICON_FA_MAXIMIZE " " + LOCAL_LABEL("Action.Timeline.ZoomReset")).c_str(), "Timeline.ZoomReset");
		ImGui::EndPopup();
	}

	ImGui::EndChild();
}

void AnimationTimelinePanel::DrawAnimPoseTrack(ImDrawList* drawList, const ImVec2& trackMin, const ImVec2& trackMax, float timelineWidth)
{
	const ImVec2 headerMin(m_TrackAreaLeft, trackMin.y);
	const auto scale = EditorUI::MakeTimelineScale(trackMin.x, timelineWidth, m_Duration, m_TimelineZoom, m_ViewStartTime);
	EditorUI::DrawTimelineRowBackground(*drawList, headerMin.x, scale, trackMin.y, trackMax.y, 0, false);
	EditorUI::DrawTimelineRowLabel(*drawList, headerMin.x, trackMin.x, trackMin.y, trackMax.y,
		ICON_FA_PERSON_RUNNING, IM_COL32(200, 210, 225, 255), LOCAL("Timeline.Pose"), EditorUI::TimelineLabelState::Normal);

	ImGui::SetCursorScreenPos(trackMin);
	ImGui::InvisibleButton("##AnimPoseTrackHitbox", ImVec2(timelineWidth, m_TrackHeight));

	drawList->PushClipRect(trackMin, trackMax, true);
	EditorUI::DrawTimelineGrid(*drawList, scale, trackMin.y, trackMax.y, m_FPS);
	const float inset = EditorUI::GetTimelineBarInset();
	const ImVec2 clipMin(scale.ToX(0.0f), trackMin.y + inset);
	const ImVec2 clipMax(std::max(clipMin.x + 1.0f, scale.ToX(m_Duration)), trackMax.y - inset);
	EditorUI::DrawTimelineBar(*drawList, clipMin, clipMax, IM_COL32(45, 95, 160, 220), false, false);
	const float textY = trackMin.y + (m_TrackHeight - ImGui::GetFontSize()) * 0.5f;
	// 클립 이름은 저장값이고, 비어 있으면 상황에 맞는 안내 문구를 표시만 한다.
	const std::string clipLabel = !m_CurrentClipName.empty() ? m_CurrentClipName
		: LOCAL(m_TargetModel.Get() ? "Timeline.NoEmbeddedAnimation" : "Timeline.NoModel");
	drawList->AddText(ImVec2(std::max(trackMin.x + 8.0f, clipMin.x + 12.0f), textY), IM_COL32(255, 255, 255, 255), clipLabel.c_str());
	drawList->PopClipRect();
}

void AnimationTimelinePanel::AddNotifyAt(int trackIdx, Engine::EAnimNotifyType type, float time)
{
	auto& notifies = m_NotifyTracks[trackIdx].Notifies;
	Engine::AnimNotifyData notify{};
	notify.TriggerTime = std::clamp(time, 0.0f, m_Duration);
	notify.Type = type;
	switch (type)
	{
	case Engine::EAnimNotifyType::Sound: notify.Name = "Sound_Cue"; break;
	case Engine::EAnimNotifyType::CustomEvent: notify.Name = "Gameplay_Event"; break;
	default: notify.Name = "VFX_Slash"; notify.AttachToSocket = true; break;
	}
	notifies.push_back(notify);
	m_Selection = { ETimelineSelectionType::NotifyMarker, trackIdx, static_cast<int>(notifies.size()) - 1 };
	m_IsDirty = true;
}

void AnimationTimelinePanel::DrawNotifyTrackRow(ImDrawList* drawList, int trackIdx, Engine::TimelineNotifyTrack& track, const ImVec2& trackMin, const ImVec2& trackMax, float timelineWidth)
{
	const ImVec2 headerMin(m_TrackAreaLeft, trackMin.y);
	const ImVec2 headerMax(trackMin.x, trackMax.y);
	const auto scale = EditorUI::MakeTimelineScale(trackMin.x, timelineWidth, m_Duration, m_TimelineZoom, m_ViewStartTime);
	const bool rowSelected = m_Selection.Type == ETimelineSelectionType::NotifyMarker && m_Selection.TrackIndex == trackIdx;
	EditorUI::DrawTimelineRowBackground(*drawList, headerMin.x, scale, trackMin.y, trackMax.y, static_cast<std::size_t>(trackIdx) + 1, rowSelected);

	const float headerCenterY = (headerMin.y + headerMax.y) * 0.5f;
	auto& input = Engine::InputManager::Get();
	const auto mousePos = input.GetScreenMousePosition();
	const bool isMouseDown = input.IsMouseButtonPressed(Engine::EMouseButton::Left);
	const bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	const bool isMouseClicked = windowHovered && input.IsMouseButtonDown(Engine::EMouseButton::Left);

	// 트랙 종류 아이콘 하나만 표시한다. 음소거는 우클릭 메뉴에서 바꾸고, 음소거 상태는 흐린 색으로 나타낸다.
	const auto trackVisual = GetNotifyTrackVisual(track);
	const auto labelState = rowSelected ? EditorUI::TimelineLabelState::Selected
		: (track.IsMuted ? EditorUI::TimelineLabelState::Disabled : EditorUI::TimelineLabelState::Normal);
	EditorUI::DrawTimelineRowLabel(*drawList, headerMin.x, headerMax.x, headerMin.y, headerMax.y, trackVisual.Icon, trackVisual.Color, track.TrackName, labelState);

	if (m_RenamingTrackIndex == trackIdx)
	{
		// 입력창 글자가 표시 중인 이름과 같은 위치에서 시작하도록 프레임 안쪽 여백만큼 당긴다.
		const float nameX = headerMin.x + EditorUI::GetTimelineLabelTextOffset() - ImGui::GetStyle().FramePadding.x;
		ImGui::SetCursorScreenPos(ImVec2(nameX, headerCenterY - ImGui::GetFrameHeight() * 0.5f));
		ImGui::SetNextItemWidth(std::max(40.0f, headerMax.x - nameX - 4.0f));
		if (ImGui::InputText("##RenameTrack", m_RenameBuf, sizeof(m_RenameBuf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
		{
			track.TrackName = m_RenameBuf;
			m_RenamingTrackIndex = -1;
			m_IsDirty = true;
		}
		if (!ImGui::IsItemActive() && isMouseClicked) m_RenamingTrackIndex = -1;
	}

	const bool isHeaderHovered = (mousePos.x >= headerMin.x && mousePos.x <= headerMax.x && mousePos.y >= headerMin.y && mousePos.y <= headerMax.y && windowHovered);
	if (isHeaderHovered && input.IsMouseButtonDown(Engine::EMouseButton::Right))
	{
		ImGui::OpenPopup(("##HeaderPopup_" + std::to_string(trackIdx)).c_str());
	}
	if (ImGui::BeginPopup(("##HeaderPopup_" + std::to_string(trackIdx)).c_str()))
	{
		if (ImGui::MenuItem((ICON_FA_PEN " " + LOCAL_LABEL("EditorCommon.Rename")).c_str()))
		{
			m_RenamingTrackIndex = trackIdx;
			snprintf(m_RenameBuf, sizeof(m_RenameBuf), "%s", track.TrackName.c_str());
		}
		const char* muteMenuKey = track.IsMuted ? "Timeline.UnmuteTrack" : "Timeline.MuteTrack";
		const char* muteMenuIcon = track.IsMuted ? ICON_FA_VOLUME_HIGH : ICON_FA_VOLUME_XMARK;
		const std::string muteMenuLabel = std::string(muteMenuIcon) + " " + LOCAL(muteMenuKey) + "###ToggleTrackMute";
		if (ImGui::MenuItem(muteMenuLabel.c_str()))
		{
			track.IsMuted = !track.IsMuted;
			m_IsDirty = true;
		}
		if (ImGui::MenuItem((ICON_FA_COPY " " + LOCAL_LABEL("Hierarchy.Duplicate")).c_str()))
		{
			Engine::TimelineNotifyTrack copyTrack = track;
			copyTrack.TrackName += "_Copy";
			m_NotifyTracks.push_back(copyTrack);
			m_IsDirty = true;
			ImGui::EndPopup();
			return;
		}
		ImGui::Separator();
		if (ImGui::MenuItem((ICON_FA_TRASH " " + LOCAL_LABEL("EditorCommon.Delete")).c_str()))
		{
			m_NotifyTracks.erase(m_NotifyTracks.begin() + trackIdx);
			m_Selection = {};
			m_IsDirty = true;
			ImGui::EndPopup();
			return;
		}
		ImGui::EndPopup();
	}

	ImGui::SetCursorScreenPos(trackMin);
	const std::string trackHitboxId = "##NotifyTrackHitbox_" + std::to_string(trackIdx);
	ImGui::InvisibleButton(trackHitboxId.c_str(), ImVec2(timelineWidth, m_TrackHeight), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	const bool isTrackHovered = ImGui::IsItemHovered();

	drawList->PushClipRect(trackMin, trackMax, true);
	EditorUI::DrawTimelineGrid(*drawList, scale, trackMin.y, trackMax.y, m_FPS);
	const float centerY = (trackMin.y + trackMax.y) * 0.5f;
	const float markerHalfSize = EditorUI::GetTimelineMarkerHalfSize(); // 마우스 판정과 그리기 크기를 일치시킨다
	if (!isMouseDown)
	{
		m_IsDraggingItem = false;
		m_DragTimeOffset = 0.0f;
		m_ItemDragMode = ETimelineItemDrag::Move;
	}

	bool anyMarkerHovered = false;
	int hoveredNotifyIndex = -1;
	const float edgeHalfWidth = ImGui::GetFontSize() * 0.4f; // 이펙트 타임라인의 끝점 판정 폭과 같다
	const float barInset = EditorUI::GetTimelineBarInset();
	for (size_t nIdx = 0; nIdx < track.Notifies.size(); ++nIdx)
	{
		auto& notify = track.Notifies[nIdx];
		const bool isStateNotify = notify.Type == Engine::EAnimNotifyType::SpawnCompositeVFX && notify.Duration > 0.0f;
		const float markerX = TimeToScreenX(notify.TriggerTime, trackMin.x, timelineWidth);
		const float endX = isStateNotify ? TimeToScreenX(notify.TriggerTime + notify.Duration, trackMin.x, timelineWidth) : markerX;
		if (endX < trackMin.x - 15.0f || markerX > trackMin.x + timelineWidth + 15.0f) continue;

		// 판정 우선순위: 끝점(길이 조절) → 다이아몬드 → 막대 몸통. 몸통과 다이아몬드는 전체 이동이다.
		const bool inBarRows = isTrackHovered && mousePos.y >= trackMin.y + barInset && mousePos.y <= trackMax.y - barInset;
		const bool isEndHovered = isStateNotify && inBarRows && std::abs(mousePos.x - endX) <= edgeHalfWidth;
		const bool isDiamondHovered = isTrackHovered && std::abs(mousePos.x - markerX) <= markerHalfSize + 2.5f && std::abs(mousePos.y - centerY) <= markerHalfSize + 2.5f;
		const bool isBarHovered = isStateNotify && inBarRows && mousePos.x >= markerX && mousePos.x <= endX;
		const bool isMarkerHovered = isEndHovered || isDiamondHovered || isBarHovered;
		if (isMarkerHovered) { anyMarkerHovered = true; hoveredNotifyIndex = static_cast<int>(nIdx); }
		const bool isSelected = (m_Selection.Type == ETimelineSelectionType::NotifyMarker && m_Selection.TrackIndex == trackIdx && m_Selection.ItemIndex == static_cast<int>(nIdx));

		if (isMarkerHovered && isMouseClicked)
		{
			m_Selection = { ETimelineSelectionType::NotifyMarker, trackIdx, static_cast<int>(nIdx) };
			m_IsDraggingItem = true;
			m_ItemDragMode = isEndHovered ? ETimelineItemDrag::ResizeEnd : ETimelineItemDrag::Move;
			const float clickTime = ScreenXToTime(mousePos.x, trackMin.x, timelineWidth);
			m_DragTimeOffset = clickTime - (isEndHovered ? notify.TriggerTime + notify.Duration : notify.TriggerTime);
		}

		const bool isResizing = m_IsDraggingItem && isSelected && m_ItemDragMode == ETimelineItemDrag::ResizeEnd;
		if (m_IsDraggingItem && isSelected && isMouseDown)
		{
			const float target = ScreenXToTime(mousePos.x, trackMin.x, timelineWidth) - m_DragTimeOffset;
			if (isResizing && isStateNotify)
			{
				const float maxDuration = std::max(EditorUI::MinimumTimelineDuration, m_Duration - notify.TriggerTime);
				notify.Duration = std::clamp(target - notify.TriggerTime, EditorUI::MinimumTimelineDuration, maxDuration);
			}
			else
			{
				// 지속형은 끝이 클립 밖으로 나가지 않도록 길이만큼 이동 범위를 줄인다.
				const float occupied = isStateNotify ? notify.Duration : 0.0f;
				notify.TriggerTime = std::clamp(target, 0.0f, std::max(0.0f, m_Duration - occupied));
			}
			m_IsDirty = true;
		}
		if (isEndHovered || isResizing) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

		ImU32 markerColor = IM_COL32(255, 160, 40, 240);
		if (notify.Type == Engine::EAnimNotifyType::Sound) markerColor = IM_COL32(60, 200, 240, 240);
		else if (notify.Type == Engine::EAnimNotifyType::CustomEvent) markerColor = IM_COL32(190, 100, 255, 240);
		else if (notify.Type == Engine::EAnimNotifyType::SpawnCompositeVFX) markerColor = IM_COL32(255, 60, 140, 240);

		if (isStateNotify)
		{
			// 유지 구간은 마커 색의 반투명 막대로 그리고, 조절 가능한 끝점에만 핸들을 표시한다.
			const ImVec2 barMin(markerX, trackMin.y + barInset);
			const ImVec2 barMax(std::max(markerX + 1.0f, endX), trackMax.y - barInset);
			const ImU32 barColor = (markerColor & ~IM_COL32_A_MASK) | (static_cast<ImU32>(isSelected ? 150 : 95) << IM_COL32_A_SHIFT);
			EditorUI::DrawTimelineBar(*drawList, barMin, barMax, barColor, isSelected, false);
			if (isSelected || isEndHovered)
				drawList->AddLine(ImVec2(barMax.x - 2.0f, barMin.y), ImVec2(barMax.x - 2.0f, barMax.y), IM_COL32_WHITE, 2.0f);
		}
		EditorUI::DrawTimelineMarker(*drawList, ImVec2(markerX, centerY), markerColor, isSelected);

		if (isMarkerHovered)
		{
			ImGui::BeginTooltip();
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[%s]", notify.Name.c_str());
			ImGui::Text(LOCAL_CSTR("Timeline.MarkerTimeSocket"), notify.TriggerTime, notify.SocketBoneName.empty() ? LOCAL_CSTR("Timeline.Root") : notify.SocketBoneName.c_str());
			if (isStateNotify)
				ImGui::TextUnformatted(LOCAL_FMT("Timeline.MarkerDuration", notify.Duration).c_str());
			if (notify.Type == Engine::EAnimNotifyType::Sound && !notify.SoundPath.empty())
			{
				ImGui::TextDisabled(LOCAL_CSTR("Timeline.Sound"), Engine::WStrToStr(std::filesystem::path(Engine::StrToWStr(notify.SoundPath)).filename().wstring()).c_str());
			}
			else if (notify.Type == Engine::EAnimNotifyType::SpawnCompositeVFX && !notify.VFXPath.empty())
			{
				const auto vfxPath = std::filesystem::path(Engine::StrToWStr(notify.VFXPath));
				const auto vfxName = Engine::WStrToStr(vfxPath.filename().wstring());
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.7f, 1.0f), "%s", LOCAL_FMT("Timeline.VFXFile", vfxName).c_str());
			}
			ImGui::EndTooltip();
		}
	}
	drawList->PopClipRect();

	if (isTrackHovered && !anyMarkerHovered && isMouseClicked)
	{
		m_IsDraggingItem = false;
		m_DragTimeOffset = 0.0f;
		m_Selection = {};
	}

	if (isTrackHovered && !anyMarkerHovered && input.IsMouseButtonDoubleClicked(Engine::EMouseButton::Left))
	{
		AddNotifyAt(trackIdx, GetNotifyTrackVisual(track).DefaultType, ScreenXToTime(mousePos.x, trackMin.x, timelineWidth));
	}

	// 19-5절: 우클릭은 컨텍스트 메뉴. 마커 위에서 열면 그 마커를 선택하고 항목 명령도 보여준다.
	const std::string bodyPopupId = "##BodyPopup_" + std::to_string(trackIdx);
	if (isTrackHovered && input.IsMouseButtonDown(Engine::EMouseButton::Right))
	{
		m_AddTrackTime = std::clamp(ScreenXToTime(mousePos.x, trackMin.x, timelineWidth), 0.0f, m_Duration);
		m_NotifyContextOnItem = hoveredNotifyIndex >= 0;
		if (m_NotifyContextOnItem) m_Selection = { ETimelineSelectionType::NotifyMarker, trackIdx, hoveredNotifyIndex };
		ImGui::OpenPopup(bodyPopupId.c_str());
	}
	if (ImGui::BeginPopup(bodyPopupId.c_str()))
	{
		if (ImGui::MenuItem((ICON_FA_WAND_MAGIC_SPARKLES " " + LOCAL_LABEL("Timeline.AddVFXNotify")).c_str())) AddNotifyAt(trackIdx, Engine::EAnimNotifyType::SpawnCompositeVFX, m_AddTrackTime);
		if (ImGui::MenuItem((ICON_FA_MUSIC " " + LOCAL_LABEL("Timeline.AddSoundNotify")).c_str())) AddNotifyAt(trackIdx, Engine::EAnimNotifyType::Sound, m_AddTrackTime);
		if (ImGui::MenuItem((ICON_FA_BELL " " + LOCAL_LABEL("Timeline.AddEventNotify")).c_str())) AddNotifyAt(trackIdx, Engine::EAnimNotifyType::CustomEvent, m_AddTrackTime);
		if (m_NotifyContextOnItem)
		{
			auto* selected = GetSelectedNotify();
			ImGui::Separator();
			if (ImGui::MenuItem((ICON_FA_PLAY " " + LOCAL_LABEL("Timeline.TriggerTest")).c_str(), nullptr, false, selected != nullptr) && selected) TriggerNotify(*selected, true);
			if (ImGui::MenuItem((ICON_FA_TRASH " " + LOCAL_LABEL("Timeline.DeleteNotify")).c_str(), nullptr, false, CanDeleteSelected())) DeleteSelected();
		}
		ImGui::EndPopup();
	}
}

void AnimationTimelinePanel::DrawCameraTrackRow(ImDrawList* drawList, int camIdx, Engine::TimelineCameraTrack& camTrack, const ImVec2& trackMin, const ImVec2& trackMax, float timelineWidth)
{
	const ImVec2 headerMin(m_TrackAreaLeft, trackMin.y);
	const ImVec2 headerMax(trackMin.x, trackMax.y);
	const auto scale = EditorUI::MakeTimelineScale(trackMin.x, timelineWidth, m_Duration, m_TimelineZoom, m_ViewStartTime);
	const bool rowSelected = m_Selection.Type == ETimelineSelectionType::CameraShot && m_Selection.TrackIndex == camIdx;
	EditorUI::DrawTimelineRowBackground(*drawList, headerMin.x, scale, trackMin.y, trackMax.y,
		m_NotifyTracks.size() + static_cast<std::size_t>(camIdx) + 1, rowSelected);

	const float headerCenterY = (headerMin.y + headerMax.y) * 0.5f;
	auto& input = Engine::InputManager::Get();
	const auto mousePos = input.GetScreenMousePosition();
	const bool isMouseDown = input.IsMouseButtonPressed(Engine::EMouseButton::Left);
	const bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	const bool isMouseClicked = windowHovered && input.IsMouseButtonDown(Engine::EMouseButton::Left);

	// 카메라 아이콘 하나만 표시한다. 활성 여부는 우클릭 메뉴에서 바꾸고, 비활성 샷은 흐린 색으로 나타낸다.
	const auto labelState = rowSelected ? EditorUI::TimelineLabelState::Selected
		: (camTrack.IsActive ? EditorUI::TimelineLabelState::Normal : EditorUI::TimelineLabelState::Disabled);
	EditorUI::DrawTimelineRowLabel(*drawList, headerMin.x, headerMax.x, headerMin.y, headerMax.y, ICON_FA_VIDEO, IM_COL32(200, 130, 255, 255), camTrack.TrackName, labelState);

	const bool isHeaderHovered = (mousePos.x >= headerMin.x && mousePos.x <= headerMax.x && mousePos.y >= headerMin.y && mousePos.y <= headerMax.y && windowHovered);
	if (isHeaderHovered && input.IsMouseButtonDown(Engine::EMouseButton::Right))
	{
		ImGui::OpenPopup(("##CamHeaderPopup_" + std::to_string(camIdx)).c_str());
	}
	if (ImGui::BeginPopup(("##CamHeaderPopup_" + std::to_string(camIdx)).c_str()))
	{
		const char* activeMenuKey = camTrack.IsActive ? "Timeline.DeactivateShot" : "Timeline.ActivateShot";
		const char* activeMenuIcon = camTrack.IsActive ? ICON_FA_EYE_SLASH : ICON_FA_EYE;
		const std::string activeMenuLabel = std::string(activeMenuIcon) + " " + LOCAL(activeMenuKey) + "###ToggleShotActive";
		if (ImGui::MenuItem(activeMenuLabel.c_str()))
		{
			camTrack.IsActive = !camTrack.IsActive;
			m_IsDirty = true;
		}
		ImGui::Separator();
		if (ImGui::MenuItem((ICON_FA_TRASH " " + LOCAL_LABEL("EditorCommon.Delete")).c_str()))
		{
			m_CameraTracks.erase(m_CameraTracks.begin() + camIdx);
			m_Selection = {};
			m_IsDirty = true;
			ImGui::EndPopup();
			return;
		}
		ImGui::EndPopup();
	}

	ImGui::SetCursorScreenPos(trackMin);
	const std::string camHitboxId = "##CamTrackHitbox_" + std::to_string(camIdx);
	ImGui::InvisibleButton(camHitboxId.c_str(), ImVec2(timelineWidth, m_TrackHeight), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	const bool isTrackHovered = ImGui::IsItemHovered();

	drawList->PushClipRect(trackMin, trackMax, true);
	EditorUI::DrawTimelineGrid(*drawList, scale, trackMin.y, trackMax.y, m_FPS);

	const float shotStartX = TimeToScreenX(camTrack.StartTime, trackMin.x, timelineWidth);
	const float shotEndX = TimeToScreenX(camTrack.StartTime + camTrack.Duration, trackMin.x, timelineWidth);
	const float inset = EditorUI::GetTimelineBarInset();
	const ImVec2 shotMin(shotStartX, trackMin.y + inset);
	const ImVec2 shotMax(shotEndX, trackMax.y - inset);

	const bool isSelected = (m_Selection.Type == ETimelineSelectionType::CameraShot && m_Selection.TrackIndex == camIdx);
	const bool isShotHovered = isTrackHovered && (mousePos.x >= shotMin.x && mousePos.x <= shotMax.x && mousePos.y >= shotMin.y && mousePos.y <= shotMax.y);

	EditorUI::DrawTimelineBar(*drawList, shotMin, ImVec2(std::max(shotMin.x + 1.0f, shotMax.x), shotMax.y),
		isSelected ? IM_COL32(160, 70, 220, 230) : IM_COL32(110, 45, 160, 210), isSelected, isSelected);

	const std::string shotLabel = std::string(ICON_FA_VIDEO " ") + LOCAL("Timeline.Shot");
	const ImVec2 labelSize = ImGui::CalcTextSize(shotLabel.c_str());
	const float shotCenterY = (shotMin.y + shotMax.y) * 0.5f;
	const float labelPosY = shotCenterY - labelSize.y * 0.5f - 1.0f;

	if (shotMax.x - shotMin.x > labelSize.x + 12.0f)
	{
		drawList->AddText(ImVec2(shotMin.x + 8.0f, labelPosY), IM_COL32(255, 255, 255, 245), shotLabel.c_str());
	}
	drawList->PopClipRect();

	if (isShotHovered && isMouseClicked)
	{
		m_Selection = { ETimelineSelectionType::CameraShot, camIdx, 0 };
		m_IsDraggingItem = true;
		const float clickTime = ScreenXToTime(mousePos.x, trackMin.x, timelineWidth);
		m_DragTimeOffset = clickTime - camTrack.StartTime;
	}

	if (m_IsDraggingItem && isSelected && isMouseDown)
	{
		const float curMouseTime = ScreenXToTime(mousePos.x, trackMin.x, timelineWidth);
		camTrack.StartTime = std::clamp(curMouseTime - m_DragTimeOffset, 0.0f, std::max(0.0f, m_Duration - camTrack.Duration));
		m_IsDirty = true;
	}

	if (isTrackHovered && !isShotHovered && isMouseClicked)
	{
		m_IsDraggingItem = false;
		m_DragTimeOffset = 0.0f;
		m_Selection = {};
	}
}

void AnimationTimelinePanel::CaptureState(EditorWorkspace& workspace) const
{
	IImGui::CaptureState(workspace);
	workspace.SetSplitter("Timeline.LeftPanelWidth", m_LeftPanelWidth);
	workspace.SetSplitter("Timeline.InspectorWidth", m_InspectorWidth);
	workspace.SetSplitter("Timeline.TimelineHeight", m_TimelineHeight);
	workspace.SetSplitter("Timeline.TrackHeaderWidth", m_TrackHeaderWidth);
}

void AnimationTimelinePanel::RestoreState(const EditorWorkspace& workspace)
{
	IImGui::RestoreState(workspace);
	m_LeftPanelWidth = workspace.GetSplitter("Timeline.LeftPanelWidth", 220.0f);
	m_InspectorWidth = workspace.GetSplitter("Timeline.InspectorWidth", 280.0f);
	m_TimelineHeight = workspace.GetSplitter("Timeline.TimelineHeight", 220.0f);
	m_TrackHeaderWidth = workspace.GetSplitter("Timeline.TrackHeaderWidth", 180.0f);
}