#include "AnimationTimelinePanel.h"
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
#include "Components/Core/TransformComponent.h"
#include "Components/Core/WorldTransformComponent.h"
#include "Components/Core/FlagComponent.h"
#include "Components/Render/LightComponent.h"
#include "Components/Render/AnimatorComponent.h"
#include "Components/Render/SkinnedMeshRendererComponent.h"
#include "TransformSystem.h"
#include "LightSystem.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include "AudioClip.h"
#include "AudioManager.h"
#include "Components/Render/ActionTimelineComponent.h"
#include "Components/Core/NameComponent.h"
#include "ShortcutManager.h"

namespace Editor
{
	AnimationTimelinePanel::AnimationTimelinePanel()
	{
		s_ActivePanel = this;
		m_Name = L"AnimationTimelinePanel";
		m_DisplayNameKey = "Timeline.PanelTitle";
		Close();
		// 기본 다중 트랙 프리셋 구성 (FX 트랙 1개 + 카메라 샷 트랙 1개)
		Engine::TimelineNotifyTrack fxTrack{};
		fxTrack.TrackName = "FX_Weapon";
		Engine::AnimNotifyData defaultNotify{};
		defaultNotify.Name = "Slash_Burst";
		defaultNotify.TriggerTime = 0.40f;
		defaultNotify.Type = Engine::EAnimNotifyType::Particle;
		defaultNotify.ParticleColor = vec4(1.0f, 0.65f, 0.15f, 1.0f);
		defaultNotify.BurstCount = 30;
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
		FreePreviewScene();
	}
	void AnimationTimelinePanel::StepFrame(int step)
	{
		const float frameStep = 1.0f / m_FPS;
		m_CurrentTime = std::clamp(m_CurrentTime + static_cast<float>(step) * frameStep, 0.0f, m_Duration);
	}
	bool AnimationTimelinePanel::CanDeleteSelected() const
	{
		if (!m_Open || m_IsDraggingItem || m_IsScrubbing || m_Selection.TrackIndex < 0)
			return false;

		if (m_Selection.Type == ETimelineSelectionType::NotifyMarker)
		{
			if (m_Selection.TrackIndex >= static_cast<int>(m_NotifyTracks.size()))
				return false;

			const auto& notifies = m_NotifyTracks[m_Selection.TrackIndex].Notifies;
			return m_Selection.ItemIndex >= 0 && m_Selection.ItemIndex < static_cast<int>(notifies.size());
		}

		return m_Selection.Type == ETimelineSelectionType::CameraShot &&
			m_Selection.TrackIndex < static_cast<int>(m_CameraTracks.size());
	}

	void AnimationTimelinePanel::DeleteSelected()
	{
		if (!CanDeleteSelected())
			return;

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

	void AnimationTimelinePanel::SetupPreviewScene()
	{
		if (m_PreviewScene) return;

		m_Pipeline.Initialize(L"TimelinePreview", m_PreviewWidth, m_PreviewHeight, false);
		m_PostProcessChain.Initialize(L"TimelinePreview_PP");
		m_PostProcessChain.PrepareResources(m_PreviewWidth, m_PreviewHeight);
		m_DisplayRTName = m_Pipeline.GetFinalColorRTName();

		Engine::SceneCreateDesc desc{};
		desc.name = L"TimelinePreview_Scene";
		m_PreviewScene = Engine::Scene::Create(&desc);
		if (!m_PreviewScene) return;

		auto& registry = m_PreviewScene->GetRegistry();

		auto createDirLight = [&](const vec3& euler, const vec3& color, f32 intensity, bool specular)
			{
				entt::entity e = registry.create();
				auto& trans = registry.emplace<Engine::TransformComponent>(e);
				registry.emplace<Engine::WorldTransformComponent>(e);
				auto& flag = registry.emplace<Engine::FlagComponent>(e);
				flag.flags = Engine::EEntityFlag::Active | Engine::EEntityFlag::Visible;
				auto& comp = registry.emplace<Engine::LightComponent>(e);
				comp.LightType = Engine::ELightType::Directional;
				comp.Intensity = intensity;
				comp.Color = color;
				comp.Flags = Engine::ELightFlags::UseInDeferredRendering | Engine::ELightFlags::AffectDiffuse;
				if (specular) comp.Flags |= Engine::ELightFlags::AffectSpecular;
				(void)trans.SetEulerRotation(euler);
			};

		createDirLight(vec3(35.0f, 30.0f, 0.0f), vec3(1.0f, 0.98f, 0.95f), 3.0f, true);
		createDirLight(vec3(20.0f, -60.0f, 0.0f), vec3(0.85f, 0.92f, 1.0f), 1.6f, false);
		createDirLight(vec3(45.0f, 160.0f, 0.0f), vec3(0.95f, 0.98f, 1.0f), 2.2f, true);

		Engine::TransformSystem::UpdateHierarchy(registry);
		Engine::LightSystem::UpdateLights(registry);
		m_PipelineInitialized = true;

		auto& rm = Engine::ResourceManager::Get();
		const std::filesystem::path defaultModelPath = L"Resources/Model/Fox.bammodel";
		if (std::filesystem::exists(defaultModelPath))
		{
			const auto handle = rm.LoadFile(defaultModelPath.wstring());
			if (auto* res = rm.GetResource(handle))
			{
				if (auto* model = dynamic_cast<Engine::Model*>(res))
				{
					SetTargetModel(Engine::ResourceHandle<Engine::Model>(handle));
				}
			}
		}
	}

	void AnimationTimelinePanel::FreePreviewScene()
	{
		if (m_PipelineInitialized)
		{
			m_PostProcessChain.Free();
			m_Pipeline.Free();
			m_PipelineInitialized = false;
		}

		if (m_PreviewScene)
		{
			Engine::Safe_Release(m_PreviewScene);
			m_PreviewScene = nullptr;
		}
	}

	void AnimationTimelinePanel::SetTargetModel(const Engine::ResourceHandle<Engine::Model>& model)
	{
		m_TargetModel = model;
		m_SelectedClipIndex = 0;
		m_BoneMatrices.clear();

		auto* modelPtr = m_TargetModel.Get();
		if (modelPtr)
		{
			auto anims = modelPtr->GetAnimations();
			if (!anims.empty())
			{
				auto* clip = anims[0].Get();
				if (clip)
				{
					m_CurrentClipName = Engine::WStrToStr(clip->GetName());
					const f64 tps = clip->GetTicksPerSecond() > 0.0 ? clip->GetTicksPerSecond() : 25.0;
					const f32 durationSec = clip->GetDuration() > 0.0 ? static_cast<f32>(clip->GetDuration() / tps) : 1.0f;
					m_Duration = std::max(0.1f, durationSec);
				}
			}
			else
			{
				m_CurrentClipName = "No Animation Clips";
				m_Duration = 1.0f;
			}
		}
		else
		{
			m_CurrentClipName = "No Model Loaded";
		}

		m_CurrentTime = 0.0f;
		m_PrevTime = 0.0f;
		m_TimelineZoom = 1.0f;
		m_ViewStartTime = 0.0f;
	}

	void AnimationTimelinePanel::SyncFromSelectedEntity()
	{
		auto* selectedEntity = SelectionManager::Get().GetPrimarySelectedEntity();
		if (!selectedEntity) return;

		auto* scene = selectedEntity->GetScene();
		const entt::entity handle = selectedEntity->GetEntityHandle();
		if (!scene || handle == entt::null || !scene->GetRegistry().valid(handle)) return;

		auto& registry = scene->GetRegistry();

		// 1. ActionTimelineComponent가 있다면 타임라인 파일 로드 (모델 경로 및 클립 자동 동기화)
		if (auto* timelineComp = registry.try_get<Engine::ActionTimelineComponent>(handle))
		{
			if (!timelineComp->timelinePath.empty())
			{
				LoadFromFile(timelineComp->timelinePath);
				return;
			}
		}

		// 2. 타임라인 에셋이 없는 경우 엔티티 이름(NameComponent)으로 Resources/Model에서 모델 탐색
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
		const float visibleDuration = (m_Duration > 0.0f) ? (m_Duration / m_TimelineZoom) : 1.0f;
		const float normalized = (time - m_ViewStartTime) / visibleDuration;
		return timelineMinX + normalized * timelineWidth;
	}

	float AnimationTimelinePanel::ScreenXToTime(float screenX, float timelineMinX, float timelineWidth) const
	{
		const float visibleDuration = (m_Duration > 0.0f) ? (m_Duration / m_TimelineZoom) : 1.0f;
		const float normalized = (screenX - timelineMinX) / timelineWidth;
		return std::clamp(m_ViewStartTime + normalized * visibleDuration, 0.0f, m_Duration);
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
		ResourceManager::Get().DestroyResource(asset);

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
			ResourceManager::Get().DestroyResource(asset);
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
		m_Duration = asset->GetDuration();
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

		ResourceManager::Get().DestroyResource(asset);
		m_CurrentAssetPath = filePath;
		m_Selection = {};
		m_IsDirty = false;
		return true;
	}

	void AnimationTimelinePanel::TriggerNotify(const Engine::AnimNotifyData& notify)
	{
		m_LastTriggeredNotifyName = notify.Name;
		m_TriggerToastTimer = 1.4f;
		m_LastTriggeredWorldPos = GetSocketWorldPosition(notify.SocketBoneName, notify.OffsetPosition);

		if (notify.Type == Engine::EAnimNotifyType::Particle)
		{
			for (uint32 i = 0; i < notify.BurstCount; ++i)
			{
				TimelinePreviewParticle p{};
				p.Position = m_LastTriggeredWorldPos;

				const float theta = (static_cast<float>(rand()) / RAND_MAX) * 6.2831853f;
				const float phi = (static_cast<float>(rand()) / RAND_MAX) * 3.1415926f;
				const float speed = notify.ParticleSpeed * (0.6f + 0.8f * (static_cast<float>(rand()) / RAND_MAX));
				p.Velocity = vec3(std::sin(phi) * std::cos(theta), std::abs(std::cos(phi)) + 0.35f, std::sin(phi) * std::sin(theta)) * speed;
				p.Color = notify.ParticleColor;
				p.Size = notify.ParticleSize * (0.8f + 0.4f * (static_cast<float>(rand()) / RAND_MAX));
				p.Age = 0.0f;
				p.Lifetime = notify.ParticleLifetime * (0.8f + 0.4f * (static_cast<float>(rand()) / RAND_MAX));
				m_PreviewParticles.push_back(p);
			}
		}
		else if (notify.Type == Engine::EAnimNotifyType::Sound && !notify.SoundPath.empty())
		{
			auto& rm = Engine::ResourceManager::Get();
			auto handle = rm.LoadFile(Engine::StrToWStr(notify.SoundPath));
			if (auto* clip = dynamic_cast<Engine::AudioClip*>(rm.GetResource(handle)))
			{
				if (clip->GetFMODSound())
				{
					Engine::AudioManager::Get().PlayOneShot(clip->GetFMODSound(), notify.SoundVolume);
				}
			}
		}
	}

	vec3 AnimationTimelinePanel::GetSocketWorldPosition(const std::string& boneName, const vec3& offset) const
	{
		auto* model = m_TargetModel.Get();
		if (!model) return offset;

		auto skeleton = model->GetSkeleton();
		if (!skeleton.IsValid() || boneName.empty() || m_BoneMatrices.empty())
		{
			return offset;
		}

		const std::wstring boneNameW = Engine::StrToWStr(boneName);
		const auto& bones = skeleton->GetBones();
		const auto& boneMap = skeleton->GetBoneMap();

		auto it = boneMap.find(boneNameW);
		if (it != boneMap.end())
		{
			uint32 boneIdx = it->second;
			if (boneIdx < m_BoneMatrices.size() && boneIdx < bones.size())
			{
				mat4 invOffset = glm::inverse(bones[boneIdx].OffsetMatrix);
				mat4 combined = m_BoneMatrices[boneIdx] * invOffset;

				const auto& nodes = model->GetNodes();
				mat4 rootTransform = nodes.empty() ? mat4(1.0f) : nodes[0].LocalTransform;

				vec4 localSocketPos = combined * vec4(offset, 1.0f);
				vec4 worldSocketPos = rootTransform * localSocketPos;
				return vec3(worldSocketPos);
			}
		}

		return offset;
	}

	void AnimationTimelinePanel::Update(f32 dt)
	{
		if (!m_Open) return;

		if (!m_PipelineInitialized)
		{
			SetupPreviewScene();
		}

		const f32 oldTime = m_CurrentTime;

		// 타임라인 재생 및 다중 트랙 노티파이 검출
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
				if (m_IsLooping)
				{
					m_CurrentTime = std::fmod(m_CurrentTime, m_Duration);
				}
				else
				{
					m_CurrentTime = m_Duration;
					m_IsPlaying = false;
				}
			}
		}

		m_PrevTime = m_CurrentTime;

		// 실시간 카메라 샷 연출 평가 (FOV 줌인 & 셰이크)
		m_CurrentFOV = m_BaseFOV;
		vec3 shakeOffset(0.0f);
		for (const auto& camShot : m_CameraTracks)
		{
			if (!camShot.IsActive) continue;
			if (m_CurrentTime >= camShot.StartTime && m_CurrentTime <= camShot.StartTime + camShot.Duration)
			{
				// 화각 부드럽게 보간
				const float progress = (m_CurrentTime - camShot.StartTime) / std::max(0.01f, camShot.Duration);
				const float blend = std::sin(progress * 3.14159265f); // 줌인 후 자연스럽게 복귀
				m_CurrentFOV = glm::mix(m_BaseFOV, camShot.TargetFOV, blend);

				// 카메라 셰이크
				if (camShot.ShakeIntensity > 0.0f)
				{
					const float rx = ((static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f) * camShot.ShakeIntensity * 0.08f;
					const float ry = ((static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f) * camShot.ShakeIntensity * 0.08f;
					shakeOffset = vec3(rx, ry, 0.0f);
				}
			}
		}

		if (m_TriggerToastTimer > 0.0f) m_TriggerToastTimer -= dt;
		for (auto& p : m_PreviewParticles)
		{
			p.Age += dt;
			p.Velocity += vec3(0.0f, -4.5f, 0.0f) * dt;
			p.Position += p.Velocity * dt;
		}
		std::erase_if(m_PreviewParticles, [](const auto& p) { return !p.IsAlive(); });

		RenderPreview(dt);
	}

	void AnimationTimelinePanel::RenderPreview(f32 dt)
	{
		if (!m_PreviewScene || !m_PipelineInitialized) return;

		auto& registry = m_PreviewScene->GetRegistry();
		Engine::TransformSystem::UpdateHierarchy(registry);
		Engine::LightSystem::UpdateLights(registry);

		auto* model = m_TargetModel.Get();
		auto skeleton = model ? model->GetSkeleton() : Engine::ResourceHandle<Engine::Skeleton>();
		auto anims = model ? model->GetAnimations() : std::vector<Engine::ResourceHandle<Engine::Animation>>();

		if (skeleton.IsValid() && !anims.empty() && m_SelectedClipIndex < static_cast<int>(anims.size()))
		{
			auto* clip = anims[m_SelectedClipIndex].Get();
			auto* skel = skeleton.Get();
			if (clip && skel)
			{
				Engine::AnimationEvaluator::EvaluateSkinningMatrices(*clip, *skel, m_CurrentTime, m_BoneMatrices, m_IsLooping);
			}
		}

		vec3 offset;
		offset.x = m_CamDistance * cosf(m_CamPitch) * sinf(m_CamYaw);
		offset.y = m_CamDistance * sinf(m_CamPitch);
		offset.z = -m_CamDistance * cosf(m_CamPitch) * cosf(m_CamYaw);
		vec3 eye = m_CamTarget + offset;

		mat4 view = glm::lookAtLH(eye, m_CamTarget, vec3(0.0f, 1.0f, 0.0f));
		mat4 proj = glm::perspectiveLH_ZO(glm::radians(m_CurrentFOV), static_cast<f32>(m_PreviewWidth) / static_cast<f32>(m_PreviewHeight), 0.1f, 100.0f);
		m_CachedView = view;
		m_CachedProj = proj;

		CameraBuffer cameraBuffer = {};
		cameraBuffer.viewMatrix = view;
		cameraBuffer.invViewMatrix = glm::inverse(view);
		cameraBuffer.projMatrix = proj;
		cameraBuffer.invProjMatrix = glm::inverse(proj);
		cameraBuffer.viewProjMatrix = proj * view;
		cameraBuffer.invViewProjMatrix = glm::inverse(cameraBuffer.viewProjMatrix);
		cameraBuffer.cameraPosition = eye;
		cameraBuffer.time = static_cast<f32>(Engine::TimeManager::Get().GetTotalTime());

		m_Pipeline.Submit(cameraBuffer, m_PreviewScene);

		if (model)
		{
			auto meshes = model->GetMeshes();
			auto materials = model->GetMaterials();
			const auto& nodes = model->GetNodes();

			for (const auto& node : nodes)
			{
				for (uint32 meshIdx : node.MeshIndices)
				{
					if (meshIdx >= meshes.size()) continue;

					auto* mesh = meshes[meshIdx].Get();
					auto matHandle = model->GetMeshMaterial(meshIdx);
					auto* mat = matHandle.IsValid() ? matHandle.Get() : nullptr;
					if (!mat && meshIdx < materials.size()) mat = materials[meshIdx].Get();
					if (!mat && !materials.empty()) mat = materials[0].Get();

					if (mesh && mat)
					{
						if (!m_BoneMatrices.empty())
						{
							Engine::SkinnedDrawCommand cmd{};
							cmd.mesh = mesh;
							cmd.material = mat;
							cmd.worldMatrix = node.LocalTransform;
							cmd.boneMatrices = &m_BoneMatrices;
							Engine::Renderer::Get().SubmitSkinned(cmd, m_Pipeline.GetGeometryPassID());
						}
						else
						{
							Engine::StaticDrawCommand cmd{};
							cmd.mesh = mesh;
							cmd.material = mat;
							cmd.worldMatrix = node.LocalTransform;
							Engine::Renderer::Get().SubmitStatic(cmd, m_Pipeline.GetGeometryPassID());
						}
					}
				}
			}
		}

		m_DisplayRTName = m_PostProcessChain.ExecuteChain(dt, m_Pipeline.GetFinalColorRTName());
	}

	void AnimationTimelinePanel::Draw()
	{
		m_Focused = false;
		m_Hovered = false;


		if (!m_Open) return;

		ImGui::SetNextWindowSize(ImVec2(1060.0f, 660.0f), ImGuiCond_FirstUseEver);
		const std::string title = GetDisplayName() + "###AnimationTimelinePanel";

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar;
		if (m_IsDraggingItem || m_IsScrubbing || m_IsDraggingHeaderSplitter) windowFlags |= ImGuiWindowFlags_NoMove;

		if (ImGui::Begin(title.c_str(), &m_Open, windowFlags))
		{
			m_Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
			m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

			DrawToolbar();
			ImGui::Separator();

			const float availWidth = ImGui::GetContentRegionAvail().x;
			const float availHeight = ImGui::GetContentRegionAvail().y;
			const float spacingX = ImGui::GetStyle().ItemSpacing.x;

			// ── 1. 상단 높이 계산 및 클램핑 (여백 1px의 오차도 없이 정확히 분할하여 가로 스크롤바 원천 차단) ──
			const float splitterThickness = 5.0f;
			const float upperHeight = std::max(120.0f, availHeight - m_TimelineHeight - splitterThickness - 4.0f);
			const float previewWidth = std::max(150.0f, availWidth - m_InspectorWidth - splitterThickness - spacingX * 2.0f);

			// ── 상단 좌측: 3D 오프스크린 뷰포트 영역 ──
			ImGui::BeginChild("##ViewportRegion", ImVec2(previewWidth, upperHeight), false);
			DrawPreviewArea();
			ImGui::EndChild();

			ImGui::SameLine();

			// ── [스플리터 1] 뷰포트 vs 인스펙터 수직 스플리터 ──
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.6f, 1.0f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 1.0f, 0.35f));
			ImGui::Button("##VerticalSplitter_Inspector", ImVec2(splitterThickness, upperHeight));
			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			if (ImGui::IsItemActive())
			{
				m_InspectorWidth -= Engine::InputManager::Get().GetMouseDelta().x;
				m_InspectorWidth = std::clamp(m_InspectorWidth, 180.0f, std::max(180.0f, availWidth - 250.0f));
			}
			ImGui::PopStyleColor(3);

			ImGui::SameLine();

			// ── 상단 우측: 노티파이 속성 인스펙터 영역 ──
			ImGui::BeginChild("##InspectorRegion", ImVec2(m_InspectorWidth, upperHeight), true);
			DrawInspector();
			ImGui::EndChild();

			// ── [스플리터 2] 상단 영역 vs 하단 타임라인 수평 스플리터 ──
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.20f, 0.24f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.6f, 1.0f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 1.0f, 0.35f));
			ImGui::Button("##HorizontalSplitter_Timeline", ImVec2(-1.0f, splitterThickness));
			if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
			if (ImGui::IsItemActive())
			{
				m_TimelineHeight -= Engine::InputManager::Get().GetMouseDelta().y;
				m_TimelineHeight = std::clamp(m_TimelineHeight, 130.0f, std::max(130.0f, availHeight - 140.0f));
			}
			ImGui::PopStyleColor(3);

			// ── 하단: 다중 트랙 타임라인 영역 ──
			DrawTimelineTrackArea();
		}
		ImGui::End();
	}

	void AnimationTimelinePanel::DrawToolbar()
	{
		// 파일 [Open] & [Save]
		if (ImGui::Button((ICON_FA_FOLDER_OPEN " " + LOCAL_LABEL("EditorCommon.Open")).c_str()))
		{
			std::wstring selectedPath;
			std::vector<FileDialogFilter> filters = {
				{ Engine::StrToWStr(LOCAL("FileDialog.ActionTimelineFiles")), L"*.actiontimeline.json" },
				{ Engine::StrToWStr(LOCAL("FileDialog.AllFiles")), L"*.*" }
			};
			if (FileDialogs::OpenFileDialog(selectedPath, filters, L"Resources/Animation"))
			{
				LoadFromFile(Engine::WStrToStr(selectedPath));
			}
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.OpenTimeline"));
		ImGui::SameLine();

		if (ImGui::Button((ICON_FA_FLOPPY_DISK " " + LOCAL_LABEL("EditorCommon.Save")).c_str()))
		{
			if (!m_CurrentAssetPath.empty())
			{
				SaveToFile(m_CurrentAssetPath);
			}
			else
			{
				std::wstring savePath;
				std::vector<FileDialogFilter> filters = {
					{ Engine::StrToWStr(LOCAL("FileDialog.ActionTimelineFiles")), L"*.actiontimeline.json" }
				};
				std::wstring defaultName = Engine::StrToWStr(m_CurrentClipName) + L".actiontimeline.json";
				if (FileDialogs::SaveFileDialog(savePath, filters, defaultName, L"actiontimeline.json"))
				{
					SaveToFile(Engine::WStrToStr(savePath));
				}
			}
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.SaveTimeline"));
		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();

		const float frameStep = 1.0f / m_FPS;

		if (ImGui::Button(ICON_FA_BACKWARD_FAST)) { m_CurrentTime = 0.0f; }
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.FirstFrame"));
		ImGui::SameLine();

		if (ImGui::Button(ICON_FA_BACKWARD)) { m_CurrentTime = std::max(0.0f, m_CurrentTime - frameStep); }
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.PrevFrame"));
		ImGui::SameLine();

		const char* playIcon = m_IsPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY;
		if (ImGui::Button(playIcon, ImVec2(32.0f, 0.0f))) { m_IsPlaying = !m_IsPlaying; }
		ImGui::SameLine();

		if (ImGui::Button(ICON_FA_FORWARD)) { m_CurrentTime = std::min(m_Duration, m_CurrentTime + frameStep); }
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.NextFrame"));
		ImGui::SameLine();

		if (ImGui::Button(ICON_FA_FORWARD_FAST)) { m_CurrentTime = m_Duration; m_IsPlaying = false; }
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.LastFrame"));
		ImGui::SameLine();

		const bool wasLooping = m_IsLooping;
		if (wasLooping) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.7f, 1.0f));
		if (ImGui::Button(ICON_FA_REPEAT)) { m_IsLooping = !m_IsLooping; }
		if (wasLooping) ImGui::PopStyleColor();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.Loop"));

		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();

		const int currentFrame = static_cast<int>(m_CurrentTime * m_FPS);
		const int totalFrames = static_cast<int>(m_Duration * m_FPS);
		const std::string timeLabel = LOCAL_FMT("Timeline.TimeCounter", m_CurrentTime, m_Duration);
		ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.5f, 1.0f), "%s", timeLabel.c_str());
		ImGui::SameLine();
		const std::string frameLabel = LOCAL_FMT("Timeline.FrameCounter", currentFrame, totalFrames);
		ImGui::TextDisabled("%s", frameLabel.c_str());

		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();

		if (ImGui::Button((ICON_FA_ARROWS_ROTATE " " + LOCAL_LABEL("Timeline.SyncEntity")).c_str()))
		{
			SyncFromSelectedEntity();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.SyncTooltip"));
		ImGui::SameLine();

		if (ImGui::Button((ICON_FA_FOLDER_OPEN " " + LOCAL_LABEL("Timeline.SelectModel")).c_str()))
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
				ImGui::SameLine();
				ImGui::SetNextItemWidth(140.0f);
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
		}

		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
		const std::string zoomLabel = LOCAL_FMT("Timeline.ZoomFactor", m_TimelineZoom);
		ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f), "%s", zoomLabel.c_str());
		ImGui::SameLine();
		auto& shortcuts = ShortcutManager::Get();
		const std::string zoomResetLabel = std::string(ICON_FA_MAXIMIZE " ") + LOCAL_LABEL("Timeline.ZoomReset");

		ImGui::BeginDisabled(!shortcuts.CanExecuteAction("Timeline.ZoomReset"));
		if (ImGui::Button(zoomResetLabel.c_str()))
			shortcuts.ExecuteAction("Timeline.ZoomReset");
		ImGui::EndDisabled();

		if (ImGui::IsItemHovered())
		{
			const char* hint = shortcuts.GetShortcutHint("Timeline.ZoomReset");
			if (hint && hint[0] != '\0')
				ImGui::SetTooltip("%s (%s)", LOCAL_CSTR("Timeline.ZoomReset"), hint);
			else
				ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.ZoomReset"));
		}

		ImGui::SameLine(ImGui::GetWindowWidth() - 110.0f);
		ImGui::SetNextItemWidth(80.0f);
		ImGui::SliderFloat("##Speed", &m_PlaybackSpeed, 0.1f, 2.0f, "%.1fx");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Timeline.PlaybackSpeed"));
	}

	void AnimationTimelinePanel::DrawPreviewArea()
	{
		auto& rtm = Engine::RenderTargetManager::Get();
		auto* colorRT = rtm.GetRenderTarget(m_DisplayRTName);
		if (colorRT && colorRT->GetTexture())
		{
			ImTextureID texID = ImGuiManager::Get().GetImGuiTextureID(colorRT->GetTexture());
			if (texID)
			{
				ImVec2 avail = ImGui::GetContentRegionAvail();
				ImVec2 cursorPos = ImGui::GetCursorScreenPos();
				ImDrawList* drawList = ImGui::GetWindowDrawList();

				m_ViewportScreenMin = cursorPos;
				m_ViewportSize = avail;

				ImGui::InvisibleButton("##TimelineViewportHitbox", avail,
					ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

				drawList->AddImage(texID, cursorPos, ImVec2(cursorPos.x + avail.x, cursorPos.y + avail.y));

				auto projectToScreen = [&](const vec3& worldPos, ImVec2& outScreenPos) -> bool
					{
						vec4 clip = m_CachedProj * m_CachedView * vec4(worldPos, 1.0f);
						if (clip.w <= 0.001f) return false;
						vec3 ndc = vec3(clip) / clip.w;
						if (ndc.z < 0.0f || ndc.z > 1.0f) return false;

						outScreenPos.x = cursorPos.x + (ndc.x * 0.5f + 0.5f) * avail.x;
						outScreenPos.y = cursorPos.y + (-ndc.y * 0.5f + 0.5f) * avail.y;
						return true;
					};

				for (const auto& p : m_PreviewParticles)
				{
					ImVec2 sPos;
					if (projectToScreen(p.Position, sPos))
					{
						const f32 alpha = 1.0f - (p.Age / p.Lifetime);
						const float radius = std::max(2.0f, p.Size * 35.0f * alpha);
						const ImU32 col = IM_COL32(
							static_cast<int>(p.Color.r * 255),
							static_cast<int>(p.Color.g * 255),
							static_cast<int>(p.Color.b * 255),
							static_cast<int>(p.Color.a * alpha * 255)
						);
						drawList->AddCircleFilled(sPos, radius, col, 10);
					}
				}

				// 선택된 노티파이 본 소켓 위치 기즈모
				if (m_Selection.Type == ETimelineSelectionType::NotifyMarker &&
					m_Selection.TrackIndex >= 0 && m_Selection.TrackIndex < static_cast<int>(m_NotifyTracks.size()))
				{
					const auto& track = m_NotifyTracks[m_Selection.TrackIndex];
					if (m_Selection.ItemIndex >= 0 && m_Selection.ItemIndex < static_cast<int>(track.Notifies.size()))
					{
						const auto& notify = track.Notifies[m_Selection.ItemIndex];
						vec3 socketPos = GetSocketWorldPosition(notify.SocketBoneName, notify.OffsetPosition);
						ImVec2 sPos;
						if (projectToScreen(socketPos, sPos))
						{
							drawList->AddCircle(sPos, 8.0f, IM_COL32(255, 220, 50, 240), 12, 2.0f);
							drawList->AddCircleFilled(sPos, 4.0f, IM_COL32(255, 100, 50, 255), 10);
							std::string label = notify.SocketBoneName.empty() ? LOCAL("Timeline.Root") : notify.SocketBoneName;
							drawList->AddText(ImVec2(sPos.x + 10.0f, sPos.y - 8.0f), IM_COL32(255, 230, 100, 230), label.c_str());
						}
					}
				}

				if (m_TriggerToastTimer > 0.0f)
				{
					const std::string toastText = std::string(ICON_FA_BELL " ") + LOCAL_FMT("Timeline.NotifyTriggered", m_LastTriggeredNotifyName);
					const ImVec2 textSize = ImGui::CalcTextSize(toastText.c_str());
					const ImVec2 bannerMin(cursorPos.x + (avail.x - textSize.x) * 0.5f - 16.0f, cursorPos.y + 16.0f);
					const ImVec2 bannerMax(bannerMin.x + textSize.x + 32.0f, bannerMin.y + textSize.y + 12.0f);
					const f32 toastAlpha = std::min(1.0f, m_TriggerToastTimer);

					drawList->AddRectFilled(bannerMin, bannerMax, IM_COL32(25, 30, 40, static_cast<int>(220 * toastAlpha)), 6.0f);
					drawList->AddRect(bannerMin, bannerMax, IM_COL32(255, 180, 50, static_cast<int>(240 * toastAlpha)), 6.0f, 0, 1.5f);
					drawList->AddText(ImVec2(bannerMin.x + 16.0f, bannerMin.y + 6.0f), IM_COL32(255, 220, 120, static_cast<int>(255 * toastAlpha)), toastText.c_str());
				}

				if (ImGui::IsItemHovered())
				{
					auto& input = Engine::InputManager::Get();
					const Engine::vec2 delta = input.GetMouseDelta();

					if (input.IsMouseButtonDragging(Engine::EMouseButton::Left))
					{
						m_CamYaw += delta.x * 0.015f;
						m_CamPitch = std::clamp(m_CamPitch - delta.y * 0.015f, -1.5f, 1.5f);
					}
					if (input.IsMouseButtonDragging(Engine::EMouseButton::Right) || input.IsMouseButtonDragging(Engine::EMouseButton::Middle))
					{
						vec3 forward = glm::normalize(m_CamTarget - (m_CamTarget + vec3(sinf(m_CamYaw), 0.0f, -cosf(m_CamYaw))));
						vec3 right = glm::normalize(glm::cross(vec3(0.0f, 1.0f, 0.0f), forward));
						m_CamTarget += right * (-delta.x * 0.005f * m_CamDistance);
						m_CamTarget.y += delta.y * 0.005f * m_CamDistance;
					}
					const float wheel = Engine::InputManager::Get().GetMouseScrollDeltaY();
					if (wheel != 0.0f)
					{
						m_CamDistance = std::clamp(m_CamDistance - wheel * 0.35f, 0.5f, 25.0f);
					}
				}
			}
		}
		else
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("Timeline.PreviewPlaceholder"));
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
			notify.Name = nameBuf;

		ImGui::SliderFloat(LOCAL_LABEL("Timeline.TriggerTime").c_str(), &notify.TriggerTime, 0.0f, m_Duration, "%.2fs");

		const std::string typeNames[] = {
			LOCAL("Timeline.NotifyTypeParticle"), LOCAL("Timeline.NotifyTypeSound"), LOCAL("Timeline.NotifyTypeCustomEvent")
		};
		const char* typeItems[] = { typeNames[0].c_str(), typeNames[1].c_str(), typeNames[2].c_str() };
		int currentType = static_cast<int>(notify.Type);
		if (ImGui::Combo(LOCAL_LABEL("Timeline.NotifyType").c_str(), &currentType, typeItems, IM_ARRAYSIZE(typeItems)))
			notify.Type = static_cast<Engine::EAnimNotifyType>(currentType);

		auto* model = m_TargetModel.Get();
		auto skeleton = model ? model->GetSkeleton() : Engine::ResourceHandle<Engine::Skeleton>();
		if (skeleton.IsValid())
		{
			const auto& bones = skeleton->GetBones();
			const std::string currentBoneDisplay = notify.SocketBoneName.empty()
				? LOCAL("Timeline.RootOrNone") : notify.SocketBoneName;

			if (ImGui::BeginCombo(LOCAL_LABEL("Timeline.SocketBone").c_str(), currentBoneDisplay.c_str()))
			{
				if (ImGui::Selectable(LOCAL_LABEL("Timeline.RootOrNone").c_str(), notify.SocketBoneName.empty()))
					notify.SocketBoneName.clear();

				for (const auto& bone : bones)
				{
					const std::string boneNameStr = Engine::WStrToStr(bone.Name);
					const bool isSel = notify.SocketBoneName == boneNameStr;
					if (ImGui::Selectable(boneNameStr.c_str(), isSel))
						notify.SocketBoneName = boneNameStr;

					if (isSel)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		ImGui::DragFloat3(LOCAL_LABEL("Timeline.OffsetPos").c_str(), &notify.OffsetPosition.x, 0.02f, -10.0f, 10.0f, "%.2f");

		if (notify.Type == Engine::EAnimNotifyType::Particle)
		{
			ImGui::Separator();
			ImGui::ColorEdit4(LOCAL_LABEL("Timeline.ParticleColor").c_str(), &notify.ParticleColor.x, ImGuiColorEditFlags_NoInputs);

			int count = static_cast<int>(notify.BurstCount);
			if (ImGui::SliderInt(LOCAL_LABEL("Timeline.BurstCount").c_str(), &count, 5, 80))
				notify.BurstCount = static_cast<uint32>(count);

			ImGui::SliderFloat(LOCAL_LABEL("Timeline.ParticleSpeed").c_str(), &notify.ParticleSpeed, 0.2f, 5.0f, "%.1f");
			ImGui::SliderFloat(LOCAL_LABEL("PROP_SIZE").c_str(), &notify.ParticleSize, 0.02f, 0.5f, "%.2f");
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
					notify.SoundPath = Engine::WStrToStr(soundPath);
			}

			ImGui::SliderFloat(LOCAL_LABEL("PROP_VOLUME").c_str(), &notify.SoundVolume, 0.0f, 2.0f, "%.2f");
		}
		else if (notify.Type == Engine::EAnimNotifyType::CustomEvent)
		{
			ImGui::Separator();
			char tagBuf[64];
			snprintf(tagBuf, sizeof(tagBuf), "%s", notify.EventTag.c_str());
			if (ImGui::InputText(LOCAL_LABEL("Timeline.EventTag").c_str(), tagBuf, sizeof(tagBuf)))
				notify.EventTag = tagBuf;
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
		if (ImGui::Button(deleteLabel.c_str()))
			DeleteSelected();
		ImGui::EndDisabled();
	}

	void AnimationTimelinePanel::DrawCameraShotInspector(Engine::TimelineCameraTrack& camShot)
	{
		char nameBuf[64];
		snprintf(nameBuf, sizeof(nameBuf), "%s", camShot.TrackName.c_str());
		if (ImGui::InputText(LOCAL_LABEL("Timeline.CameraShotName").c_str(), nameBuf, sizeof(nameBuf)))
			camShot.TrackName = nameBuf;

		ImGui::Checkbox(LOCAL_LABEL("PROP_ENABLED").c_str(), &camShot.IsActive);
		ImGui::SliderFloat(LOCAL_LABEL("Timeline.CameraShotStartTime").c_str(), &camShot.StartTime, 0.0f, m_Duration, "%.2fs");
		ImGui::SliderFloat(LOCAL_LABEL("Timeline.CameraShotDuration").c_str(), &camShot.Duration, 0.05f, 5.0f, "%.2fs");
		ImGui::SliderFloat(LOCAL_LABEL("Timeline.CameraShotTargetFOV").c_str(), &camShot.TargetFOV, 15.0f, 90.0f, "%.1f deg");
		ImGui::SliderFloat(LOCAL_LABEL("Timeline.CameraShotShakeIntensity").c_str(), &camShot.ShakeIntensity, 0.0f, 1.5f, "%.2f");

		auto* model = m_TargetModel.Get();
		auto skeleton = model ? model->GetSkeleton() : Engine::ResourceHandle<Engine::Skeleton>();
		if (skeleton.IsValid())
		{
			const auto& bones = skeleton->GetBones();
			const std::string currentBoneDisplay = camShot.FocusBoneName.empty()
				? LOCAL("Timeline.RootOrNone") : camShot.FocusBoneName;

			if (ImGui::BeginCombo(LOCAL_LABEL("Timeline.CameraShotFocusBone").c_str(), currentBoneDisplay.c_str()))
			{
				if (ImGui::Selectable(LOCAL_LABEL("Timeline.RootOrNone").c_str(), camShot.FocusBoneName.empty()))
					camShot.FocusBoneName.clear();

				for (const auto& bone : bones)
				{
					const std::string boneNameStr = Engine::WStrToStr(bone.Name);
					const bool isSelected = camShot.FocusBoneName == boneNameStr;
					if (ImGui::Selectable(boneNameStr.c_str(), isSelected))
						camShot.FocusBoneName = boneNameStr;

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		const std::string deleteLabel = std::string(ICON_FA_TRASH " ") + LOCAL_LABEL("EditorCommon.Delete");
		ImGui::BeginDisabled(!CanDeleteSelected());
		if (ImGui::Button(deleteLabel.c_str()))
			DeleteSelected();
		ImGui::EndDisabled();
	}

	void AnimationTimelinePanel::DrawTimelineTrackArea()
	{
		ImGui::BeginChild("##TimelineTracksScrollArea", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 contentMin = ImGui::GetCursorScreenPos();
		const ImVec2 contentSize = ImGui::GetContentRegionAvail();
		auto& input = Engine::InputManager::Get();
		const auto mousePos = input.GetScreenMousePosition();
		const bool isMouseDown = input.IsMouseButtonPressed(Engine::EMouseButton::Left);
		const bool isMouseClicked = input.IsMouseButtonDown(Engine::EMouseButton::Left);

		// ── [스플리터 위젯] InvisibleButton 배치 후 원래 커서 좌표를 복원하여 가로 스크롤바 발생 원천 제거! ──
		const float rulerHeight = 26.0f;
		const float splitterHitWidth = 8.0f;
		const ImVec2 splitterPos(contentMin.x + m_TrackHeaderWidth - splitterHitWidth * 0.5f, contentMin.y);

		const ImVec2 backupCursorPos = ImGui::GetCursorPos();
		ImGui::SetCursorScreenPos(splitterPos);
		ImGui::InvisibleButton("##HeaderSplitterHitbox", ImVec2(splitterHitWidth, contentSize.y));
		const bool isSplitterHovered = ImGui::IsItemHovered();
		const bool isSplitterActive = ImGui::IsItemActive();
		ImGui::SetCursorPos(backupCursorPos); // 커서 복원!

		if (isSplitterHovered || isSplitterActive)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}
		if (isSplitterActive && isMouseDown)
		{
			m_TrackHeaderWidth += input.GetMouseDelta().x;
			m_TrackHeaderWidth = std::clamp(m_TrackHeaderWidth, 90.0f, std::min(450.0f, contentSize.x - 120.0f));
		}

		// ── 정확한 너비 산출 (스크롤바 너비 침범 방지) ──
		const float totalFrames = std::max(1.0f, m_Duration * m_FPS);
		const float timelineWidth = std::max(10.0f, contentSize.x - m_TrackHeaderWidth - 4.0f);
		const ImVec2 timelineMin(contentMin.x + m_TrackHeaderWidth, contentMin.y);
		const ImVec2 timelineMax(timelineMin.x + timelineWidth, contentMin.y + contentSize.y);

		const ImVec2 rulerMin = timelineMin;
		const ImVec2 rulerMax(timelineMax.x, timelineMin.y + rulerHeight);

		// ── 눈금자 전용 상호작용 히트박스 ──
		ImGui::SetCursorScreenPos(rulerMin);
		ImGui::InvisibleButton("##TimelineRulerHitbox", ImVec2(timelineWidth, rulerHeight));
		const bool isRulerHovered = ImGui::IsItemHovered();

		const bool isOverTimelineArea = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
			mousePos.x >= contentMin.x && mousePos.x <= timelineMax.x &&
			mousePos.y >= contentMin.y && mousePos.y <= timelineMax.y;

		// 마우스 휠 줌(Zoom) & 중간클릭 패닝(Pan)
		if (isOverTimelineArea)
		{
			const float wheel = input.GetMouseScrollDeltaY();
			if (wheel != 0.0f)
			{
				const float mouseX = mousePos.x;
				const float mouseTime = ScreenXToTime(mouseX, timelineMin.x, timelineWidth);
				const float ratio = (mouseX - timelineMin.x) / timelineWidth;

				const float zoomFactor = (wheel > 0.0f) ? 1.25f : 0.8f;
				m_TimelineZoom = std::clamp(m_TimelineZoom * zoomFactor, 1.0f, 40.0f);

				const float newVisibleDuration = (m_Duration > 0.0f) ? (m_Duration / m_TimelineZoom) : 1.0f;
				m_ViewStartTime = std::clamp(mouseTime - ratio * newVisibleDuration, 0.0f, std::max(0.0f, m_Duration - newVisibleDuration));
			}

			if (input.IsMouseButtonDragging(Engine::EMouseButton::Middle) || (input.IsAltPressed() && input.IsMouseButtonDragging(Engine::EMouseButton::Right)))
			{
				const float deltaX = input.GetMouseDelta().x;
				const float visibleDuration = (m_Duration > 0.0f) ? (m_Duration / m_TimelineZoom) : 1.0f;
				const float deltaTime = -(deltaX / timelineWidth) * visibleDuration;
				m_ViewStartTime = std::clamp(m_ViewStartTime + deltaTime, 0.0f, std::max(0.0f, m_Duration - visibleDuration));
			}
		}

		if (isRulerHovered && isMouseClicked && !m_IsDraggingItem)
		{
			m_IsScrubbing = true;
			m_IsPlaying = false;
		}
		if (m_IsScrubbing)
		{
			if (isMouseDown)
			{
				m_CurrentTime = ScreenXToTime(mousePos.x, timelineMin.x, timelineWidth);
			}
			else
			{
				m_IsScrubbing = false;
			}
		}

		// ── 눈금자 좌측 상단 헤더 코너 렌더링 ──
		const ImVec2 cornerMin(contentMin.x, contentMin.y);
		const ImVec2 cornerMax(timelineMin.x, rulerMax.y);
		drawList->AddRectFilled(cornerMin, cornerMax, IM_COL32(24, 26, 30, 255));
		const std::string tracksLabel = std::string(ICON_FA_LAYER_GROUP " ") + LOCAL("Timeline.Tracks");
		drawList->AddText(ImVec2(cornerMin.x + 8.0f, cornerMin.y + 5.0f), IM_COL32(170, 180, 195, 255), tracksLabel.c_str());
		drawList->AddLine(ImVec2(cornerMin.x, rulerMax.y), rulerMax, IM_COL32(50, 54, 62, 255));

		// 헤더와 본체 사이의 수직 스플리터 라인 (활성/호버 시 하이라이트)
		const ImU32 splitterCol = (isSplitterHovered || isSplitterActive) ? IM_COL32(80, 160, 255, 240) : IM_COL32(55, 60, 70, 255);
		const float splitterThicknessDraw = (isSplitterHovered || isSplitterActive) ? 2.5f : 1.0f;
		drawList->AddLine(ImVec2(timelineMin.x, cornerMin.y), ImVec2(timelineMin.x, timelineMax.y), splitterCol, splitterThicknessDraw);

		// ── 눈금자 렌더링 (타임라인 본체 영역만 클리핑) ──
		drawList->PushClipRect(timelineMin, rulerMax, true);
		DrawRuler(drawList, rulerMin, rulerMax, totalFrames);
		drawList->PopClipRect();

		// ── 각 트랙 렌더링 (헤더와 본체가 각각 정상 노출됨) ──
		float currentY = rulerMax.y;

		// 1. 베이스 애니메이션 클립 트랙
		const ImVec2 animTrackMin(timelineMin.x, currentY);
		const ImVec2 animTrackMax(timelineMax.x, currentY + m_TrackHeight);
		DrawAnimPoseTrack(drawList, animTrackMin, animTrackMax, timelineWidth);
		currentY += m_TrackHeight;

		// 2. 다중 노티파이 트랙 목록
		for (size_t i = 0; i < m_NotifyTracks.size(); ++i)
		{
			const ImVec2 tMin(timelineMin.x, currentY);
			const ImVec2 tMax(timelineMax.x, currentY + m_TrackHeight);
			DrawNotifyTrackRow(drawList, static_cast<int>(i), m_NotifyTracks[i], tMin, tMax, timelineWidth);
			currentY += m_TrackHeight;
		}

		// 3. 다중 카메라 연출 트랙 목록
		for (size_t i = 0; i < m_CameraTracks.size(); ++i)
		{
			const ImVec2 tMin(timelineMin.x, currentY);
			const ImVec2 tMax(timelineMax.x, currentY + m_TrackHeight);
			DrawCameraTrackRow(drawList, static_cast<int>(i), m_CameraTracks[i], tMin, tMax, timelineWidth);
			currentY += m_TrackHeight;
		}

		// ── 수직 재생 헤드 라인 ──
		const float curX = TimeToScreenX(m_CurrentTime, timelineMin.x, timelineWidth);
		drawList->PushClipRect(timelineMin, ImVec2(timelineMax.x, currentY), true);
		DrawPlayhead(drawList, timelineMin, ImVec2(timelineMax.x, currentY), curX);
		drawList->PopClipRect();

		// ── 트랙 아래 빈 공간 또는 눈금자 우클릭 시 트랙 추가 메뉴 ──
		const bool isRulerRightClicked = (isRulerHovered && input.IsMouseButtonDown(Engine::EMouseButton::Right));
		const bool isEmptyAreaRightClicked = isOverTimelineArea && mousePos.y > currentY &&
			input.IsMouseButtonDown(Engine::EMouseButton::Right);

		if (isRulerRightClicked || isEmptyAreaRightClicked)
		{
			ImGui::OpenPopup("##MultiTrackContextMenu");
		}
		if (ImGui::BeginPopup("##MultiTrackContextMenu"))
		{
			const float clickTime = ScreenXToTime(mousePos.x, timelineMin.x, timelineWidth);

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
				newCam.StartTime = clickTime;
				newCam.Duration = 0.5f;
				newCam.TargetFOV = 35.0f;
				m_CameraTracks.push_back(newCam);
				m_IsDirty = true;
			}

			ImGui::Separator();
			ACTION_MENU_ITEM((ICON_FA_MAXIMIZE " " + LOCAL_LABEL("Timeline.ZoomReset")).c_str(), "Timeline.ZoomReset");
			ImGui::EndPopup();
		}

		ImGui::EndChild();
	}

	void AnimationTimelinePanel::DrawTrackGridLines(ImDrawList* drawList, const ImVec2& trackMin, const ImVec2& trackMax, float timelineWidth)
	{
		const float visibleDuration = (m_Duration > 0.0f) ? (m_Duration / m_TimelineZoom) : 1.0f;
		const int startFrame = static_cast<int>(std::floor(m_ViewStartTime * m_FPS));
		const int endFrame = static_cast<int>(std::ceil((m_ViewStartTime + visibleDuration) * m_FPS));
		const int visibleFrames = std::max(1, endFrame - startFrame);
		const float totalFrames = std::max(1.0f, m_Duration * m_FPS);

		const float pxPerFrame = timelineWidth / static_cast<float>(visibleFrames);

		int step = 1;
		if (pxPerFrame < 3.0f) step = 5;
		if (pxPerFrame < 1.5f) step = 10;
		if (pxPerFrame < 0.5f) step = 30;
		if (pxPerFrame < 0.15f) step = 60;

		const int firstTick = (startFrame / step) * step;
		for (int f = firstTick; f <= endFrame; f += step)
		{
			if (f < 0 || f > static_cast<int>(totalFrames)) continue;

			const float time = static_cast<float>(f) / m_FPS;
			const float x = TimeToScreenX(time, trackMin.x, timelineWidth);
			if (x < trackMin.x || x > trackMax.x) continue;

			const bool isMajor = (f % (step * 5) == 0);
			const ImU32 gridColor = isMajor ? IM_COL32(70, 78, 92, 160) : IM_COL32(45, 50, 60, 95);
			drawList->AddLine(ImVec2(x, trackMin.y), ImVec2(x, trackMax.y), gridColor, 1.0f);
		}
	}

	void AnimationTimelinePanel::DrawAnimPoseTrack(ImDrawList* drawList, const ImVec2& trackMin, const ImVec2& trackMax, float timelineWidth)
	{
		const ImVec2 headerMin(trackMin.x - m_TrackHeaderWidth, trackMin.y);
		const ImVec2 headerMax(trackMin.x, trackMax.y);
		drawList->AddRectFilled(headerMin, headerMax, IM_COL32(35, 38, 44, 255));
		drawList->AddText(ImVec2(headerMin.x + 8.0f, headerMin.y + 7.0f), IM_COL32(200, 210, 225, 255), (std::string(ICON_FA_PERSON_RUNNING " ") + LOCAL("Timeline.Pose")).c_str());

		// 본체 영역 ImGui 히트박스 배치 (마우스 클릭 소비로 창 이동 원천 차단)
		ImGui::SetCursorScreenPos(trackMin);
		ImGui::InvisibleButton("##AnimPoseTrackHitbox", ImVec2(timelineWidth, m_TrackHeight));

		drawList->PushClipRect(trackMin, trackMax, true);
		drawList->AddRectFilled(trackMin, trackMax, IM_COL32(40, 44, 52, 255));
		DrawTrackGridLines(drawList, trackMin, trackMax, timelineWidth);

		const float clipStartX = TimeToScreenX(0.0f, trackMin.x, timelineWidth);
		const float clipEndX = TimeToScreenX(m_Duration, trackMin.x, timelineWidth);
		drawList->AddRectFilled(ImVec2(clipStartX, trackMin.y + 4.0f), ImVec2(clipEndX, trackMax.y - 4.0f), IM_COL32(45, 95, 160, 220), 4.0f);
		drawList->AddText(ImVec2(std::max(trackMin.x + 8.0f, clipStartX + 12.0f), trackMin.y + 7.0f), IM_COL32(255, 255, 255, 255), m_CurrentClipName.c_str());
		drawList->PopClipRect();

		drawList->AddLine(ImVec2(headerMin.x, trackMax.y), trackMax, IM_COL32(50, 54, 62, 255));
	}

	void AnimationTimelinePanel::DrawNotifyTrackRow(ImDrawList* drawList, int trackIdx, Engine::TimelineNotifyTrack& track, const ImVec2& trackMin, const ImVec2& trackMax, float timelineWidth)
	{
		const ImVec2 headerMin(trackMin.x - m_TrackHeaderWidth, trackMin.y);
		const ImVec2 headerMax(trackMin.x, trackMax.y);
		drawList->AddRectFilled(headerMin, headerMax, IM_COL32(30, 33, 38, 255));

		const float headerCenterY = (headerMin.y + headerMax.y) * 0.5f;
		auto& input = Engine::InputManager::Get();
		const auto mousePos = input.GetScreenMousePosition();
		const bool isMouseDown = input.IsMouseButtonPressed(Engine::EMouseButton::Left);
		const bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		const bool isMouseClicked = windowHovered && input.IsMouseButtonDown(Engine::EMouseButton::Left);

		// 1. 트랙 헤더 Mute 아이콘 버튼 (x + 10.0f, 수직 중앙 정렬)
		const char* muteIcon = track.IsMuted ? ICON_FA_VOLUME_XMARK : ICON_FA_VOLUME_HIGH;
		const ImU32 muteColor = track.IsMuted ? IM_COL32(180, 70, 70, 255) : IM_COL32(120, 180, 240, 255);
		const ImVec2 muteSize = ImGui::CalcTextSize(muteIcon);
		const ImVec2 muteBtnPos(headerMin.x + 10.0f, headerCenterY - muteSize.y * 0.5f);
		drawList->AddText(muteBtnPos, muteColor, muteIcon);

		const bool isMuteHovered = windowHovered && (mousePos.x >= headerMin.x + 8.0f && mousePos.x <= headerMin.x + 32.0f && mousePos.y >= headerMin.y && mousePos.y <= headerMax.y);
		if (isMuteHovered && isMouseClicked)
		{
			track.IsMuted = !track.IsMuted;
			m_IsDirty = true;
		}

		// 2. 트랙 성격 아이콘 (x + 40.0f: Mute 버튼과 30px의 넉넉한 간격 확보, 수직 중앙 정렬)
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
		if (track.IsMuted) trackIconColor = IM_COL32(110, 110, 110, 200);

		const ImVec2 iconSize = ImGui::CalcTextSize(trackTypeIcon);
		drawList->AddText(ImVec2(headerMin.x + 40.0f, headerCenterY - iconSize.y * 0.5f), trackIconColor, trackTypeIcon);

		// 3. 트랙 이름 표시 (x + 72.0f: 별 아이콘과 32px 여유 간격 확보, 수직 중앙 정렬)
		if (m_RenamingTrackIndex == trackIdx)
		{
			ImGui::SetCursorScreenPos(ImVec2(headerMin.x + 70.0f, headerCenterY - 11.0f));
			ImGui::SetNextItemWidth(m_TrackHeaderWidth - 78.0f);
			if (ImGui::InputText("##RenameTrack", m_RenameBuf, sizeof(m_RenameBuf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
			{
				track.TrackName = m_RenameBuf;
				m_RenamingTrackIndex = -1;
				m_IsDirty = true;
			}
			if (!ImGui::IsItemActive() && isMouseClicked)
			{
				m_RenamingTrackIndex = -1;
			}
		}
		else
		{
			const ImU32 textColor = track.IsMuted ? IM_COL32(110, 110, 110, 200) : IM_COL32(230, 210, 160, 255);
			const ImVec2 nameSize = ImGui::CalcTextSize(track.TrackName.c_str());
			drawList->AddText(ImVec2(headerMin.x + 72.0f, headerCenterY - nameSize.y * 0.5f), textColor, track.TrackName.c_str());
		}

		// 4. 트랙 헤더 영역 우클릭 컨텍스트 메뉴
		const bool isHeaderHovered = (mousePos.x >= headerMin.x && mousePos.x <= headerMax.x && mousePos.y >= headerMin.y && mousePos.y <= headerMax.y && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem));
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

		// 5. 트랙 타임라인 본체 렌더링
		// 본체 영역 ImGui 히트박스 배치 (클릭 즉시 ActiveId를 점유하여 창 전체 드래그 이동 원천 차단!)
		ImGui::SetCursorScreenPos(trackMin);
		const std::string trackHitboxId = "##NotifyTrackHitbox_" + std::to_string(trackIdx);
		ImGui::InvisibleButton(trackHitboxId.c_str(), ImVec2(timelineWidth, m_TrackHeight), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		const bool isTrackHovered = ImGui::IsItemHovered();
		const bool isTrackActive = ImGui::IsItemActive();
		drawList->PushClipRect(trackMin, trackMax, true);
		drawList->AddRectFilled(trackMin, trackMax, IM_COL32(32, 35, 42, 255));
		DrawTrackGridLines(drawList, trackMin, trackMax, timelineWidth);
		const float centerY = (trackMin.y + trackMax.y) * 0.5f;
		const float markerHalfSize = 7.0f;
		if (!isMouseDown)
		{
			m_IsDraggingItem = false;
			m_DragTimeOffset = 0.0f;
		}
		// 6. 노티파이 다이아몬드 마커 렌더링 및 드래그
		bool anyMarkerHovered = false;
		for (size_t nIdx = 0; nIdx < track.Notifies.size(); ++nIdx)
		{
			auto& notify = track.Notifies[nIdx];
			const float markerX = TimeToScreenX(notify.TriggerTime, trackMin.x, timelineWidth);
			if (markerX < trackMin.x - 15.0f || markerX > trackMin.x + timelineWidth + 15.0f) continue;
			const ImVec2 pTop(markerX, centerY - markerHalfSize);
			const ImVec2 pRight(markerX + markerHalfSize, centerY);
			const ImVec2 pBottom(markerX, centerY + markerHalfSize);
			const ImVec2 pLeft(markerX - markerHalfSize, centerY);
			const bool isMarkerHovered = isTrackHovered && (std::abs(mousePos.x - markerX) <= markerHalfSize + 2.5f && std::abs(mousePos.y - centerY) <= markerHalfSize + 2.5f);
			if (isMarkerHovered) anyMarkerHovered = true;
			const bool isSelected = (m_Selection.Type == ETimelineSelectionType::NotifyMarker && m_Selection.TrackIndex == trackIdx && m_Selection.ItemIndex == static_cast<int>(nIdx));
			// 마커 클릭 시 잡은 지점(Time Offset) 기록
			if (isMarkerHovered && isMouseClicked)
			{
				m_Selection = { ETimelineSelectionType::NotifyMarker, trackIdx, static_cast<int>(nIdx) };
				m_IsDraggingItem = true;
				const float clickTime = ScreenXToTime(mousePos.x, trackMin.x, timelineWidth);
				m_DragTimeOffset = clickTime - notify.TriggerTime;
			}
			// 드래그 시 잡은 오프셋을 유지하며 즉각 프레임 이동 반영
			if (m_IsDraggingItem && isSelected && isMouseDown)
			{
				const float curMouseTime = ScreenXToTime(mousePos.x, trackMin.x, timelineWidth);
				notify.TriggerTime = std::clamp(curMouseTime - m_DragTimeOffset, 0.0f, m_Duration);
				m_IsDirty = true;
			}
			ImU32 markerColor = IM_COL32(255, 160, 40, 240);
			if (notify.Type == Engine::EAnimNotifyType::Sound) markerColor = IM_COL32(60, 200, 240, 240);
			else if (notify.Type == Engine::EAnimNotifyType::CustomEvent) markerColor = IM_COL32(190, 100, 255, 240);
			drawList->AddQuadFilled(pTop, pRight, pBottom, pLeft, markerColor);
			drawList->AddQuad(pTop, pRight, pBottom, pLeft, isSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(20, 20, 20, 200), isSelected ? 2.0f : 1.0f);
			if (isMarkerHovered)
			{
				ImGui::BeginTooltip();
				ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[%s]", notify.Name.c_str());
				ImGui::Text(LOCAL_CSTR("Timeline.MarkerTimeSocket"), notify.TriggerTime, notify.SocketBoneName.empty() ? LOCAL_CSTR("Timeline.Root") : notify.SocketBoneName.c_str());
				if (notify.Type == Engine::EAnimNotifyType::Sound && !notify.SoundPath.empty())
				{
					ImGui::TextDisabled(LOCAL_CSTR("Timeline.Sound"), Engine::WStrToStr(std::filesystem::path(Engine::StrToWStr(notify.SoundPath)).filename().wstring()).c_str());
				}
				ImGui::EndTooltip();
			}
		}
		drawList->PopClipRect();
		// 하단 구분선
		drawList->AddLine(ImVec2(headerMin.x, trackMax.y), trackMax, IM_COL32(50, 54, 62, 255));
		// 7. 트랙 빈 공간 클릭 시 선택 해제
		if (isTrackHovered && !anyMarkerHovered && isMouseClicked)
		{
			m_IsDraggingItem = false;
			m_DragTimeOffset = 0.0f;
			m_Selection = {};
		}
		// 8. 트랙 빈 공간 더블클릭 시 새 마커 추가
		if (isTrackHovered && !anyMarkerHovered && input.IsMouseButtonDoubleClicked(Engine::EMouseButton::Left))
		{
			const float clickTime = ScreenXToTime(mousePos.x, trackMin.x, timelineWidth);
			Engine::AnimNotifyData newMarker{};
			const bool isSoundTrack = (track.TrackName.find("Audio") != std::string::npos || track.TrackName.find("Sound") != std::string::npos);
			newMarker.Name = isSoundTrack ? "Sound_Cue" : "FX_Burst";
			newMarker.TriggerTime = clickTime;
			newMarker.Type = isSoundTrack ? Engine::EAnimNotifyType::Sound : Engine::EAnimNotifyType::Particle;
			track.Notifies.push_back(newMarker);
			m_Selection = { ETimelineSelectionType::NotifyMarker, trackIdx, static_cast<int>(track.Notifies.size()) - 1 };
			m_IsDirty = true;
		}
		// 9. 트랙 바디 우클릭 컨텍스트 메뉴
		if (isTrackHovered && input.IsMouseButtonDown(Engine::EMouseButton::Right))
		{
			ImGui::OpenPopup(("##BodyPopup_" + std::to_string(trackIdx)).c_str());
		}
	}

	void AnimationTimelinePanel::DrawCameraTrackRow(ImDrawList* drawList, int camIdx, Engine::TimelineCameraTrack& camTrack, const ImVec2& trackMin, const ImVec2& trackMax, float timelineWidth)
	{
		const ImVec2 headerMin(trackMin.x - m_TrackHeaderWidth, trackMin.y);
		const ImVec2 headerMax(trackMin.x, trackMax.y);
		drawList->AddRectFilled(headerMin, headerMax, IM_COL32(38, 30, 42, 255));

		const float headerCenterY = (headerMin.y + headerMax.y) * 0.5f;
		auto& input = Engine::InputManager::Get();
		const auto mousePos = input.GetScreenMousePosition();
		const bool isMouseDown = input.IsMouseButtonPressed(Engine::EMouseButton::Left);
		const bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		const bool isMouseClicked = windowHovered && input.IsMouseButtonDown(Engine::EMouseButton::Left);

		// 1. 카메라 활성화/비활성화 아이콘 토글 (x + 10.0f, 수직 중앙 정렬)
		const char* activeIcon = camTrack.IsActive ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
		const ImU32 activeColor = camTrack.IsActive ? IM_COL32(180, 140, 240, 255) : IM_COL32(120, 120, 120, 200);
		const ImVec2 eyeSize = ImGui::CalcTextSize(activeIcon);
		const ImVec2 activeBtnPos(headerMin.x + 10.0f, headerCenterY - eyeSize.y * 0.5f);
		drawList->AddText(activeBtnPos, activeColor, activeIcon);

		const bool isActiveHovered = windowHovered && (mousePos.x >= headerMin.x + 8.0f && mousePos.x <= headerMin.x + 32.0f && mousePos.y >= headerMin.y && mousePos.y <= headerMax.y);
		if (isActiveHovered && isMouseClicked)
		{
			camTrack.IsActive = !camTrack.IsActive;
			m_IsDirty = true;
		}

		// 2. 카메라 트랙 아이콘 (x + 40.0f: 눈 아이콘과 30px 간격 확보, 수직 중앙 정렬)
		const ImU32 camIconColor = camTrack.IsActive ? IM_COL32(200, 130, 255, 255) : IM_COL32(120, 120, 120, 200);
		const ImVec2 camIconSize = ImGui::CalcTextSize(ICON_FA_VIDEO);
		drawList->AddText(ImVec2(headerMin.x + 40.0f, headerCenterY - camIconSize.y * 0.5f), camIconColor, ICON_FA_VIDEO);

		// 3. 트랙 이름 표시 (x + 72.0f: 카메라 아이콘과 32px 여유 간격 확보, 수직 중앙 정렬)
		const ImU32 textColor = camTrack.IsActive ? IM_COL32(220, 190, 255, 255) : IM_COL32(120, 120, 120, 200);
		const ImVec2 nameSize = ImGui::CalcTextSize(camTrack.TrackName.c_str());
		drawList->AddText(ImVec2(headerMin.x + 72.0f, headerCenterY - nameSize.y * 0.5f), textColor, camTrack.TrackName.c_str());

		// 4. 트랙 헤더 우클릭 컨텍스트 메뉴
		const bool isHeaderHovered = (mousePos.x >= headerMin.x && mousePos.x <= headerMax.x && mousePos.y >= headerMin.y && mousePos.y <= headerMax.y && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem));
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

		// 5. 본체 영역 클리핑 렌더링
// 본체 영역 ImGui 히트박스 배치 (클릭 즉시 ActiveId 점유)
		ImGui::SetCursorScreenPos(trackMin);
		const std::string camHitboxId = "##CamTrackHitbox_" + std::to_string(camIdx);
		ImGui::InvisibleButton(camHitboxId.c_str(), ImVec2(timelineWidth, m_TrackHeight), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		const bool isTrackHovered = ImGui::IsItemHovered();
		const bool isTrackActive = ImGui::IsItemActive();

		drawList->PushClipRect(trackMin, trackMax, true);
		drawList->AddRectFilled(trackMin, trackMax, IM_COL32(32, 28, 36, 255));
		DrawTrackGridLines(drawList, trackMin, trackMax, timelineWidth);

		const float shotStartX = TimeToScreenX(camTrack.StartTime, trackMin.x, timelineWidth);
		const float shotEndX = TimeToScreenX(camTrack.StartTime + camTrack.Duration, trackMin.x, timelineWidth);
		const ImVec2 shotMin(shotStartX, trackMin.y + 4.0f);
		const ImVec2 shotMax(shotEndX, trackMax.y - 4.0f);

		const bool isSelected = (m_Selection.Type == ETimelineSelectionType::CameraShot && m_Selection.TrackIndex == camIdx);
		const bool isShotHovered = isTrackHovered && (mousePos.x >= shotMin.x && mousePos.x <= shotMax.x && mousePos.y >= shotMin.y && mousePos.y <= shotMax.y);

		drawList->AddRectFilled(shotMin, shotMax, isSelected ? IM_COL32(160, 70, 220, 230) : IM_COL32(110, 45, 160, 210), 4.0f);
		drawList->AddRect(shotMin, shotMax, isSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 140, 240, 200), 4.0f, 0, isSelected ? 2.0f : 1.0f);

		// ── 샷 라벨 렌더링 (디센더 보정 -1.0f 적용으로 완벽한 수직 중앙 정렬) ──
		const std::string shotLabel = std::string(ICON_FA_VIDEO " ") + LOCAL("Timeline.Shot");
		const ImVec2 labelSize = ImGui::CalcTextSize(shotLabel.c_str());
		const float shotCenterY = (shotMin.y + shotMax.y) * 0.5f;
		const float labelPosY = shotCenterY - labelSize.y * 0.5f - 1.0f;

		if (shotMax.x - shotMin.x > labelSize.x + 12.0f)
		{
			drawList->AddText(ImVec2(shotMin.x + 8.0f, labelPosY), IM_COL32(255, 255, 255, 245), shotLabel.c_str());
		}
		drawList->PopClipRect();

		// 하단 구분선
		drawList->AddLine(ImVec2(headerMin.x, trackMax.y), trackMax, IM_COL32(50, 54, 62, 255));

		// 6. 샷 블록 클릭 시 잡은 지점(Time Offset) 기록
		if (isShotHovered && isMouseClicked)
		{
			m_Selection = { ETimelineSelectionType::CameraShot, camIdx, 0 };
			m_IsDraggingItem = true;
			const float clickTime = ScreenXToTime(mousePos.x, trackMin.x, timelineWidth);
			m_DragTimeOffset = clickTime - camTrack.StartTime;
		}

		// 7. 드래그 시 잡은 오프셋을 유지하며 즉각 이동
		if (m_IsDraggingItem && isSelected && isMouseDown)
		{
			const float curMouseTime = ScreenXToTime(mousePos.x, trackMin.x, timelineWidth);
			camTrack.StartTime = std::clamp(curMouseTime - m_DragTimeOffset, 0.0f, std::max(0.0f, m_Duration - camTrack.Duration));
			m_IsDirty = true;
		}

		// 8. 트랙 빈 공간 클릭 시 선택 해제
		if (isTrackHovered && !isShotHovered && isMouseClicked)
		{
			m_IsDraggingItem = false;
			m_DragTimeOffset = 0.0f;
			m_Selection = {};
		}
	}

	void AnimationTimelinePanel::DrawRuler(ImDrawList* drawList, const ImVec2& rulerMin, const ImVec2& rulerMax, float totalFrames)
	{
		const float rulerWidth = rulerMax.x - rulerMin.x;
		drawList->AddRectFilled(rulerMin, rulerMax, IM_COL32(28, 30, 35, 255));
		drawList->AddLine(ImVec2(rulerMin.x, rulerMax.y), rulerMax, IM_COL32(55, 60, 70, 255));

		const float visibleDuration = (m_Duration > 0.0f) ? (m_Duration / m_TimelineZoom) : 1.0f;
		const int startFrame = static_cast<int>(std::floor(m_ViewStartTime * m_FPS));
		const int endFrame = static_cast<int>(std::ceil((m_ViewStartTime + visibleDuration) * m_FPS));
		const int visibleFrames = std::max(1, endFrame - startFrame);

		const float pxPerFrame = rulerWidth / static_cast<float>(visibleFrames);

		int step = 1;
		if (pxPerFrame < 3.0f) step = 5;
		if (pxPerFrame < 1.5f) step = 10;
		if (pxPerFrame < 0.5f) step = 30;
		if (pxPerFrame < 0.15f) step = 60;

		const int firstTick = (startFrame / step) * step;
		for (int f = firstTick; f <= endFrame; f += step)
		{
			if (f < 0 || f > static_cast<int>(totalFrames)) continue;

			const float time = static_cast<float>(f) / m_FPS;
			const float x = TimeToScreenX(time, rulerMin.x, rulerWidth);
			if (x < rulerMin.x || x > rulerMax.x) continue;

			const bool isMajor = (f % (step * 5) == 0);
			const float tickHeight = isMajor ? 12.0f : 6.0f;

			drawList->AddLine(ImVec2(x, rulerMax.y - tickHeight), ImVec2(x, rulerMax.y),
				isMajor ? IM_COL32(180, 190, 205, 255) : IM_COL32(90, 95, 105, 200));

			if (isMajor && x + 25.0f < rulerMax.x)
			{
				char frameBuf[16];
				snprintf(frameBuf, sizeof(frameBuf), "%d", f);
				drawList->AddText(ImVec2(x + 3.0f, rulerMin.y + 2.0f), IM_COL32(160, 170, 185, 220), frameBuf);
			}
		}
	}

	void AnimationTimelinePanel::DrawPlayhead(ImDrawList* drawList, const ImVec2& trackMin, const ImVec2& trackMax, float curX)
	{
		if (curX < trackMin.x || curX > trackMax.x) return;

		const ImU32 playheadColor = IM_COL32(255, 75, 45, 255);
		drawList->AddLine(ImVec2(curX, trackMin.y), ImVec2(curX, trackMax.y), playheadColor, 1.8f);

		const float headHalfW = 6.0f;
		const float headH = 10.0f;
		drawList->AddTriangleFilled(
			ImVec2(curX - headHalfW, trackMin.y),
			ImVec2(curX + headHalfW, trackMin.y),
			ImVec2(curX, trackMin.y + headH),
			playheadColor
		);
	}

	void AnimationTimelinePanel::CaptureState(EditorWorkspace& workspace) const
	{
		IImGui::CaptureState(workspace);
		workspace.SetSplitter("Timeline.InspectorWidth", m_InspectorWidth);
		workspace.SetSplitter("Timeline.TimelineHeight", m_TimelineHeight);
		workspace.SetSplitter("Timeline.TrackHeaderWidth", m_TrackHeaderWidth);
	}

	void AnimationTimelinePanel::RestoreState(const EditorWorkspace& workspace)
	{
		IImGui::RestoreState(workspace);
		m_InspectorWidth = workspace.GetSplitter("Timeline.InspectorWidth", 280.0f);
		m_TimelineHeight = workspace.GetSplitter("Timeline.TimelineHeight", 220.0f);
		m_TrackHeaderWidth = workspace.GetSplitter("Timeline.TrackHeaderWidth", 180.0f);
	}
}