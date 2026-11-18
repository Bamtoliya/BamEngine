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

AnimationTimelinePanel::AnimationTimelinePanel()
{
	s_ActivePanel = this;
	m_Name = L"AnimationTimelinePanel";
	m_DisplayNameKey = "Timeline.Title";
	Close();

	// 엔진 표준 뷰포트 위젯 초기화 (그리드, 스튜디오 조명, 카메라 컨트롤러 내장)
	m_PreviewWidget.Initialize(L"TimelinePreview", 512, 512, true);
	m_PreviewWidget.SetupDefaultStudioLighting();
	m_PreviewWidget.FocusTarget(vec3(0.0f, 0.9f, 0.0f), 3.2f);

	// 기본 다중 트랙 프리셋 구성 (FX 트랙 1개 + 카메라 샷 트랙 1개)
	Engine::TimelineNotifyTrack fxTrack{};
	fxTrack.TrackName = "FX_Weapon";
	Engine::AnimNotifyData defaultNotify{};
	defaultNotify.Name = "Slash_Burst";
	defaultNotify.TriggerTime = 0.40f;
	defaultNotify.Type = Engine::EAnimNotifyType::SpawnCompositeVFX;
	defaultNotify.AttachToSocket = true;
	defaultNotify.SocketBoneName = "Weapon_Socket";
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

bool AnimationTimelinePanel::CanDeleteSelected() const
{
	if (!m_Open || m_IsDraggingItem || m_IsScrubbing || m_Selection.TrackIndex < 0) return false;

	if (m_Selection.Type == ETimelineSelectionType::NotifyMarker)
	{
		if (m_Selection.TrackIndex >= static_cast<int>(m_NotifyTracks.size())) return false;
		const auto& notifies = m_NotifyTracks[m_Selection.TrackIndex].Notifies;
		return m_Selection.ItemIndex >= 0 && m_Selection.ItemIndex < static_cast<int>(notifies.size());
	}

	return m_Selection.Type == ETimelineSelectionType::CameraShot && m_Selection.TrackIndex < static_cast<int>(m_CameraTracks.size());
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

void AnimationTimelinePanel::ClearPreviewVFX()
{
	auto* scene = m_PreviewWidget.GetScene();
	if (!scene)
	{
		m_PreviewVFXEntities.clear();
		return;
	}
	auto& reg = scene->GetRegistry();
	for (auto e : m_PreviewVFXEntities)
	{
		if (reg.valid(e)) reg.destroy(e);
	}
	m_PreviewVFXEntities.clear();
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
	ClearPreviewVFX();
	SetupPreviewModel();

	auto* m = m_TargetModel.Get();
	if (!m) return;

	auto anims = m->GetAnimations();
	if (!anims.empty())
	{
		m_SelectedClipIndex = 0;
		auto* clip = anims[0].Get();
		if (clip)
		{
			m_CurrentClipName = Engine::WStrToStr(clip->GetName());
			const f64 tps = clip->GetTicksPerSecond() > 0.0 ? clip->GetTicksPerSecond() : 25.0;
			m_Duration = clip->GetDuration() > 0.0 ? static_cast<f32>(clip->GetDuration() / tps) : 1.0f;
		}
	}
	else
	{
		m_SelectedClipIndex = -1;
		m_CurrentClipName = LOCAL("Timeline.NoEmbeddedAnimation");
		if (m_Duration <= 0.0f) m_Duration = 2.0f;
	}

	m_CurrentTime = 0.0f;
	m_PrevTime = 0.0f;
	m_TimelineZoom = 1.0f;
	m_ViewStartTime = 0.0f;
	m_IsPlaying = false;
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
	if (m_TargetModel.Get() && relocation.Affects(m_TargetModel->GetPath())) return false;
	for (const auto& track : m_NotifyTracks)
		for (const auto& notify : track.Notifies)
			if (relocation.Affects(Engine::Utf8ToPath(notify.SoundPath)) ||
				relocation.Affects(Engine::Utf8ToPath(notify.VFXPath)))
				return false;
	relocation.AddUtf8(m_CurrentAssetPath);
	return true;
}

bool AnimationTimelinePanel::SaveToFile(const std::string& filePath)
{
	auto* asset = Engine::ActionTimelineAsset::CreateEmpty();
	if (!asset) return false;

	std::string modelPathStr = "";
	if (m_TargetModel.IsValid() && m_TargetModel.Get())
	{
		modelPathStr = Engine::WStrToStr(m_TargetModel->GetPath());
	}
	asset->SetModelPath(modelPathStr);
	asset->SetClipName(m_CurrentClipName);
	asset->SetDuration(m_Duration);
	asset->GetNotifyTracks() = m_NotifyTracks;
	asset->GetCameraTracks() = m_CameraTracks;

	const bool success = asset->SaveToFile(filePath);
	Engine::ResourceManager::Get().DestroyResource(asset);

	if (success)
	{
		m_CurrentAssetPath = filePath;
		m_IsDirty = false;
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

	if (!asset->GetModelPath().empty())
	{
		auto& rm = Engine::ResourceManager::Get();
		const auto handle = rm.LoadFile(Engine::StrToWStr(asset->GetModelPath()));
		if (handle.IsValid())
		{
			SetTargetModel(Engine::ResourceHandle<Engine::Model>(handle));
		}
	}

	m_CurrentClipName = asset->GetClipName();
	m_Duration = std::max(0.1f, asset->GetDuration());
	m_NotifyTracks = asset->GetNotifyTracks();
	m_CameraTracks = asset->GetCameraTracks();

	if (m_TargetModel.IsValid() && m_TargetModel.Get())
	{
		const auto anims = m_TargetModel->GetAnimations();
		for (size_t i = 0; i < anims.size(); ++i)
		{
			if (anims[i].IsValid() && Engine::WStrToStr(anims[i]->GetName()) == m_CurrentClipName)
			{
				m_SelectedClipIndex = static_cast<int>(i);
				break;
			}
		}
	}

	Engine::ResourceManager::Get().DestroyResource(asset);
	m_CurrentAssetPath = filePath;
	m_Selection = {};
	m_IsDirty = false;
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

void AnimationTimelinePanel::TriggerNotify(const Engine::AnimNotifyData& notify)
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
				vfxComp.playRate = notify.VFXPlayRate;
				vfxComp.Restart();

				if (notify.AttachToSocket)
				{
					auto& attach = reg.emplace<Engine::SocketAttachmentComponent>(e);
					attach.targetEntity = entt::null;
					attach.boneName = notify.SocketBoneName;
					attach.offsetPosition = notify.OffsetPosition;
					attach.offsetRotation = notify.OffsetRotation;
					attach.scale = notify.VFXScale;
				}

				m_PreviewVFXEntities.push_back(e);
			}
		}
	}
}

void AnimationTimelinePanel::Update(f32 dt)
{
	if (!m_Open) return;

	m_PreviewWidget.BeginFrame(dt);

	const f32 oldTime = m_CurrentTime;

	if (m_IsPlaying && !m_IsScrubbing && m_Duration > 0.0f)
	{
		m_CurrentTime += dt * m_PlaybackSpeed;

		for (const auto& track : m_NotifyTracks)
		{
			if (track.IsMuted) continue;
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

	auto* model = m_TargetModel.Get();
	auto skeleton = model ? model->GetSkeleton() : Engine::ResourceHandle<Engine::Skeleton>();
	auto anims = model ? model->GetAnimations() : std::vector<Engine::ResourceHandle<Engine::Animation>>();

	if (skeleton.IsValid() && !anims.empty() && m_SelectedClipIndex >= 0 && m_SelectedClipIndex < static_cast<int>(anims.size()))
	{
		auto* clip = anims[m_SelectedClipIndex].Get();
		auto* skel = skeleton.Get();
		if (clip && skel)
		{
			Engine::AnimationEvaluator::EvaluateSkinningMatrices(*clip, *skel, m_CurrentTime, m_BoneMatrices, m_IsLooping);
		}
	}
	else if (skeleton.IsValid())
	{
		auto* skel = skeleton.Get();
		if (skel) Engine::AnimationEvaluator::EvaluateBindPoseSkinningMatrices(*skel, m_BoneMatrices);
	}

	if (m_PreviewModelEntity && m_PreviewWidget.GetScene())
	{
		auto& reg = m_PreviewWidget.GetRegistry();
		if (auto* animator = reg.try_get<Engine::AnimatorComponent>(m_PreviewModelEntity->GetEntityHandle()))
		{
			animator->finalBoneMatrices = m_BoneMatrices;
		}
	}

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

void AnimationTimelinePanel::HandleModelDragDrop()
{
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ASSET_PAYLOAD"))
		{
			const auto* rawPath = static_cast<const wchar_t*>(payload->Data);
			if (rawPath)
			{
				const std::filesystem::path p(rawPath);
				const std::string ext = p.extension().string();
				if (ext == ".bammodel" || ext == ".glb" || ext == ".gltf")
				{
					auto& rm = Engine::ResourceManager::Get();
					const auto handle = rm.LoadFile(p.wstring());
					if (auto* res = rm.GetResource(handle))
					{
						if (auto* model = dynamic_cast<Engine::Model*>(res))
						{
							SetTargetModel(Engine::ResourceHandle<Engine::Model>(handle));
						}
					}
				}
			}
		}
		ImGui::EndDragDropTarget();
	}
}

void AnimationTimelinePanel::DrawBoneHierarchyTree()
{
	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), ICON_FA_SITEMAP " %s", LOCAL_CSTR("Timeline.BoneHierarchy"));
	ImGui::Separator();

	static char s_BoneFilter[64] = {};
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputTextWithHint("##BoneSearch", LOCAL_CSTR("Timeline.SearchBones"), s_BoneFilter, sizeof(s_BoneFilter));

	auto* model = m_TargetModel.Get();
	auto skeleton = model ? model->GetSkeleton() : Engine::ResourceHandle<Engine::Skeleton>();
	if (!skeleton.IsValid())
	{
		ImGui::Spacing();
		ImGui::TextDisabled("%s", LOCAL_CSTR("Timeline.NoSkeletonAvailable"));
		return;
	}

	const auto& bones = skeleton->GetBones();
	if (bones.empty()) return;

	std::string filterStr = s_BoneFilter;
	std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

	auto drawBoneItem = [&](uint32 boneIdx, const std::string& boneNameStr)
		{
			bool isSelectedSocket = false;
			if (m_Selection.Type == ETimelineSelectionType::NotifyMarker &&
				m_Selection.TrackIndex >= 0 && m_Selection.TrackIndex < static_cast<int>(m_NotifyTracks.size()))
			{
				const auto& track = m_NotifyTracks[m_Selection.TrackIndex];
				if (m_Selection.ItemIndex >= 0 && m_Selection.ItemIndex < static_cast<int>(track.Notifies.size()))
				{
					isSelectedSocket = (track.Notifies[m_Selection.ItemIndex].SocketBoneName == boneNameStr);
				}
			}

			ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (isSelectedSocket) nodeFlags |= ImGuiTreeNodeFlags_Selected;

			std::vector<uint32> children;
			for (size_t b = 0; b < bones.size(); ++b)
			{
				if (bones[b].ParentIndex == static_cast<Engine::int32>(boneIdx)) children.push_back(static_cast<uint32>(b));
			}
			if (children.empty()) nodeFlags |= ImGuiTreeNodeFlags_Leaf;

			const bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)boneIdx, nodeFlags, "%s %s", ICON_FA_BONE, boneNameStr.c_str());

			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				if (m_Selection.Type == ETimelineSelectionType::NotifyMarker &&
					m_Selection.TrackIndex >= 0 && m_Selection.TrackIndex < static_cast<int>(m_NotifyTracks.size()))
				{
					auto& track = m_NotifyTracks[m_Selection.TrackIndex];
					if (m_Selection.ItemIndex >= 0 && m_Selection.ItemIndex < static_cast<int>(track.Notifies.size()))
					{
						track.Notifies[m_Selection.ItemIndex].SocketBoneName = boneNameStr;
						m_IsDirty = true;
					}
				}
			}

			if (ImGui::IsItemHovered() && Engine::InputManager::Get().IsMouseButtonDoubleClicked(Engine::EMouseButton::Left))
			{
				const vec3 bPos = GetSocketWorldPosition(boneNameStr, vec3(0.0f));
				m_PreviewWidget.FocusTarget(bPos, 1.2f);
			}

			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem(LOCAL_CSTR("Timeline.SetAsSocket")))
				{
					if (m_Selection.Type == ETimelineSelectionType::NotifyMarker &&
						m_Selection.TrackIndex >= 0 && m_Selection.TrackIndex < static_cast<int>(m_NotifyTracks.size()))
					{
						auto& track = m_NotifyTracks[m_Selection.TrackIndex];
						if (m_Selection.ItemIndex >= 0 && m_Selection.ItemIndex < static_cast<int>(track.Notifies.size()))
						{
							track.Notifies[m_Selection.ItemIndex].SocketBoneName = boneNameStr;
							m_IsDirty = true;
						}
					}
				}
				if (ImGui::MenuItem(LOCAL_CSTR("Timeline.FocusBone")))
				{
					const vec3 bPos = GetSocketWorldPosition(boneNameStr, vec3(0.0f));
					m_PreviewWidget.FocusTarget(bPos, 1.2f);
				}
				ImGui::EndPopup();
			}

			return std::make_pair(nodeOpen, children);
		};

	if (!filterStr.empty())
	{
		for (size_t i = 0; i < bones.size(); ++i)
		{
			std::string bName = Engine::WStrToStr(bones[i].Name);
			std::string lowerBName = bName;
			std::transform(lowerBName.begin(), lowerBName.end(), lowerBName.begin(), ::tolower);
			if (lowerBName.find(filterStr) != std::string::npos)
			{
				auto [open, ch] = drawBoneItem(static_cast<uint32>(i), bName);
				if (open) ImGui::TreePop();
			}
		}
	}
	else
	{
		std::function<void(uint32)> traverse = [&](uint32 idx)
			{
				auto [open, ch] = drawBoneItem(idx, Engine::WStrToStr(bones[idx].Name));
				if (open)
				{
					for (uint32 c : ch) traverse(c);
					ImGui::TreePop();
				}
			};

		for (size_t i = 0; i < bones.size(); ++i)
		{
			if (bones[i].ParentIndex < 0 || bones[i].ParentIndex >= static_cast<Engine::int32>(bones.size()))
			{
				traverse(static_cast<uint32>(i));
			}
		}
	}
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
	HandleModelDragDrop();
	ImGui::EndChild();

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
}

void AnimationTimelinePanel::DrawCustomToolbar()
{
	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.File").c_str()))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("EditorCommon.Open").c_str())) RunWithDiscardConfirmation([this] { OpenTimelineFile(); });
		if (ImGui::MenuItem(LOCAL_LABEL("EditorCommon.Save").c_str(), ShortcutManager::Get().GetShortcutHint("AnimationTimeline.Save"))) SaveTimelineFile();
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
	if (!m_CurrentAssetPath.empty())
	{
		(void)SaveToFile(m_CurrentAssetPath);
	}
	else
	{
		std::wstring savePath;
		std::vector<FileDialogFilter> filters = {
			{ Engine::StrToWStr(LOCAL("FileDialog.ActionTimelineFiles")), L"*.bamtimeline.json;*.actiontimeline.json" }
		};
		std::wstring defaultName = Engine::StrToWStr(m_CurrentClipName) + L".bamtimeline.json";
		if (FileDialogs::SaveFileDialog(savePath, filters, defaultName, L"bamtimeline.json", EFileDialogContext::TimelineSave))
		{
			(void)SaveToFile(Engine::WStrToStr(savePath));
		}
	}
}

void AnimationTimelinePanel::DrawCustomQuickAccess()
{
	EditorUI::ToolbarLayout toolbar;
	if (toolbar.Icon("Open", ICON_FA_FOLDER_OPEN, "Timeline.OpenTimeline")) RunWithDiscardConfirmation([this] { OpenTimelineFile(); });
	if (toolbar.Icon("Save", ICON_FA_FLOPPY_DISK, "Timeline.SaveTimeline")) SaveTimelineFile();

	if (toolbar.Button("Sync", ICON_FA_ARROWS_ROTATE, "Timeline.SyncEntity"))
	{
		SyncFromSelectedEntity();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.SyncTooltip"));

	if (toolbar.Button("SelectModel", ICON_FA_FOLDER_OPEN, "Timeline.SelectModel"))
	{
		std::wstring selectedPath;
		std::vector<FileDialogFilter> filters = {
			{ Engine::StrToWStr(LOCAL("FileDialog.ModelFiles")), L"*.bammodel;*.glb" },
			{ Engine::StrToWStr(LOCAL("FileDialog.AllFiles")), L"*.*" }
		};
		if (FileDialogs::OpenFileDialog(selectedPath, filters, L"Resources/Model"))
		{
			auto& rm = Engine::ResourceManager::Get();
			const auto handle = rm.LoadFile(selectedPath);
			if (auto* res = rm.GetResource(handle))
			{
				if (auto* model = dynamic_cast<Engine::Model*>(res))
				{
					SetTargetModel(Engine::ResourceHandle<Engine::Model>(handle));
				}
			}
		}
	}

	auto* model = m_TargetModel.Get();
	if (model)
	{
		auto anims = model->GetAnimations();
		if (!anims.empty())
		{
			toolbar.SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
			if (ImGui::BeginCombo("##ClipSelect", m_CurrentClipName.c_str()))
			{
				for (int i = 0; i < static_cast<int>(anims.size()); ++i)
				{
					auto* clip = anims[i].Get();
					if (!clip) continue;

					const std::string clipNameStr = Engine::WStrToStr(clip->GetName());
					const bool isSelected = (i == m_SelectedClipIndex);
					if (ImGui::Selectable(clipNameStr.c_str(), isSelected))
					{
						m_SelectedClipIndex = i;
						m_CurrentClipName = clipNameStr;
						const f64 tps = clip->GetTicksPerSecond() > 0.0 ? clip->GetTicksPerSecond() : 25.0;
						const f32 durationSec = clip->GetDuration() > 0.0 ? static_cast<f32>(clip->GetDuration() / tps) : 1.0f;
						m_Duration = std::max(0.1f, durationSec);
						m_CurrentTime = 0.0f;
						m_TimelineZoom = 1.0f;
						m_ViewStartTime = 0.0f;
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
	char nameBuf[64];
	snprintf(nameBuf, sizeof(nameBuf), "%s", notify.Name.c_str());
	if (ImGui::InputText(LOCAL_LABEL("Timeline.NotifyName").c_str(), nameBuf, sizeof(nameBuf)))
	{
		notify.Name = nameBuf;
		m_IsDirty = true;
	}

	if (ImGui::SliderFloat(LOCAL_LABEL("Timeline.TriggerTime").c_str(), &notify.TriggerTime, 0.0f, m_Duration, "%.2fs"))
	{
		m_IsDirty = true;
	}

	const std::string typeNames[] = {
		LOCAL("Timeline.NotifyTypeParticle"),
		LOCAL("Timeline.NotifyTypeSound"),
		LOCAL("Timeline.NotifyTypeCustomEvent"),
		LOCAL("Timeline.NotifyTypeSpawnCompositeVFX")
	};
	const char* typeItems[] = { typeNames[0].c_str(), typeNames[1].c_str(), typeNames[2].c_str(), typeNames[3].c_str() };
	int currentType = static_cast<int>(notify.Type);
	if (ImGui::Combo(LOCAL_LABEL("Timeline.NotifyType").c_str(), &currentType, typeItems, IM_ARRAYSIZE(typeItems)))
	{
		notify.Type = static_cast<Engine::EAnimNotifyType>(currentType);
		m_IsDirty = true;
	}

	auto* model = m_TargetModel.Get();
	auto skeleton = model ? model->GetSkeleton() : Engine::ResourceHandle<Engine::Skeleton>();
	if (skeleton.IsValid())
	{
		const auto& bones = skeleton->GetBones();
		const std::string currentBoneDisplay = notify.SocketBoneName.empty() ? LOCAL("Timeline.RootOrNone") : notify.SocketBoneName;

		if (ImGui::BeginCombo(LOCAL_LABEL("Timeline.SocketBone").c_str(), currentBoneDisplay.c_str()))
		{
			if (ImGui::Selectable(LOCAL_LABEL("Timeline.RootOrNone").c_str(), notify.SocketBoneName.empty()))
			{
				notify.SocketBoneName.clear();
				m_IsDirty = true;
			}

			for (const auto& bone : bones)
			{
				const std::string boneNameStr = Engine::WStrToStr(bone.Name);
				const bool isSel = (notify.SocketBoneName == boneNameStr);
				if (ImGui::Selectable(boneNameStr.c_str(), isSel))
				{
					notify.SocketBoneName = boneNameStr;
					m_IsDirty = true;
				}
				if (isSel) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	if (ImGui::DragFloat3(LOCAL_LABEL("Timeline.OffsetPos").c_str(), &notify.OffsetPosition.x, 0.02f, -10.0f, 10.0f, "%.2f")) m_IsDirty = true;

	if (notify.Type == Engine::EAnimNotifyType::Particle)
	{
		ImGui::Separator();
		if (ImGui::ColorEdit4(LOCAL_LABEL("Timeline.ParticleColor").c_str(), &notify.ParticleColor.x, ImGuiColorEditFlags_NoInputs)) m_IsDirty = true;
		int count = static_cast<int>(notify.BurstCount);
		if (ImGui::SliderInt(LOCAL_LABEL("Timeline.BurstCount").c_str(), &count, 5, 80))
		{
			notify.BurstCount = static_cast<uint32>(count);
			m_IsDirty = true;
		}
		if (ImGui::SliderFloat(LOCAL_LABEL("Timeline.ParticleSpeed").c_str(), &notify.ParticleSpeed, 0.2f, 5.0f, "%.1f")) m_IsDirty = true;
		if (ImGui::SliderFloat(LOCAL_LABEL("PROP_SIZE").c_str(), &notify.ParticleSize, 0.02f, 0.5f, "%.2f")) m_IsDirty = true;
	}
	else if (notify.Type == Engine::EAnimNotifyType::Sound)
	{
		ImGui::Separator();
		std::string soundName = LOCAL("EditorCommon.None");
		if (!notify.SoundPath.empty())
		{
			const std::filesystem::path soundFile(Engine::StrToWStr(notify.SoundPath));
			soundName = Engine::WStrToStr(soundFile.filename().wstring());
		}
		const std::string soundLabel = LOCAL_FMT("Timeline.SoundFile", soundName);
		ImGui::TextUnformatted(soundLabel.c_str());

		const std::string selectSoundLabel = std::string(ICON_FA_FOLDER_OPEN " ") + LOCAL_LABEL("Timeline.SelectSound");
		if (ImGui::Button(selectSoundLabel.c_str()))
		{
			std::wstring soundPath;
			const std::vector<FileDialogFilter> filters = {
				{ Engine::StrToWStr(LOCAL("FileDialog.AudioFiles")), L"*.wav;*.mp3;*.ogg" },
				{ Engine::StrToWStr(LOCAL("FileDialog.AllFiles")), L"*.*" }
			};
			if (FileDialogs::OpenFileDialog(soundPath, filters, L"Resources/Audio"))
			{
				notify.SoundPath = Engine::WStrToStr(soundPath);
				m_IsDirty = true;
			}
		}
		if (ImGui::SliderFloat(LOCAL_LABEL("PROP_VOLUME").c_str(), &notify.SoundVolume, 0.0f, 2.0f, "%.2f")) m_IsDirty = true;
	}
	else if (notify.Type == Engine::EAnimNotifyType::CustomEvent)
	{
		ImGui::Separator();
		char tagBuf[64];
		snprintf(tagBuf, sizeof(tagBuf), "%s", notify.EventTag.c_str());
		if (ImGui::InputText(LOCAL_LABEL("Timeline.EventTag").c_str(), tagBuf, sizeof(tagBuf)))
		{
			notify.EventTag = tagBuf;
			m_IsDirty = true;
		}
	}
	else if (notify.Type == Engine::EAnimNotifyType::SpawnCompositeVFX)
	{
		ImGui::Separator();
		std::string vfxName = LOCAL("EditorCommon.None");
		if (!notify.VFXPath.empty())
		{
			const std::filesystem::path vfxFile(Engine::StrToWStr(notify.VFXPath));
			vfxName = Engine::WStrToStr(vfxFile.filename().wstring());
		}
		const std::string vfxLabel = LOCAL_FMT("Timeline.VFXFile", vfxName);
		ImGui::TextUnformatted(vfxLabel.c_str());

		const std::string selectVFXLabel = std::string(ICON_FA_FOLDER_OPEN " ") + LOCAL_LABEL("Timeline.SelectVFX");
		if (ImGui::Button(selectVFXLabel.c_str()))
		{
			std::wstring vfxPath;
			const std::vector<FileDialogFilter> filters = {
				{ Engine::StrToWStr(LOCAL("FileDialog.VFXFiles")), L"*.bamvfx" },
				{ Engine::StrToWStr(LOCAL("FileDialog.AllFiles")), L"*.*" }
			};
			if (FileDialogs::OpenFileDialog(vfxPath, filters, L"Resources/VFX"))
			{
				notify.VFXPath = Engine::WStrToStr(vfxPath);
				m_IsDirty = true;
			}
		}

		if (ImGui::Checkbox(LOCAL_LABEL("Timeline.AttachToSocket").c_str(), &notify.AttachToSocket)) m_IsDirty = true;
		if (ImGui::DragFloat3(LOCAL_LABEL("Timeline.OffsetRot").c_str(), &notify.OffsetRotation.x, 1.0f, -180.0f, 180.0f, "%.1f deg")) m_IsDirty = true;
		if (ImGui::DragFloat3(LOCAL_LABEL("Timeline.VFXScale").c_str(), &notify.VFXScale.x, 0.05f, 0.01f, 10.0f, "%.2f")) m_IsDirty = true;
		if (ImGui::SliderFloat(LOCAL_LABEL("Timeline.VFXPlayRate").c_str(), &notify.VFXPlayRate, 0.1f, 5.0f, "%.2fx")) m_IsDirty = true;
		if (ImGui::Checkbox(LOCAL_LABEL("Timeline.AutoDestroy").c_str(), &notify.AutoDestroy)) m_IsDirty = true;
	}

	ImGui::Spacing();
	ImGui::Separator();

	const std::string triggerTestLabel = std::string(ICON_FA_PLAY " ") + LOCAL_LABEL("Timeline.TriggerTest");
	if (ImGui::Button(triggerTestLabel.c_str()))
	{
		TriggerNotify(notify);
	}
	ImGui::SameLine();
	const std::string deleteLabel = std::string(ICON_FA_TRASH " ") + LOCAL_LABEL("Timeline.DeleteNotify");
	ImGui::BeginDisabled(!CanDeleteSelected());
	if (ImGui::Button(deleteLabel.c_str())) DeleteSelected();
	ImGui::EndDisabled();
}

void AnimationTimelinePanel::DrawCameraShotInspector(Engine::TimelineCameraTrack& camShot)
{
	char nameBuf[64];
	snprintf(nameBuf, sizeof(nameBuf), "%s", camShot.TrackName.c_str());
	if (ImGui::InputText(LOCAL_LABEL("Timeline.CameraShotName").c_str(), nameBuf, sizeof(nameBuf)))
	{
		camShot.TrackName = nameBuf;
		m_IsDirty = true;
	}

	if (ImGui::Checkbox(LOCAL_LABEL("PROP_ENABLED").c_str(), &camShot.IsActive)) m_IsDirty = true;
	if (ImGui::SliderFloat(LOCAL_LABEL("Timeline.CameraShotStartTime").c_str(), &camShot.StartTime, 0.0f, m_Duration, "%.2fs")) m_IsDirty = true;
	if (ImGui::SliderFloat(LOCAL_LABEL("Timeline.CameraShotDuration").c_str(), &camShot.Duration, 0.05f, 5.0f, "%.2fs")) m_IsDirty = true;
	if (ImGui::SliderFloat(LOCAL_LABEL("Timeline.CameraShotTargetFOV").c_str(), &camShot.TargetFOV, 15.0f, 90.0f, "%.1f deg")) m_IsDirty = true;
	if (ImGui::SliderFloat(LOCAL_LABEL("Timeline.CameraShotShakeIntensity").c_str(), &camShot.ShakeIntensity, 0.0f, 1.5f, "%.2f")) m_IsDirty = true;

	auto* model = m_TargetModel.Get();
	auto skeleton = model ? model->GetSkeleton() : Engine::ResourceHandle<Engine::Skeleton>();
	if (skeleton.IsValid())
	{
		const auto& bones = skeleton->GetBones();
		const std::string currentBoneDisplay = camShot.FocusBoneName.empty() ? LOCAL("Timeline.RootOrNone") : camShot.FocusBoneName;

		if (ImGui::BeginCombo(LOCAL_LABEL("Timeline.CameraShotFocusBone").c_str(), currentBoneDisplay.c_str()))
		{
			if (ImGui::Selectable(LOCAL_LABEL("Timeline.RootOrNone").c_str(), camShot.FocusBoneName.empty()))
			{
				camShot.FocusBoneName.clear();
				m_IsDirty = true;
			}

			for (const auto& bone : bones)
			{
				const std::string boneNameStr = Engine::WStrToStr(bone.Name);
				const bool isSelected = (camShot.FocusBoneName == boneNameStr);
				if (ImGui::Selectable(boneNameStr.c_str(), isSelected))
				{
					camShot.FocusBoneName = boneNameStr;
					m_IsDirty = true;
				}
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	ImGui::Spacing();
	ImGui::Separator();
	const std::string deleteLabel = std::string(ICON_FA_TRASH " ") + LOCAL_LABEL("EditorCommon.Delete");
	ImGui::BeginDisabled(!CanDeleteSelected());
	if (ImGui::Button(deleteLabel.c_str())) DeleteSelected();
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
		if (ImGui::MenuItem((ICON_FA_PLUS " " + LOCAL_LABEL("Timeline.AddFXTrack")).c_str()))
		{
			Engine::TimelineNotifyTrack newTrack{};
			newTrack.TrackName = "FX_Track_" + std::to_string(m_NotifyTracks.size() + 1);
			m_NotifyTracks.push_back(newTrack);
			m_IsDirty = true;
		}
		if (ImGui::MenuItem((ICON_FA_MUSIC " " + LOCAL_LABEL("Timeline.AddAudioTrack")).c_str()))
		{
			Engine::TimelineNotifyTrack newTrack{};
			newTrack.TrackName = "Audio_Track_" + std::to_string(m_NotifyTracks.size() + 1);
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
	drawList->AddText(ImVec2(std::max(trackMin.x + 8.0f, clipMin.x + 12.0f), textY), IM_COL32(255, 255, 255, 255), m_CurrentClipName.c_str());
	drawList->PopClipRect();
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
	const char* trackTypeIcon = ICON_FA_BURST;
	ImU32 trackIconColor = IM_COL32(255, 175, 45, 255);
	if (track.TrackName.find("Audio") != std::string::npos || track.TrackName.find("Sound") != std::string::npos)
	{
		trackTypeIcon = ICON_FA_MUSIC;
		trackIconColor = IM_COL32(80, 200, 255, 255);
	}
	else if (track.TrackName.find("Event") != std::string::npos)
	{
		trackTypeIcon = ICON_FA_BELL;
		trackIconColor = IM_COL32(200, 120, 255, 255);
	}
	const auto labelState = rowSelected ? EditorUI::TimelineLabelState::Selected
		: (track.IsMuted ? EditorUI::TimelineLabelState::Disabled : EditorUI::TimelineLabelState::Normal);
	EditorUI::DrawTimelineRowLabel(*drawList, headerMin.x, headerMax.x, headerMin.y, headerMax.y, trackTypeIcon, trackIconColor, track.TrackName, labelState);

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
	}

	bool anyMarkerHovered = false;
	for (size_t nIdx = 0; nIdx < track.Notifies.size(); ++nIdx)
	{
		auto& notify = track.Notifies[nIdx];
		const float markerX = TimeToScreenX(notify.TriggerTime, trackMin.x, timelineWidth);
		if (markerX < trackMin.x - 15.0f || markerX > trackMin.x + timelineWidth + 15.0f) continue;
		const bool isMarkerHovered = isTrackHovered && (std::abs(mousePos.x - markerX) <= markerHalfSize + 2.5f && std::abs(mousePos.y - centerY) <= markerHalfSize + 2.5f);
		if (isMarkerHovered) anyMarkerHovered = true;
		const bool isSelected = (m_Selection.Type == ETimelineSelectionType::NotifyMarker && m_Selection.TrackIndex == trackIdx && m_Selection.ItemIndex == static_cast<int>(nIdx));

		if (isMarkerHovered && isMouseClicked)
		{
			m_Selection = { ETimelineSelectionType::NotifyMarker, trackIdx, static_cast<int>(nIdx) };
			m_IsDraggingItem = true;
			const float clickTime = ScreenXToTime(mousePos.x, trackMin.x, timelineWidth);
			m_DragTimeOffset = clickTime - notify.TriggerTime;
		}

		if (m_IsDraggingItem && isSelected && isMouseDown)
		{
			const float curMouseTime = ScreenXToTime(mousePos.x, trackMin.x, timelineWidth);
			notify.TriggerTime = std::clamp(curMouseTime - m_DragTimeOffset, 0.0f, m_Duration);
			m_IsDirty = true;
		}

		ImU32 markerColor = IM_COL32(255, 160, 40, 240);
		if (notify.Type == Engine::EAnimNotifyType::Sound) markerColor = IM_COL32(60, 200, 240, 240);
		else if (notify.Type == Engine::EAnimNotifyType::CustomEvent) markerColor = IM_COL32(190, 100, 255, 240);
		else if (notify.Type == Engine::EAnimNotifyType::SpawnCompositeVFX) markerColor = IM_COL32(255, 60, 140, 240);

		EditorUI::DrawTimelineMarker(*drawList, ImVec2(markerX, centerY), markerColor, isSelected);

		if (isMarkerHovered)
		{
			ImGui::BeginTooltip();
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[%s]", notify.Name.c_str());
			ImGui::Text(LOCAL_CSTR("Timeline.MarkerTimeSocket"), notify.TriggerTime, notify.SocketBoneName.empty() ? LOCAL_CSTR("Timeline.Root") : notify.SocketBoneName.c_str());
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
		const float clickTime = ScreenXToTime(mousePos.x, trackMin.x, timelineWidth);
		Engine::AnimNotifyData newMarker{};
		const bool isSoundTrack = (track.TrackName.find("Audio") != std::string::npos || track.TrackName.find("Sound") != std::string::npos);
		newMarker.Name = isSoundTrack ? "Sound_Cue" : "VFX_Slash";
		newMarker.TriggerTime = clickTime;
		newMarker.Type = isSoundTrack ? Engine::EAnimNotifyType::Sound : Engine::EAnimNotifyType::SpawnCompositeVFX;
		newMarker.AttachToSocket = true;
		newMarker.VFXScale = vec3(1.0f);
		track.Notifies.push_back(newMarker);
		m_Selection = { ETimelineSelectionType::NotifyMarker, trackIdx, static_cast<int>(track.Notifies.size()) - 1 };
		m_IsDirty = true;
	}

	if (isTrackHovered && input.IsMouseButtonDown(Engine::EMouseButton::Right))
	{
		ImGui::OpenPopup(("##BodyPopup_" + std::to_string(trackIdx)).c_str());
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