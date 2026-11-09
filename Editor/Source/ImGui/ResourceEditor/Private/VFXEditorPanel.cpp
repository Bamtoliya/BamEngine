#include "VFXEditorPanel.h"
#include "ResourcePicker.h"
#include "IconsFontAwesome7.h"
#include "LocalizationManager.h"
#include "InputManager.h"
#include "FileDialogs.h"
#include "Functions.h"
#include "PlatformPath.h"
#include "Scene.h"
#include "WorldComponents.h"
#include "CompositeVFXComponent.h"
#include "LightComponent.h"
#include "CompositeVFXSystem.h"
#include "ResourceManager.h"
#include "SpriteAnimation.h"
#include "Sprite.h"
#include "ParticleEffect.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "AudioClip.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace Editor
{
	VFXEditorPanel::VFXEditorPanel()
	{
		m_Name = L"Composite VFX Editor";
		m_DisplayNameKey = "VFXEditor.Title";
		m_PreviewWidget.Initialize(L"VFXEditorPreview", 512, 512, true);
		m_PreviewWidget.SetupDefaultStudioLighting();
		m_PreviewWidget.FocusTarget(Engine::vec3(0.0f, 0.5f, 0.0f), 3.5f);

		for (auto* effect : m_PreviewWidget.GetPostProcessChain().GetPipeline().GetEffects())
		{
			if (effect && effect->GetName() == L"Bloom")
			{
				effect->SetEnabled(true);
			}
		}
	}

	VFXEditorPanel::~VFXEditorPanel()
	{
		m_PreviewWidget.Free();
	}

	bool VFXEditorPanel::IsSupported(const std::filesystem::path& assetPath) const
	{
		const std::string ext = assetPath.extension().string();
		const std::string name = WStrToStr(assetPath.filename().wstring());
		return (ext == ".bamvfx" || ext == ".vfx" || name.ends_with(".bamvfx.json") || name.ends_with(".vfx.json"));
	}

	void VFXEditorPanel::SetTargetResource(const std::filesystem::path& assetPath)
	{
		ResourceEditorInterface::SetTargetResource(assetPath);
		m_IsDirty = false;
		m_SelectedTrack.Clear();

		const std::wstring relPath = Engine::PlatformPath::ToProjectRelative(assetPath).wstring();
		auto& rm = Engine::ResourceManager::Get();

		m_EffectHandle = rm.GetResourceHandle<Engine::CompositeEffect>(relPath);
		if (!m_EffectHandle.IsValid())
		{
			auto handle = rm.LoadFile(relPath);
			if (handle.IsValid())
				m_EffectHandle = Engine::ResourceHandle<Engine::CompositeEffect>(handle);
		}

		if (!m_EffectHandle.IsValid() && std::filesystem::exists(assetPath))
		{
			auto* directEffect = Engine::CompositeEffect::Create();
			if (directEffect)
			{
				if (directEffect->LoadFromFile(assetPath.wstring()))
				{
					m_EffectHandle = rm.AddResource<Engine::CompositeEffect>(relPath, directEffect);
				}
				else
				{
					rm.DestroyResource(directEffect);
				}
			}
		}

		if (!m_EffectHandle.IsValid())
		{
			NewEffect();
			m_TargetResourcePath = assetPath;
		}
		else
		{
			SetupPreview();
		}
	}

	void VFXEditorPanel::CaptureState(EditorWorkspace& workspace) const
	{
		IImGui::CaptureState(workspace);
		workspace.SetSplitter("VFXEditor.LeftPaneWidth", m_LeftPaneWidth);
		workspace.SetSplitter("VFXEditor.RightPaneWidth", m_RightPaneWidth);
		workspace.SetSplitter("VFXEditor.TimelineHeight", m_TimelineHeight);
		workspace.SetSplitter("VFXEditor.TimelineHeaderWidth", m_TimelineHeaderWidth);
	}

	void VFXEditorPanel::RestoreState(const EditorWorkspace& workspace)
	{
		IImGui::RestoreState(workspace);
		m_LeftPaneWidth = workspace.GetSplitter("VFXEditor.LeftPaneWidth", m_LeftPaneWidth);
		m_RightPaneWidth = workspace.GetSplitter("VFXEditor.RightPaneWidth", m_RightPaneWidth);
		m_TimelineHeight = workspace.GetSplitter("VFXEditor.TimelineHeight", m_TimelineHeight);
		m_TimelineHeaderWidth = workspace.GetSplitter("VFXEditor.TimelineHeaderWidth", m_TimelineHeaderWidth);
	}

	void VFXEditorPanel::SetupPreview()
	{
		auto* scene = m_PreviewWidget.GetScene();
		if (!scene) return;

		if (m_PreviewEntity)
		{
			(void)scene->DestroyEntity(*m_PreviewEntity);
			m_PreviewEntity = nullptr;
		}

		auto* effect = m_EffectHandle.Get();
		if (!effect) return;

		m_PreviewEntity = &m_PreviewWidget.CreateEntity(L"CompositeVFXPreview");
		(void)scene->AddComponent<Engine::FlagComponent>(*m_PreviewEntity);
		auto& comp = scene->AddComponent<Engine::CompositeVFXComponent>(*m_PreviewEntity);
		comp.effectHandle = m_EffectHandle;
		comp.isPlaying = m_IsPlaying;
		comp.isLooping = m_IsLooping;
		comp.playRate = m_PlayRate;
		comp.Play();

		m_CurrentTime = 0.0f;
	}

	void VFXEditorPanel::RestartSimulation()
	{
		auto* scene = m_PreviewWidget.GetScene();
		if (m_PreviewEntity && scene)
		{
			auto& comp = scene->GetComponent<Engine::CompositeVFXComponent>(*m_PreviewEntity);
			comp.Restart();
		}
		m_CurrentTime = 0.0f;
	}

	void VFXEditorPanel::ScrubTime(f32 targetTime)
	{
		m_CurrentTime = targetTime;
		auto* scene = m_PreviewWidget.GetScene();
		if (m_PreviewEntity && scene)
		{
			auto& comp = scene->GetComponent<Engine::CompositeVFXComponent>(*m_PreviewEntity);
			comp.currentTime = targetTime;
			comp.prevTime = targetTime;
			Engine::CompositeVFXSystem::SimulateSceneVFX(scene, 0.0f);
		}
	}

	void VFXEditorPanel::Update(f32 dt)
	{
		auto* scene = m_PreviewWidget.GetScene();
		if (m_PreviewEntity && scene && m_EffectHandle.IsValid())
		{
			auto& comp = scene->GetComponent<Engine::CompositeVFXComponent>(*m_PreviewEntity);
			comp.isPlaying = m_IsPlaying;
			comp.isLooping = m_IsLooping;
			comp.playRate = m_PlayRate;

			if (m_IsPlaying)
			{
				Engine::CompositeVFXSystem::SimulateSceneVFX(scene, dt * m_PlayRate);
				m_CurrentTime = comp.currentTime;
			}
		}

		m_PreviewWidget.BeginFrame(dt);
		m_PreviewWidget.Submit(dt);
	}

	void VFXEditorPanel::NewEffect()
	{
		Engine::CompositeEffectCreateDesc desc;
		desc.totalDuration = 2.0f;
		desc.isLooping = false;
		auto* newEffect = Engine::CompositeEffect::Create(&desc);
		if (!newEffect) return;

		std::wstring tempKey = L"NewEffect_" + std::to_wstring(Engine::TimeManager::Get().GetTotalTime());
		m_EffectHandle = Engine::ResourceManager::Get().AddResource<Engine::CompositeEffect>(tempKey, newEffect);
		m_TargetResourcePath.clear();
		m_SelectedTrack.Clear();
		m_IsDirty = true;
		SetupPreview();
	}

	void VFXEditorPanel::OpenEffect()
	{
		std::wstring openPath;
		if (FileDialogs::OpenFileDialog(openPath, { { L"Visual Effect (*.bamvfx.json, *.bamvfx)", L"*.bamvfx.json;*.bamvfx;*.vfx" } }, L"", EFileDialogContext::General))
		{
			SetTargetResource(std::filesystem::path(openPath));
		}
	}

	void VFXEditorPanel::SaveEffect()
	{
		if (m_TargetResourcePath.empty())
		{
			SaveEffectAs();
			return;
		}

		auto* effect = m_EffectHandle.Get();
		if (effect && effect->SaveToFile(m_TargetResourcePath.wstring()))
		{
			m_IsDirty = false;
			m_StatusMessage = "Effect saved successfully.";
		}
	}

	void VFXEditorPanel::SaveEffectAs()
	{
		std::wstring savePath;
		if (FileDialogs::SaveFileDialog(savePath, { { L"Visual Effect (*.bamvfx.json)", L"*.bamvfx.json" } }, L"NewEffect.bamvfx.json", L"bamvfx.json", EFileDialogContext::General))
		{
			m_TargetResourcePath = std::filesystem::path(savePath);
			SaveEffect();
		}
	}

	void VFXEditorPanel::Draw()
	{
		if (!m_Open) return;

		ImGui::SetNextWindowSize(ImVec2(1280.0f, 760.0f), ImGuiCond_FirstUseEver);
		const std::string windowTitle = BuildResourceWindowTitle();
		if (!ImGui::Begin(windowTitle.c_str(), &m_Open, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			ImGui::End();
			return;
		}

		DrawToolbar();

		const ImVec2 contentSize = ImGui::GetContentRegionAvail();
		const float spacingX = ImGui::GetStyle().ItemSpacing.x;
		const float splitterWidth = 4.0f;
		const float totalSeparation = splitterWidth * 2.0f + spacingX * 4.0f;
		const float minPane = 160.0f;

		if (m_LeftPaneWidth < minPane) m_LeftPaneWidth = minPane;
		if (m_RightPaneWidth < minPane) m_RightPaneWidth = minPane;

		float centerWidth = contentSize.x - m_LeftPaneWidth - m_RightPaneWidth - totalSeparation;
		if (centerWidth < minPane)
		{
			centerWidth = minPane;
			m_LeftPaneWidth = (contentSize.x - centerWidth - totalSeparation) * 0.4f;
			m_RightPaneWidth = contentSize.x - centerWidth - m_LeftPaneWidth - totalSeparation;
		}

		// 좌측: 트랙 목록 및 계층 뷰
		ImGui::BeginChild("VFXLeftPane", ImVec2(m_LeftPaneWidth, contentSize.y), true);
		DrawTrackList();
		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::Button("##VFXSplitter1", ImVec2(splitterWidth, contentSize.y));
		if (ImGui::IsItemActive())
			m_LeftPaneWidth += Engine::InputManager::Get().GetMouseDelta().x;

		// 중앙: 상단 뷰포트 + 하단 타임라인
		ImGui::SameLine();
		ImGui::BeginChild("VFXCenterPane", ImVec2(centerWidth, contentSize.y), false);
		{
			const float centerAvailY = ImGui::GetContentRegionAvail().y;
			const float minTimelineH = 100.0f;
			const float minViewportH = 150.0f;
			if (m_TimelineHeight < minTimelineH) m_TimelineHeight = minTimelineH;
			if (m_TimelineHeight > centerAvailY - minViewportH) m_TimelineHeight = centerAvailY - minViewportH;

			const float viewportHeight = centerAvailY - m_TimelineHeight - splitterWidth - ImGui::GetStyle().ItemSpacing.y * 2.0f;

			// 뷰포트
			ImGui::BeginChild("VFXViewportPane", ImVec2(0.0f, viewportHeight), true);
			DrawViewport();
			ImGui::EndChild();

			// 세로 스플리터
			ImGui::Button("##VFXSplitterV", ImVec2(centerWidth, splitterWidth));
			if (ImGui::IsItemActive())
				m_TimelineHeight -= Engine::InputManager::Get().GetMouseDelta().y;

			// 타임라인
			ImGui::BeginChild("VFXTimelinePane", ImVec2(0.0f, m_TimelineHeight), true);
			DrawTimeline(ImGui::GetContentRegionAvail().x, m_TimelineHeight);
			ImGui::EndChild();
		}
		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::Button("##VFXSplitter2", ImVec2(splitterWidth, contentSize.y));
		if (ImGui::IsItemActive())
			m_RightPaneWidth -= Engine::InputManager::Get().GetMouseDelta().x;

		// 우측: 속성 인스펙터
		ImGui::SameLine();
		ImGui::BeginChild("VFXRightPane", ImVec2(0.0f, contentSize.y), true);
		DrawInspector();
		ImGui::EndChild();

		ImGui::End();
	}

	void VFXEditorPanel::DrawToolbar()
	{
		const bool hasResource = m_EffectHandle.IsValid() && (m_EffectHandle.Get() != nullptr);
		auto* effect = m_EffectHandle.Get();

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.File")))
			{
				if (ImGui::MenuItem(LOCAL_CSTR("ParticleEditor.MenuNew"), "Ctrl+N")) NewEffect();
				if (ImGui::MenuItem(LOCAL_CSTR("ParticleEditor.MenuOpen"), "Ctrl+O")) OpenEffect();
				if (ImGui::MenuItem(LOCAL_CSTR("EditorCommon.Save"), "Ctrl+S", false, hasResource)) SaveEffect();
				if (ImGui::MenuItem(LOCAL_CSTR("EditorCommon.SaveAs"), "Ctrl+Shift+S", false, hasResource)) SaveEffectAs();
				ImGui::Separator();
				if (ImGui::MenuItem(LOCAL_CSTR("EditorCommon.Close"), "Ctrl+W")) m_Open = false;
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		// 재생 제어 툴바
		ImGui::BeginDisabled(!hasResource);

		const std::string playBtnLabel = m_IsPlaying
			? (std::string(ICON_FA_PAUSE " ") + LOCAL_CSTR("VFXEditor.Pause"))
			: (std::string(ICON_FA_PLAY " ") + LOCAL_CSTR("VFXEditor.Play"));
		if (ImGui::Button(playBtnLabel.c_str()))
		{
			m_IsPlaying = !m_IsPlaying;
		}
		ImGui::SameLine();
		const std::string restartLabel = std::string(ICON_FA_ROTATE_LEFT " ") + LOCAL_CSTR("VFXEditor.Restart");
		if (ImGui::Button(restartLabel.c_str()))
		{
			RestartSimulation();
		}
		ImGui::SameLine();
		const std::string loopLabel = std::string(ICON_FA_REPEAT " ") + LOCAL_CSTR("VFXEditor.PropLoop");
		if (ImGui::Checkbox(loopLabel.c_str(), &m_IsLooping))
		{
			if (effect) effect->SetLooping(m_IsLooping);
			m_IsDirty = true;
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(75.0f);
		if (ImGui::DragFloat(LOCAL_CSTR("VFXEditor.Speed"), &m_PlayRate, 0.05f, 0.1f, 3.0f, "%.1fx"))
		{
			m_PlayRate = std::clamp(m_PlayRate, 0.1f, 5.0f);
		}

		ImGui::SameLine();
		if (effect)
		{
			float totalDur = effect->GetTotalDuration();
			ImGui::SetNextItemWidth(75.0f);
			if (ImGui::DragFloat(LOCAL_CSTR("VFXEditor.DurationTotal"), &totalDur, 0.05f, 0.1f, 30.0f, "%.2fs"))
			{
				effect->SetTotalDuration(totalDur);
				m_IsDirty = true;
			}
			ImGui::SameLine();
			if (ImGui::Button(LOCAL_CSTR("VFXEditor.AutoDuration")))
			{
				effect->RecalculateDurationFromTracks();
				m_IsDirty = true;
			}

			ImGui::SameLine();
			ImGui::TextDisabled("| Time: %.2fs / %.2fs", m_CurrentTime, effect->GetTotalDuration());
		}

		ImGui::EndDisabled();
		ImGui::Separator();
	}

	void VFXEditorPanel::DrawTrackList()
	{
		auto* effect = m_EffectHandle.Get();
		if (!effect)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOCAL_CSTR("VFXEditor.NoEffectLoaded"));
			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_FILE " ") + LOCAL_CSTR("VFXEditor.CreateNewEffect")).c_str()))
			{
				NewEffect();
			}
			ImGui::SameLine();
			if (ImGui::Button((std::string(ICON_FA_FOLDER_OPEN " ") + LOCAL_CSTR("ParticleEditor.MenuOpen")).c_str()))
			{
				OpenEffect();
			}
			return;
		}

		// 상단 타이틀 및 [+ 트랙 추가] 버튼 (가로 스크롤 방지를 위해 창 폭 정확히 클램프)
		const float availWidth = ImGui::GetContentRegionAvail().x;
		const float btnWidth = 100.0f;
		ImGui::Text(ICON_FA_LAYER_GROUP " %s (%u)", LOCAL_CSTR("VFXEditor.Tracks"), effect->GetTotalTrackCount());
		if (availWidth > btnWidth + 60.0f)
		{
			ImGui::SameLine();
			ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - btnWidth - ImGui::GetStyle().ItemSpacing.x));
		}
		if (ImGui::Button((std::string(ICON_FA_PLUS " ") + LOCAL_CSTR("VFXEditor.AddTrack")).c_str(), ImVec2(btnWidth, 0.0f)))
		{
			ImGui::OpenPopup("AddVFXTrackPopup");
		}

		if (ImGui::BeginPopup("AddVFXTrackPopup"))
		{
			if (ImGui::MenuItem((std::string(ICON_FA_IMAGES " ") + LOCAL_CSTR("VFXEditor.AddSpriteTrack")).c_str()))
			{
				Engine::VFXSpriteTrack track;
				track.trackName = "Sprite_" + std::to_string(effect->GetSpriteTracks().size() + 1);
				effect->GetSpriteTracks().push_back(track);
				m_SelectedTrack = { ESelectedVFXTrackType::Sprite, static_cast<int>(effect->GetSpriteTracks().size() - 1) };
				m_IsDirty = true;
			}
			if (ImGui::MenuItem((std::string(ICON_FA_WAND_MAGIC_SPARKLES " ") + LOCAL_CSTR("VFXEditor.AddParticleTrack")).c_str()))
			{
				Engine::VFXParticleTrack track;
				track.trackName = "Particle_" + std::to_string(effect->GetParticleTracks().size() + 1);
				effect->GetParticleTracks().push_back(track);
				m_SelectedTrack = { ESelectedVFXTrackType::Particle, static_cast<int>(effect->GetParticleTracks().size() - 1) };
				m_IsDirty = true;
			}
			if (ImGui::MenuItem((std::string(ICON_FA_LIGHTBULB " ") + LOCAL_CSTR("VFXEditor.AddLightTrack")).c_str()))
			{
				Engine::VFXLightTrack track;
				track.trackName = "Flash_" + std::to_string(effect->GetLightTracks().size() + 1);
				effect->GetLightTracks().push_back(track);
				m_SelectedTrack = { ESelectedVFXTrackType::Light, static_cast<int>(effect->GetLightTracks().size() - 1) };
				m_IsDirty = true;
			}
			if (ImGui::MenuItem((std::string(ICON_FA_CUBE " ") + LOCAL_CSTR("VFXEditor.AddMeshTrack")).c_str()))
			{
				Engine::VFXMeshTrack track;
				track.trackName = "Mesh_" + std::to_string(effect->GetMeshTracks().size() + 1);
				effect->GetMeshTracks().push_back(track);
				m_SelectedTrack = { ESelectedVFXTrackType::Mesh, static_cast<int>(effect->GetMeshTracks().size() - 1) };
				m_IsDirty = true;
			}
			if (ImGui::MenuItem((std::string(ICON_FA_MUSIC " ") + LOCAL_CSTR("VFXEditor.AddSoundTrack")).c_str()))
			{
				Engine::VFXSoundTrack track;
				track.trackName = "Audio_" + std::to_string(effect->GetSoundTracks().size() + 1);
				effect->GetSoundTracks().push_back(track);
				m_SelectedTrack = { ESelectedVFXTrackType::Sound, static_cast<int>(effect->GetSoundTracks().size() - 1) };
				m_IsDirty = true;
			}
			ImGui::EndPopup();
		}

		ImGui::Separator();

		if (effect->GetTotalTrackCount() == 0)
		{
			ImGui::Spacing();
			ImGui::TextDisabled("%s", LOCAL_CSTR("VFXEditor.NoTracks"));
			ImGui::Spacing();
			ImGui::TextDisabled("%s", LOCAL_CSTR("VFXEditor.AddPrompt"));
			ImGui::BulletText("%s", LOCAL_CSTR("VFXEditor.AddSpriteTrack"));
			ImGui::BulletText("%s", LOCAL_CSTR("VFXEditor.AddParticleTrack"));
			ImGui::BulletText("%s", LOCAL_CSTR("VFXEditor.AddLightTrack"));
			ImGui::BulletText("%s", LOCAL_CSTR("VFXEditor.AddMeshTrack"));
			ImGui::BulletText("%s", LOCAL_CSTR("VFXEditor.AddSoundTrack"));
			return;
		}

		// 트랙 리스트 렌더링 (가로 폭 0.0f로 맞추어 스크롤바 방지)
		int trackId = 0;

		// 1. Sprite Tracks
		auto& spriteTracks = effect->GetSpriteTracks();
		for (size_t i = 0; i < spriteTracks.size(); ++i)
		{
			auto& track = spriteTracks[i];
			ImGui::PushID(trackId++);

			bool isSelected = (m_SelectedTrack.type == ESelectedVFXTrackType::Sprite && m_SelectedTrack.index == static_cast<int>(i));
			ImGui::Checkbox("##En", &track.enabled);
			ImGui::SameLine();

			std::string label = ICON_FA_IMAGES " " + track.trackName;
			if (ImGui::Selectable(label.c_str(), isSelected, 0, ImVec2(0.0f, 0.0f)))
			{
				m_SelectedTrack = { ESelectedVFXTrackType::Sprite, static_cast<int>(i) };
			}

			ImGui::PopID();
		}

		// 2. Particle Tracks
		auto& particleTracks = effect->GetParticleTracks();
		for (size_t i = 0; i < particleTracks.size(); ++i)
		{
			auto& track = particleTracks[i];
			ImGui::PushID(trackId++);

			bool isSelected = (m_SelectedTrack.type == ESelectedVFXTrackType::Particle && m_SelectedTrack.index == static_cast<int>(i));
			ImGui::Checkbox("##En", &track.enabled);
			ImGui::SameLine();

			std::string label = ICON_FA_WAND_MAGIC_SPARKLES " " + track.trackName;
			if (ImGui::Selectable(label.c_str(), isSelected, 0, ImVec2(0.0f, 0.0f)))
			{
				m_SelectedTrack = { ESelectedVFXTrackType::Particle, static_cast<int>(i) };
			}

			ImGui::PopID();
		}

		// 3. Light Flash Tracks
		auto& lightTracks = effect->GetLightTracks();
		for (size_t i = 0; i < lightTracks.size(); ++i)
		{
			auto& track = lightTracks[i];
			ImGui::PushID(trackId++);

			bool isSelected = (m_SelectedTrack.type == ESelectedVFXTrackType::Light && m_SelectedTrack.index == static_cast<int>(i));
			ImGui::Checkbox("##En", &track.enabled);
			ImGui::SameLine();

			std::string label = ICON_FA_LIGHTBULB " " + track.trackName;
			if (ImGui::Selectable(label.c_str(), isSelected, 0, ImVec2(0.0f, 0.0f)))
			{
				m_SelectedTrack = { ESelectedVFXTrackType::Light, static_cast<int>(i) };
			}

			ImGui::PopID();
		}

		// 4. Mesh Tracks
		auto& meshTracks = effect->GetMeshTracks();
		for (size_t i = 0; i < meshTracks.size(); ++i)
		{
			auto& track = meshTracks[i];
			ImGui::PushID(trackId++);

			bool isSelected = (m_SelectedTrack.type == ESelectedVFXTrackType::Mesh && m_SelectedTrack.index == static_cast<int>(i));
			ImGui::Checkbox("##En", &track.enabled);
			ImGui::SameLine();

			std::string label = ICON_FA_CUBE " " + track.trackName;
			if (ImGui::Selectable(label.c_str(), isSelected, 0, ImVec2(0.0f, 0.0f)))
			{
				m_SelectedTrack = { ESelectedVFXTrackType::Mesh, static_cast<int>(i) };
			}

			ImGui::PopID();
		}

		// 5. Sound Tracks
		auto& soundTracks = effect->GetSoundTracks();
		for (size_t i = 0; i < soundTracks.size(); ++i)
		{
			auto& track = soundTracks[i];
			ImGui::PushID(trackId++);

			bool isSelected = (m_SelectedTrack.type == ESelectedVFXTrackType::Sound && m_SelectedTrack.index == static_cast<int>(i));
			ImGui::Checkbox("##En", &track.enabled);
			ImGui::SameLine();

			std::string label = ICON_FA_MUSIC " " + track.trackName;
			if (ImGui::Selectable(label.c_str(), isSelected, 0, ImVec2(0.0f, 0.0f)))
			{
				m_SelectedTrack = { ESelectedVFXTrackType::Sound, static_cast<int>(i) };
			}

			ImGui::PopID();
		}
	}

	void VFXEditorPanel::DrawViewport()
	{
		m_PreviewWidget.Draw();
	}

	void VFXEditorPanel::DrawTimeline(float availWidth, float height)
	{
		auto* effect = m_EffectHandle.Get();
		if (!effect)
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("VFXEditor.TimelineUnavailable"));
			return;
		}

		const float totalDur = std::max(effect->GetTotalDuration(), 0.1f);
		const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
		const float rulerHeight = 24.0f;
		const float trackHeight = 22.0f;
		const float trackSpacing = 4.0f;

		// 사용자가 마우스로 폭을 조절할 수 있는 트랙 헤더 영역 (기본 200px)
		const float splitterWidth = 4.0f;
		const float maxHeaderWidth = std::max(availWidth - 120.0f, 120.0f);
		m_TimelineHeaderWidth = std::clamp(m_TimelineHeaderWidth, 120.0f, maxHeaderWidth);
		const float headerWidth = m_TimelineHeaderWidth;

		const float timelineStartX = canvasPos.x + headerWidth + splitterWidth;
		const float timelineWidth = std::max(availWidth - headerWidth - splitterWidth - 15.0f, 80.0f);

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// 좌측 트랙 헤더 배경 및 수직 구분선
		const ImVec2 headerBgMin = canvasPos;
		const ImVec2 headerBgMax = ImVec2(canvasPos.x + headerWidth, canvasPos.y + height);
		drawList->AddRectFilled(headerBgMin, headerBgMax, IM_COL32(23, 25, 30, 255));
		drawList->AddLine(ImVec2(headerBgMax.x, headerBgMin.y), headerBgMax, IM_COL32(50, 55, 68, 255), 1.0f);

		// 헤더 너비 조절용 수평 스플리터 버튼
		ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + headerWidth - 2.0f, canvasPos.y));
		ImGui::InvisibleButton("##TimelineHeaderSplitter", ImVec2(splitterWidth + 4.0f, height));
		if (ImGui::IsItemActive())
		{
			m_TimelineHeaderWidth += Engine::InputManager::Get().GetMouseDelta().x;
			m_TimelineHeaderWidth = std::clamp(m_TimelineHeaderWidth, 120.0f, 450.0f);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}

		// 1. 눈금자 (Ruler) 그리기
		const ImVec2 rulerMin = ImVec2(timelineStartX, canvasPos.y);
		const ImVec2 rulerMax = ImVec2(timelineStartX + timelineWidth, rulerMin.y + rulerHeight);
		drawList->AddRectFilled(rulerMin, rulerMax, IM_COL32(28, 30, 36, 255));
		drawList->AddLine(ImVec2(rulerMin.x, rulerMax.y), rulerMax, IM_COL32(60, 65, 75, 255));

		// 초 단위 눈금
		const int numSeconds = static_cast<int>(std::ceil(totalDur));
		for (int s = 0; s <= numSeconds; ++s)
		{
			const float ratio = static_cast<float>(s) / totalDur;
			if (ratio > 1.0f) break;
			const float x = rulerMin.x + ratio * timelineWidth;
			drawList->AddLine(ImVec2(x, rulerMax.y - 12.0f), ImVec2(x, rulerMax.y), IM_COL32(180, 190, 205, 255));

			char buf[16];
			snprintf(buf, sizeof(buf), "%ds", s);
			drawList->AddText(ImVec2(x + 2.0f, rulerMin.y + 2.0f), IM_COL32(160, 170, 185, 220), buf);
		}

		// 2. 눈금자 클릭/드래그 스크러빙
		ImGui::SetCursorScreenPos(rulerMin);
		ImGui::InvisibleButton("##RulerBtn", ImVec2(timelineWidth, rulerHeight));
		if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			const float mouseX = ImGui::GetIO().MousePos.x;
			const float normalizedT = std::clamp((mouseX - rulerMin.x) / timelineWidth, 0.0f, 1.0f);
			ScrubTime(normalizedT * totalDur);
		}

		// 3. 트랙 바 그리기
		float currentY = rulerMax.y + 4.0f;

		auto DrawTrackBar = [&](const std::string& name, float startTime, float duration, ImU32 color, bool isSelected, auto onDrag)
		{
			// 트랙 라벨 텍스트 (클리핑 적용하여 트랙 바 침범 원천 차단)
			const ImVec2 textClipMin = ImVec2(canvasPos.x + 6.0f, currentY);
			const ImVec2 textClipMax = ImVec2(canvasPos.x + headerWidth - 4.0f, currentY + trackHeight);
			drawList->PushClipRect(textClipMin, textClipMax, true);
			drawList->AddText(ImVec2(textClipMin.x, currentY + 3.0f), isSelected ? IM_COL32(255, 220, 100, 255) : IM_COL32(210, 215, 225, 255), name.c_str());
			drawList->PopClipRect();

			// 트랙 라벨 호버 시 전체 이름 툴팁
			ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, currentY));
			ImGui::InvisibleButton(("##TrackLabel_" + name).c_str(), ImVec2(headerWidth, trackHeight));
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", name.c_str());
			}

			// 트랙 배경 라인
			const ImVec2 lineMin = ImVec2(timelineStartX, currentY);
			const ImVec2 lineMax = ImVec2(timelineStartX + timelineWidth, currentY + trackHeight);
			drawList->AddRectFilled(lineMin, lineMax, IM_COL32(35, 38, 45, 180));

			// 활성 구간 바
			const float startX = timelineStartX + std::clamp(startTime / totalDur, 0.0f, 1.0f) * timelineWidth;
			const float endX = timelineStartX + std::clamp((startTime + duration) / totalDur, 0.0f, 1.0f) * timelineWidth;
			const float barWidth = std::max(endX - startX, 4.0f);

			const ImVec2 barMin = ImVec2(startX, currentY + 2.0f);
			const ImVec2 barMax = ImVec2(startX + barWidth, currentY + trackHeight - 2.0f);
			drawList->AddRectFilled(barMin, barMax, color, 3.0f);

			if (isSelected)
			{
				drawList->AddRect(barMin, barMax, IM_COL32(255, 255, 255, 255), 3.0f, 0, 1.5f);
			}

			// 인터랙션: 바 드래그 이동
			std::string btnId = "##Bar_" + name;
			ImGui::SetCursorScreenPos(barMin);
			ImGui::InvisibleButton(btnId.c_str(), ImVec2(barWidth, trackHeight));
			if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			{
				const float deltaX = ImGui::GetIO().MouseDelta.x;
				const float deltaT = (deltaX / timelineWidth) * totalDur;
				onDrag(deltaT);
			}

			currentY += trackHeight + trackSpacing;
		};

		// 스프라이트 트랙
		auto& spriteTracks = effect->GetSpriteTracks();
		for (size_t i = 0; i < spriteTracks.size(); ++i)
		{
			auto& t = spriteTracks[i];
			bool isSel = (m_SelectedTrack.type == ESelectedVFXTrackType::Sprite && m_SelectedTrack.index == static_cast<int>(i));
			DrawTrackBar(t.trackName, t.startTime, t.duration, IM_COL32(40, 180, 160, 220), isSel, [&](float dt) {
				t.startTime = std::max(0.0f, t.startTime + dt);
				m_IsDirty = true;
			});
		}

		// 파티클 트랙
		auto& particleTracks = effect->GetParticleTracks();
		for (size_t i = 0; i < particleTracks.size(); ++i)
		{
			auto& t = particleTracks[i];
			bool isSel = (m_SelectedTrack.type == ESelectedVFXTrackType::Particle && m_SelectedTrack.index == static_cast<int>(i));
			DrawTrackBar(t.trackName, t.startTime, t.duration, IM_COL32(230, 130, 40, 220), isSel, [&](float dt) {
				t.startTime = std::max(0.0f, t.startTime + dt);
				m_IsDirty = true;
			});
		}

		// 라이트 트랙
		auto& lightTracks = effect->GetLightTracks();
		for (size_t i = 0; i < lightTracks.size(); ++i)
		{
			auto& t = lightTracks[i];
			bool isSel = (m_SelectedTrack.type == ESelectedVFXTrackType::Light && m_SelectedTrack.index == static_cast<int>(i));
			DrawTrackBar(t.trackName, t.startTime, t.duration, IM_COL32(230, 200, 50, 220), isSel, [&](float dt) {
				t.startTime = std::max(0.0f, t.startTime + dt);
				m_IsDirty = true;
			});
		}

		// 메쉬 트랙
		auto& meshTracks = effect->GetMeshTracks();
		for (size_t i = 0; i < meshTracks.size(); ++i)
		{
			auto& t = meshTracks[i];
			bool isSel = (m_SelectedTrack.type == ESelectedVFXTrackType::Mesh && m_SelectedTrack.index == static_cast<int>(i));
			DrawTrackBar(t.trackName, t.startTime, t.duration, IM_COL32(60, 130, 230, 220), isSel, [&](float dt) {
				t.startTime = std::max(0.0f, t.startTime + dt);
				m_IsDirty = true;
			});
		}

		// 사운드 트랙
		auto& soundTracks = effect->GetSoundTracks();
		for (size_t i = 0; i < soundTracks.size(); ++i)
		{
			auto& t = soundTracks[i];
			bool isSel = (m_SelectedTrack.type == ESelectedVFXTrackType::Sound && m_SelectedTrack.index == static_cast<int>(i));
			DrawTrackBar(t.trackName, t.startTime, 0.4f, IM_COL32(180, 70, 220, 220), isSel, [&](float dt) {
				t.startTime = std::max(0.0f, t.startTime + dt);
				m_IsDirty = true;
			});
		}

		// 4. 플레이헤드 (Playhead) 빨간 선 그리기
		const float playheadX = rulerMin.x + std::clamp(m_CurrentTime / totalDur, 0.0f, 1.0f) * timelineWidth;
		const ImU32 playheadColor = IM_COL32(255, 65, 45, 255);
		const float bottomY = std::max(currentY, canvasPos.y + height - 10.0f);
		drawList->AddLine(ImVec2(playheadX, rulerMin.y), ImVec2(playheadX, bottomY), playheadColor, 2.0f);
		drawList->AddTriangleFilled(
			ImVec2(playheadX - 6.0f, rulerMin.y),
			ImVec2(playheadX + 6.0f, rulerMin.y),
			ImVec2(playheadX, rulerMin.y + 10.0f),
			playheadColor);
	}

	void VFXEditorPanel::DrawInspector()
	{
		auto* effect = m_EffectHandle.Get();
		if (!effect)
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("VFXEditor.NoEffectLoaded"));
			return;
		}

		if (!m_SelectedTrack.IsValid())
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("VFXEditor.SelectTrackPrompt"));
			return;
		}

		ImGui::Text(ICON_FA_SLIDERS " %s", LOCAL_CSTR("VFXEditor.TrackInspector"));
		ImGui::Separator();

		auto DrawPropertyRow = [](const char* label, auto drawWidget)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			drawWidget();
		};

		// 1. Sprite Track Inspector
		if (m_SelectedTrack.type == ESelectedVFXTrackType::Sprite)
		{
			auto& tracks = effect->GetSpriteTracks();
			if (m_SelectedTrack.index >= static_cast<int>(tracks.size())) return;
			auto& track = tracks[m_SelectedTrack.index];

			if (ImGui::BeginTable("SpriteTrackInspectorTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 105.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropTrackName"), [&]() {
					char nameBuf[128];
					strncpy_s(nameBuf, track.trackName.c_str(), sizeof(nameBuf) - 1);
					if (ImGui::InputText("##Name", nameBuf, sizeof(nameBuf))) { track.trackName = nameBuf; m_IsDirty = true; }
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropEnabled"), [&]() {
					if (ImGui::Checkbox("##Enabled", &track.enabled)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropMuted"), [&]() {
					if (ImGui::Checkbox("##Muted", &track.isMuted)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropStartTime"), [&]() {
					if (ImGui::DragFloat("##StartTime", &track.startTime, 0.02f, 0.0f, 30.0f, "%.2fs")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropDuration"), [&]() {
					if (ImGui::DragFloat("##Duration", &track.duration, 0.02f, 0.05f, 30.0f, "%.2fs")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropPlaySpeed"), [&]() {
					if (ImGui::DragFloat("##Speed", &track.playbackSpeed, 0.05f, 0.1f, 5.0f, "%.1fx")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropLoop"), [&]() {
					if (ImGui::Checkbox("##Loop", &track.loop)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropAnimation"), [&]() {
					if (Editor::DrawInspectorResourcePicker(track.animationHandle, true, true)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropMaterial"), [&]() {
					if (Editor::DrawInspectorResourcePicker(track.materialHandle, true, true)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropBillboard"), [&]() {
					const char* billboardModes[] = { "None", "CameraFacing", "AxialY" };
					int currentMode = static_cast<int>(track.billboardMode);
					if (ImGui::Combo("##Billboard", &currentMode, billboardModes, IM_ARRAYSIZE(billboardModes)))
					{
						track.billboardMode = static_cast<Engine::ESpriteBillboardMode>(currentMode);
						m_IsDirty = true;
					}
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropColorTint"), [&]() {
					if (ImGui::ColorEdit4("##ColorTint", &track.colorTint.x)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropPosition"), [&]() {
					if (ImGui::DragFloat3("##Position", &track.localPosition.x, 0.05f)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropRotation"), [&]() {
					if (ImGui::DragFloat3("##Rotation", &track.localRotation.x, 1.0f)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropScale"), [&]() {
					if (ImGui::DragFloat3("##Scale", &track.localScale.x, 0.05f)) m_IsDirty = true;
				});

				ImGui::EndTable();
			}

			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
			{
				tracks.erase(tracks.begin() + m_SelectedTrack.index);
				m_SelectedTrack.Clear();
				m_IsDirty = true;
			}
		}
		// 2. Particle Track Inspector
		else if (m_SelectedTrack.type == ESelectedVFXTrackType::Particle)
		{
			auto& tracks = effect->GetParticleTracks();
			if (m_SelectedTrack.index >= static_cast<int>(tracks.size())) return;
			auto& track = tracks[m_SelectedTrack.index];

			if (ImGui::BeginTable("ParticleTrackInspectorTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 105.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropTrackName"), [&]() {
					char nameBuf[128];
					strncpy_s(nameBuf, track.trackName.c_str(), sizeof(nameBuf) - 1);
					if (ImGui::InputText("##Name", nameBuf, sizeof(nameBuf))) { track.trackName = nameBuf; m_IsDirty = true; }
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropEnabled"), [&]() {
					if (ImGui::Checkbox("##Enabled", &track.enabled)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropMuted"), [&]() {
					if (ImGui::Checkbox("##Muted", &track.isMuted)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropStartTime"), [&]() {
					if (ImGui::DragFloat("##StartTime", &track.startTime, 0.02f, 0.0f, 30.0f, "%.2fs")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropDuration"), [&]() {
					if (ImGui::DragFloat("##Duration", &track.duration, 0.02f, 0.05f, 30.0f, "%.2fs")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropPlaySpeed"), [&]() {
					if (ImGui::DragFloat("##Speed", &track.playbackSpeed, 0.05f, 0.1f, 5.0f, "%.1fx")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropEffect"), [&]() {
					if (Editor::DrawInspectorResourcePicker(track.effectHandle, true, true)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropPosition"), [&]() {
					if (ImGui::DragFloat3("##Position", &track.localPosition.x, 0.05f)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropRotation"), [&]() {
					if (ImGui::DragFloat3("##Rotation", &track.localRotation.x, 1.0f)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropScale"), [&]() {
					if (ImGui::DragFloat3("##Scale", &track.localScale.x, 0.05f)) m_IsDirty = true;
				});

				ImGui::EndTable();
			}

			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
			{
				tracks.erase(tracks.begin() + m_SelectedTrack.index);
				m_SelectedTrack.Clear();
				m_IsDirty = true;
			}
		}
		// 3. Light Flash Track Inspector
		else if (m_SelectedTrack.type == ESelectedVFXTrackType::Light)
		{
			auto& tracks = effect->GetLightTracks();
			if (m_SelectedTrack.index >= static_cast<int>(tracks.size())) return;
			auto& track = tracks[m_SelectedTrack.index];

			if (ImGui::BeginTable("LightTrackInspectorTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 105.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropTrackName"), [&]() {
					char nameBuf[128];
					strncpy_s(nameBuf, track.trackName.c_str(), sizeof(nameBuf) - 1);
					if (ImGui::InputText("##Name", nameBuf, sizeof(nameBuf))) { track.trackName = nameBuf; m_IsDirty = true; }
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropEnabled"), [&]() {
					if (ImGui::Checkbox("##Enabled", &track.enabled)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropMuted"), [&]() {
					if (ImGui::Checkbox("##Muted", &track.isMuted)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropStartTime"), [&]() {
					if (ImGui::DragFloat("##StartTime", &track.startTime, 0.02f, 0.0f, 30.0f, "%.2fs")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropDuration"), [&]() {
					if (ImGui::DragFloat("##Duration", &track.duration, 0.02f, 0.05f, 30.0f, "%.2fs")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropLightColor"), [&]() {
					if (ImGui::ColorEdit3("##LightColor", &track.lightColor.x)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropPeakIntensity"), [&]() {
					if (ImGui::DragFloat("##PeakIntensity", &track.peakIntensity, 0.2f, 0.0f, 50.0f)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropRadius"), [&]() {
					if (ImGui::DragFloat("##Radius", &track.radius, 0.2f, 0.5f, 50.0f, "%.1fm")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropAttackTime"), [&]() {
					if (ImGui::DragFloat("##AttackTime", &track.attackTime, 0.01f, 0.001f, track.duration, "%.3fs")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropDecayTime"), [&]() {
					if (ImGui::DragFloat("##DecayTime", &track.decayTime, 0.01f, 0.001f, track.duration, "%.3fs")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropPosition"), [&]() {
					if (ImGui::DragFloat3("##Position", &track.localPosition.x, 0.05f)) m_IsDirty = true;
				});

				ImGui::EndTable();
			}

			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
			{
				tracks.erase(tracks.begin() + m_SelectedTrack.index);
				m_SelectedTrack.Clear();
				m_IsDirty = true;
			}
		}
		// 4. Mesh Track Inspector
		else if (m_SelectedTrack.type == ESelectedVFXTrackType::Mesh)
		{
			auto& tracks = effect->GetMeshTracks();
			if (m_SelectedTrack.index >= static_cast<int>(tracks.size())) return;
			auto& track = tracks[m_SelectedTrack.index];

			if (ImGui::BeginTable("MeshTrackInspectorTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 105.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropTrackName"), [&]() {
					char nameBuf[128];
					strncpy_s(nameBuf, track.trackName.c_str(), sizeof(nameBuf) - 1);
					if (ImGui::InputText("##Name", nameBuf, sizeof(nameBuf))) { track.trackName = nameBuf; m_IsDirty = true; }
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropEnabled"), [&]() {
					if (ImGui::Checkbox("##Enabled", &track.enabled)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropStartTime"), [&]() {
					if (ImGui::DragFloat("##StartTime", &track.startTime, 0.02f, 0.0f, 30.0f, "%.2fs")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropDuration"), [&]() {
					if (ImGui::DragFloat("##Duration", &track.duration, 0.02f, 0.05f, 30.0f, "%.2fs")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropMesh"), [&]() {
					if (Editor::DrawInspectorResourcePicker(track.meshHandle, true, true)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropMaterial"), [&]() {
					if (Editor::DrawInspectorResourcePicker(track.materialHandle, true, true)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropPosition"), [&]() {
					if (ImGui::DragFloat3("##Position", &track.localPosition.x, 0.05f)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropScaleStart"), [&]() {
					if (ImGui::DragFloat3("##ScaleStart", &track.scaleStart.x, 0.05f)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropScaleEnd"), [&]() {
					if (ImGui::DragFloat3("##ScaleEnd", &track.scaleEnd.x, 0.05f)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropRotation"), [&]() {
					if (ImGui::DragFloat3("##RotationRate", &track.rotationRate.x, 1.0f)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropUVScroll"), [&]() {
					if (ImGui::DragFloat2("##UVScroll", &track.uvScrollSpeed.x, 0.05f)) m_IsDirty = true;
				});

				ImGui::EndTable();
			}

			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
			{
				tracks.erase(tracks.begin() + m_SelectedTrack.index);
				m_SelectedTrack.Clear();
				m_IsDirty = true;
			}
		}
		// 5. Sound Track Inspector
		else if (m_SelectedTrack.type == ESelectedVFXTrackType::Sound)
		{
			auto& tracks = effect->GetSoundTracks();
			if (m_SelectedTrack.index >= static_cast<int>(tracks.size())) return;
			auto& track = tracks[m_SelectedTrack.index];

			if (ImGui::BeginTable("SoundTrackInspectorTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 105.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropTrackName"), [&]() {
					char nameBuf[128];
					strncpy_s(nameBuf, track.trackName.c_str(), sizeof(nameBuf) - 1);
					if (ImGui::InputText("##Name", nameBuf, sizeof(nameBuf))) { track.trackName = nameBuf; m_IsDirty = true; }
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropEnabled"), [&]() {
					if (ImGui::Checkbox("##Enabled", &track.enabled)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropStartTime"), [&]() {
					if (ImGui::DragFloat("##StartTime", &track.startTime, 0.02f, 0.0f, 30.0f, "%.2fs")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropAudioClip"), [&]() {
					if (Editor::DrawInspectorResourcePicker(track.audioClip, true, true)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropVolume"), [&]() {
					if (ImGui::DragFloat("##Volume", &track.volume, 0.02f, 0.0f, 2.0f, "%.2f")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropPitch"), [&]() {
					if (ImGui::DragFloat("##Pitch", &track.pitch, 0.02f, 0.1f, 3.0f, "%.2f")) m_IsDirty = true;
				});

				ImGui::EndTable();
			}

			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
			{
				tracks.erase(tracks.begin() + m_SelectedTrack.index);
				m_SelectedTrack.Clear();
				m_IsDirty = true;
			}
		}
	}
}
