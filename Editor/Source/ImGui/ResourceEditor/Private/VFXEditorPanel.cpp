#include "VFXEditorPanel.h"
#include "EditorUI.h"
#include "ResourcePicker.h"
#include "IconsFontAwesome7.h"
#include "LocalizationManager.h"
#include "InputManager.h"
#include "ShortcutManager.h"
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
#include "ReflectionPropertyDrawer.h"
#include "ReflectionRegistry.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <type_traits>

namespace Editor
{
	namespace
	{
		struct VFXTrackPresentation { const char* Icon; ImU32 Color; };
		constexpr VFXTrackPresentation g_VFXTrackPresentation[] = {
			{ ICON_FA_IMAGES, IM_COL32(40, 180, 160, 220) },
			{ ICON_FA_WAND_MAGIC_SPARKLES, IM_COL32(230, 130, 40, 220) },
			{ ICON_FA_LIGHTBULB, IM_COL32(230, 200, 50, 220) },
			{ ICON_FA_CUBE, IM_COL32(60, 130, 230, 220) },
			{ ICON_FA_VOLUME_HIGH, IM_COL32(180, 70, 220, 220) },
			{ ICON_FA_WIND, IM_COL32(50, 220, 190, 220) },
			{ ICON_FA_IMAGE, IM_COL32(170, 90, 190, 220) },
			{ ICON_FA_CAMERA, IM_COL32(235, 80, 80, 220) }
		};

		template<class Fn>
		void ForEachVFXTrackList(Engine::CompositeEffect& effect, Fn&& fn)
		{
			fn(ESelectedVFXTrackType::Sprite, effect.GetSpriteTracks());
			fn(ESelectedVFXTrackType::Particle, effect.GetParticleTracks());
			fn(ESelectedVFXTrackType::Light, effect.GetLightTracks());
			fn(ESelectedVFXTrackType::Mesh, effect.GetMeshTracks());
			fn(ESelectedVFXTrackType::Sound, effect.GetSoundTracks());
			fn(ESelectedVFXTrackType::Trail, effect.GetTrailTracks());
			fn(ESelectedVFXTrackType::Decal, effect.GetDecalTracks());
			fn(ESelectedVFXTrackType::CameraShake, effect.GetCameraShakeTracks());
		}

		template<class Fn>
		bool VisitVFXTrack(Engine::CompositeEffect& effect, SelectedVFXTrack selection, Fn&& fn)
		{
			bool found = false;
			ForEachVFXTrackList(effect, [&](ESelectedVFXTrackType type, auto& tracks)
			{
				if (type == selection.type && selection.index >= 0 && static_cast<std::size_t>(selection.index) < tracks.size())
				{
					found = true;
					fn(tracks, static_cast<std::size_t>(selection.index));
				}
			});
			return found;
		}

		float VFXTrackEnd(const auto& track)
		{
			if constexpr (requires { track.duration; })
			{
				if (track.duration > 0.0f) return track.startTime + track.duration;
				if constexpr (requires { track.animationHandle; })
					if (const auto* animation = track.animationHandle.Get()) return track.startTime + animation->GetTotalDuration();
				return track.startTime + EditorUI::MinimumTimelineDuration;
			}
			return track.startTime;
		}

		std::uint64_t VFXRowId(ESelectedVFXTrackType type, std::size_t index)
		{
			return (static_cast<std::uint64_t>(type) << 32) | static_cast<std::uint32_t>(index);
		}
	}

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
		m_TimelineHistory.Clear();
		m_Timeline.Drag.reset();
		m_Timeline.Fit();

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
			m_IsLooping = m_EffectHandle.Get()->IsLooping();
			SetupPreview();
		}
	}

	void VFXEditorPanel::CaptureState(EditorWorkspace& workspace) const
	{
		IImGui::CaptureState(workspace);
		workspace.SetSplitter("VFXEditor.LeftPaneWidth", m_LeftPaneWidth);
		workspace.SetSplitter("VFXEditor.RightPaneWidth", m_RightPaneWidth);
		workspace.SetSplitter("VFXEditor.TimelineHeight", m_TimelineHeight);
		workspace.SetSplitter("VFXEditor.TimelineHeaderWidth", m_Timeline.HeaderWidth);
	}

	void VFXEditorPanel::RestoreState(const EditorWorkspace& workspace)
	{
		IImGui::RestoreState(workspace);
		m_LeftPaneWidth = workspace.GetSplitter("VFXEditor.LeftPaneWidth", m_LeftPaneWidth);
		m_RightPaneWidth = workspace.GetSplitter("VFXEditor.RightPaneWidth", m_RightPaneWidth);
		m_TimelineHeight = workspace.GetSplitter("VFXEditor.TimelineHeight", m_TimelineHeight);
		m_Timeline.HeaderWidth = workspace.GetSplitter("VFXEditor.TimelineHeaderWidth", m_Timeline.HeaderWidth);
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
		comp.previewSeed = 1;
		RestartSimulation();
	}

	float VFXEditorPanel::GetTimelineDuration() const
	{
		const auto* effect = m_EffectHandle.Get();
		const float duration = effect ? effect->GetTotalDuration() : 0.0f;
		return std::isfinite(duration) ? std::max(duration, EditorUI::MinimumTimelineDuration) : 1.0f;
	}

	void VFXEditorPanel::RestartSimulation()
	{
		if (auto* scene = m_PreviewWidget.GetScene(); scene && m_PreviewEntity)
		{
			auto& comp = scene->GetComponent<Engine::CompositeVFXComponent>(*m_PreviewEntity);
			comp.particleRuntimes.clear();
			comp.Restart();
			comp.isPlaying = m_IsPlaying;
		}
		m_CurrentTime = 0.0f;
		m_TimelineSeek.reset();
		m_FixedPreview = true;
	}

	void VFXEditorPanel::ScrubTime(f32 targetTime)
	{
		const float duration = GetTimelineDuration();
		if (!std::isfinite(targetTime)) return;
		if (targetTime < duration)
			targetTime = std::round(targetTime * EditorUI::EffectTimelineFPS) / EditorUI::EffectTimelineFPS;
		targetTime = std::clamp(targetTime, 0.0f, duration);
		m_IsPlaying = false;
		StopPreviewSounds();
		if (!m_FixedPreview || targetTime + 0.0001f < m_CurrentTime) RestartSimulation();
		m_TimelineSeek = targetTime;
	}

	void VFXEditorPanel::InvalidateTimelinePreview()
	{
		m_FixedPreview = false;
		ScrubTime(m_TimelineSeek.value_or(m_CurrentTime));
		m_IsDirty = true;
	}

	void VFXEditorPanel::Update(f32 dt)
	{
		if (!m_Open) return;
		auto* scene = m_PreviewWidget.GetScene();
		if (m_PreviewEntity && scene && m_EffectHandle.IsValid())
		{
			auto& comp = scene->GetComponent<Engine::CompositeVFXComponent>(*m_PreviewEntity);
			if (m_TimelineSeek)
			{
				const float target = *m_TimelineSeek;
				comp.previewSeeking = true;
				comp.isLooping = false;
				comp.playRate = 1.0f;
				comp.isPlaying = true;
				for (int i = 0; i < 32 && comp.currentTime + 0.00001f < target; ++i)
					Engine::CompositeVFXSystem::SimulateSceneVFX(scene,
						std::min(1.0f / EditorUI::EffectTimelineFPS, target - comp.currentTime));
				m_CurrentTime = comp.currentTime;
				if (m_CurrentTime + 0.00001f >= target) m_TimelineSeek.reset();
				comp.previewSeeking = false;
				comp.isPlaying = false;
				Engine::CompositeVFXSystem::RefreshFlashLight(comp);
			}
			else
			{
				comp.isPlaying = m_IsPlaying;
				comp.isLooping = m_IsLooping;
				comp.playRate = m_PlayRate;
				if (m_IsPlaying)
				{
					m_FixedPreview = false;
					Engine::CompositeVFXSystem::SimulateSceneVFX(scene, dt);
					m_CurrentTime = comp.currentTime;
					m_IsPlaying = comp.isPlaying;
				}
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
		m_TimelineHistory.Clear();
		m_Timeline.Drag.reset();
		m_Timeline.Fit();
		m_IsLooping = false;
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

	void VFXEditorPanel::DrawCustomContent()
	{
		EditorUI::DrawEffectEditorLayout(m_LeftPaneWidth, m_RightPaneWidth, m_TimelineHeight,
			[this] { DrawTrackList(); }, [this] { DrawViewport(); },
			[this] { DrawInspector(); }, [this] { DrawTimeline(); });
	}

	void VFXEditorPanel::DrawCustomToolbar()
	{
		const bool hasResource = m_EffectHandle.Get() != nullptr;

		if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.File")))
		{
			if (ImGui::MenuItem(LOCAL_LABEL("ParticleEditor.MenuNew").c_str(), nullptr)) NewEffect();
			if (ImGui::MenuItem(LOCAL_LABEL("ParticleEditor.MenuOpen").c_str(), nullptr)) OpenEffect();
			if (ImGui::MenuItem(LOCAL_LABEL("EditorCommon.Save").c_str(), ShortcutManager::Get().GetShortcutHint("CompositeTimeline.Save"), false, hasResource)) SaveEffect();
			if (ImGui::MenuItem(LOCAL_LABEL("EditorCommon.SaveAs").c_str(), nullptr, false, hasResource)) SaveEffectAs();
			ImGui::Separator();
			if (ImGui::MenuItem(LOCAL_LABEL("EditorCommon.Close").c_str(), nullptr)) m_Open = false;
			ImGui::EndMenu();
		}
	}

	void VFXEditorPanel::DrawCustomQuickAccess()
	{
		const bool hasResource = m_EffectHandle.IsValid() && (m_EffectHandle.Get() != nullptr);
		auto* effect = m_EffectHandle.Get();

		EditorUI::ToolbarLayout toolbar;
		if (toolbar.Icon("New", ICON_FA_FILE, "ParticleEditor.MenuNew")) NewEffect();
		if (toolbar.Icon("Open", ICON_FA_FOLDER_OPEN, "ParticleEditor.MenuOpen")) OpenEffect();
		EditorUI::DrawTimelineAction(toolbar, "CompositeTimeline", "Save", ICON_FA_FLOPPY_DISK, "EditorCommon.Save");
		EditorUI::DrawTimelineAction(toolbar, "CompositeTimeline", "Undo", ICON_FA_ROTATE_LEFT, "EditorCommon.Undo");
		EditorUI::DrawTimelineAction(toolbar, "CompositeTimeline", "Redo", ICON_FA_ROTATE_RIGHT, "EditorCommon.Redo");
		EditorUI::DrawTimelineTransport(toolbar, "CompositeTimeline", m_IsPlaying, m_IsLooping);
		ImGui::BeginDisabled(!hasResource);
		toolbar.Text(LOCAL_CSTR("VFXEditor.Speed"));
		toolbar.SetNextItemWidth(ImGui::GetFontSize() * 5.0f);
		if (ImGui::DragFloat("##Speed", &m_PlayRate, 0.05f, 0.1f, 3.0f, "%.1fx"))
		{
			m_PlayRate = std::clamp(m_PlayRate, 0.1f, 5.0f);
		}

		if (effect)
		{
			if (toolbar.Button("AutoDuration", ICON_FA_ARROWS_ROTATE, "VFXEditor.AutoDuration"))
			{
				const float previous = effect->GetTotalDuration();
				effect->RecalculateDurationFromTracks();
				const float next = effect->GetTotalDuration();
				effect->SetTotalDuration(previous);
				if (next != previous)
					ApplyTimelineEdit({ EditorUI::TimelineDragMode::Duration, 0, 0.0f, next });
			}
			toolbar.Text(LOCAL_FMT("Timeline.TimeCounter", m_TimelineSeek.value_or(m_CurrentTime), GetTimelineDuration()).c_str());
		}

		ImGui::EndDisabled();
	}

	void VFXEditorPanel::DrawTrackList()
	{
		EditorUI::ToolbarLayout toolbar;
		auto* effect = m_EffectHandle.Get();
		if (!effect)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOCAL_CSTR("VFXEditor.NoEffectLoaded"));
			ImGui::Spacing();
			if (toolbar.Button("VFXEditor.CreateNewEffect", ICON_FA_FILE, "VFXEditor.CreateNewEffect"))
			{
				NewEffect();
			}
			if (toolbar.Button("ParticleEditor.MenuOpen", ICON_FA_FOLDER_OPEN, "ParticleEditor.MenuOpen"))
			{
				OpenEffect();
			}
			return;
		}

		ImGuiTextBuffer trackLabel;
		trackLabel.appendf("%s %s (%u)", ICON_FA_LAYER_GROUP, LOCAL_CSTR("VFXEditor.Tracks"), effect->GetTotalTrackCount());
		toolbar.Text(trackLabel.c_str(), false);
		if (toolbar.Button("AddTrack", ICON_FA_PLUS, "VFXEditor.AddTrack"))
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
				m_TimelineHistory.Clear();
				InvalidateTimelinePreview();
			}
			if (ImGui::MenuItem((std::string(ICON_FA_WAND_MAGIC_SPARKLES " ") + LOCAL_CSTR("VFXEditor.AddParticleTrack")).c_str()))
			{
				Engine::VFXParticleTrack track;
				track.trackName = "Particle_" + std::to_string(effect->GetParticleTracks().size() + 1);
				effect->GetParticleTracks().push_back(track);
				m_SelectedTrack = { ESelectedVFXTrackType::Particle, static_cast<int>(effect->GetParticleTracks().size() - 1) };
				m_TimelineHistory.Clear();
				InvalidateTimelinePreview();
			}
			if (ImGui::MenuItem((std::string(ICON_FA_LIGHTBULB " ") + LOCAL_CSTR("VFXEditor.AddLightTrack")).c_str()))
			{
				Engine::VFXLightTrack track;
				track.trackName = "Flash_" + std::to_string(effect->GetLightTracks().size() + 1);
				effect->GetLightTracks().push_back(track);
				m_SelectedTrack = { ESelectedVFXTrackType::Light, static_cast<int>(effect->GetLightTracks().size() - 1) };
				m_TimelineHistory.Clear();
				InvalidateTimelinePreview();
			}
			if (ImGui::MenuItem((std::string(ICON_FA_CUBE " ") + LOCAL_CSTR("VFXEditor.AddMeshTrack")).c_str()))
			{
				Engine::VFXMeshTrack track;
				track.trackName = "Mesh_" + std::to_string(effect->GetMeshTracks().size() + 1);
				effect->GetMeshTracks().push_back(track);
				m_SelectedTrack = { ESelectedVFXTrackType::Mesh, static_cast<int>(effect->GetMeshTracks().size() - 1) };
				m_TimelineHistory.Clear();
				InvalidateTimelinePreview();
			}
			if (ImGui::MenuItem((std::string(ICON_FA_MUSIC " ") + LOCAL_CSTR("VFXEditor.AddSoundTrack")).c_str()))
			{
				Engine::VFXSoundTrack track;
				track.trackName = "Audio_" + std::to_string(effect->GetSoundTracks().size() + 1);
				effect->GetSoundTracks().push_back(track);
				m_SelectedTrack = { ESelectedVFXTrackType::Sound, static_cast<int>(effect->GetSoundTracks().size() - 1) };
				m_TimelineHistory.Clear();
				InvalidateTimelinePreview();
			}
			if (ImGui::MenuItem((std::string(ICON_FA_LINES_LEANING " ") + LOCAL_CSTR("VFXEditor.AddTrailTrack")).c_str()))
			{
				Engine::VFXTrailTrack track;
				track.trackName = "Trail_" + std::to_string(effect->GetTrailTracks().size() + 1);
				effect->GetTrailTracks().push_back(track);
				m_SelectedTrack = { ESelectedVFXTrackType::Trail, static_cast<int>(effect->GetTrailTracks().size() - 1) };
				m_TimelineHistory.Clear();
				InvalidateTimelinePreview();
			}
			if (ImGui::MenuItem((std::string(ICON_FA_STAMP " ") + LOCAL_CSTR("VFXEditor.AddDecalTrack")).c_str()))
			{
				Engine::VFXDecalTrack track;
				track.trackName = "Decal_" + std::to_string(effect->GetDecalTracks().size() + 1);
				effect->GetDecalTracks().push_back(track);
				m_SelectedTrack = { ESelectedVFXTrackType::Decal, static_cast<int>(effect->GetDecalTracks().size() - 1) };
				m_TimelineHistory.Clear();
				InvalidateTimelinePreview();
			}
			if (ImGui::MenuItem((std::string(ICON_FA_EARTH_AMERICAS " ") + LOCAL_CSTR("VFXEditor.AddCameraShakeTrack")).c_str()))
			{
				Engine::VFXCameraShakeTrack track;
				track.trackName = "Shake_" + std::to_string(effect->GetCameraShakeTracks().size() + 1);
				effect->GetCameraShakeTracks().push_back(track);
				m_SelectedTrack = { ESelectedVFXTrackType::CameraShake, static_cast<int>(effect->GetCameraShakeTracks().size() - 1) };
				m_TimelineHistory.Clear();
				InvalidateTimelinePreview();
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
			ImGui::BulletText("%s", LOCAL_CSTR("VFXEditor.AddTrailTrack"));
			ImGui::BulletText("%s", LOCAL_CSTR("VFXEditor.AddDecalTrack"));
			ImGui::BulletText("%s", LOCAL_CSTR("VFXEditor.AddCameraShakeTrack"));
			return;
		}

		ForEachVFXTrackList(*effect, [&](ESelectedVFXTrackType type, auto& tracks)
		{
			const auto& style = g_VFXTrackPresentation[static_cast<int>(type)];
			ImGui::PushID(static_cast<int>(type));
			for (std::size_t i = 0; i < tracks.size(); ++i)
			{
				auto& track = tracks[i];
				ImGui::PushID(static_cast<int>(i));
				if (ImGui::Checkbox("##Enabled", &track.enabled)) InvalidateTimelinePreview();
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(style.Color));
				const auto label = std::string(style.Icon) + " " + track.trackName + "###Track";
				const bool selected = m_SelectedTrack.type == type && m_SelectedTrack.index == static_cast<int>(i);
				if (ImGui::Selectable(label.c_str(), selected)) SelectTrack({ type, static_cast<int>(i) });
				ImGui::PopStyleColor();
				ImGui::PopID();
			}
			ImGui::PopID();
		});
	}

	void VFXEditorPanel::DrawViewport()
	{
		m_PreviewWidget.Draw();
	}

	void VFXEditorPanel::SelectTrack(SelectedVFXTrack selection)
	{
		auto* effect = m_EffectHandle.Get();
		if (!effect) return;
		VisitVFXTrack(*effect, selection, [&](auto& tracks, std::size_t index)
		{
			m_SelectedTrack = selection;
			const auto& track = tracks[index];
			if constexpr (requires { track.localPosition; })
				m_PreviewWidget.FocusTarget(track.localPosition);
			else if constexpr (requires { track.localOffsetA; track.localOffsetB; })
				m_PreviewWidget.FocusTarget((track.localOffsetA + track.localOffsetB) * 0.5f);
			else
				m_PreviewWidget.FocusTarget(Engine::vec3(0.0f, 0.5f, 0.0f));

			if constexpr (requires { track.duration; })
			{
				const float time = m_TimelineSeek.value_or(m_CurrentTime);
				const float end = VFXTrackEnd(track);
				if (time < track.startTime || time >= end)
					ScrubTime(std::min(end, track.startTime + 1.0f / EditorUI::EffectTimelineFPS));
			}
		});
	}

	void VFXEditorPanel::DeleteSelectedTrack()
	{
		auto* effect = m_EffectHandle.Get();
		if (!effect) return;
		const auto selected = m_SelectedTrack;
		VisitVFXTrack(*effect, selected, [&](auto& tracks, std::size_t index)
		{
			using Track = typename std::remove_reference_t<decltype(tracks)>::value_type;
			const auto removed = tracks[index];
			const auto handle = m_EffectHandle;
			const auto apply = [this, handle, selected, removed](bool undo)
			{
				auto* target = handle.Get();
				if (!target) return false;
				bool changed = false;
				ForEachVFXTrackList(*target, [&](ESelectedVFXTrackType type, auto& list)
				{
					if constexpr (std::is_same_v<typename std::remove_reference_t<decltype(list)>::value_type, Track>)
					{
						const auto i = static_cast<std::size_t>(selected.index);
						if (type != selected.type || i > list.size() || (!undo && i == list.size())) return;
						if (undo) list.insert(list.begin() + i, removed);
						else list.erase(list.begin() + i);
						changed = true;
					}
				});
				if (changed)
				{
					m_SelectedTrack = undo ? selected : SelectedVFXTrack{};
					m_Timeline.Drag.reset();
					InvalidateTimelinePreview();
				}
				return changed;
			};
			m_TimelineHistory.Execute(std::make_unique<CallbackEditorCommand>(LOCAL("Action.Timeline.DeleteSelected"),
				[apply] { return apply(false); }, [apply] { return apply(true); }));
		});
	}

	void VFXEditorPanel::ApplyTimelineEdit(const EditorUI::TimelineEdit& edit)
	{
		auto* effect = m_EffectHandle.Get();
		if (!effect) return;
		const auto handle = m_EffectHandle;
		const float oldDuration = GetTimelineDuration();
		if (edit.Mode == EditorUI::TimelineDragMode::Duration)
		{
			const auto apply = [this, handle](float duration)
			{
				auto* target = handle.Get();
				if (!target) return false;
				target->SetTotalDuration(duration);
				InvalidateTimelinePreview();
				return true;
			};
			m_TimelineHistory.Execute(std::make_unique<CallbackEditorCommand>(LOCAL("Timeline.Duration"),
				[apply, edit] { return apply(edit.End); }, [apply, oldDuration] { return apply(oldDuration); }));
			return;
		}

		const SelectedVFXTrack selection{ static_cast<ESelectedVFXTrackType>(edit.Row >> 32), static_cast<int>(edit.Row & 0xffffffffu) };
		VisitVFXTrack(*effect, selection, [&](auto& tracks, std::size_t index)
		{
			const auto& track = tracks[index];
			const float oldStart = track.startTime;
			float oldLength = 0.0f;
			if constexpr (requires { track.duration; }) oldLength = track.duration;
			const float length = edit.Mode == EditorUI::TimelineDragMode::Move ? oldLength : edit.End - edit.Start;
			const float total = std::max(oldDuration, edit.End + (length == 0.0f ? EditorUI::MinimumTimelineDuration : 0.0f));
			const auto apply = [this, handle, selection](float start, float duration, float totalDuration)
			{
				auto* target = handle.Get();
				if (!target) return false;
				if (!VisitVFXTrack(*target, selection, [&](auto& list, std::size_t i)
				{
					list[i].startTime = start;
					if constexpr (requires { list[i].duration; }) list[i].duration = duration;
				})) return false;
				target->SetTotalDuration(totalDuration);
				InvalidateTimelinePreview();
				return true;
			};
			m_TimelineHistory.Execute(std::make_unique<CallbackEditorCommand>(LOCAL("EffectTimeline.EditRange"),
				[apply, edit, length, total] { return apply(edit.Start, length, total); },
				[apply, oldStart, oldLength, oldDuration] { return apply(oldStart, oldLength, oldDuration); }));
		});
	}

	void VFXEditorPanel::DrawTimeline()
	{
		auto* effect = m_EffectHandle.Get();
		if (!effect)
		{
			EditorUI::EmptyState("VFXEditor.TimelineUnavailable");
			return;
		}
		std::vector<EditorUI::TimelineRow> rows;
		rows.reserve(effect->GetTotalTrackCount());
		float minimumDuration = EditorUI::MinimumTimelineDuration;
		ForEachVFXTrackList(*effect, [&](ESelectedVFXTrackType type, const auto& tracks)
		{
			const auto& style = g_VFXTrackPresentation[static_cast<int>(type)];
			for (std::size_t i = 0; i < tracks.size(); ++i)
			{
				const auto& track = tracks[i];
				const float end = VFXTrackEnd(track);
				if (!std::isfinite(track.startTime) || !std::isfinite(end)) continue;
				const bool marker = type == ESelectedVFXTrackType::Sound;
				minimumDuration = std::max(minimumDuration, end + (marker ? EditorUI::MinimumTimelineDuration : 0.0f));
				rows.push_back({ VFXRowId(type, i), track.trackName, style.Icon, style.Color, track.startTime, end,
					m_SelectedTrack.type == type && m_SelectedTrack.index == static_cast<int>(i),
					marker ? EditorUI::TimelineRowKind::Marker : EditorUI::TimelineRowKind::Interval });
			}
		});
		const auto result = EditorUI::DrawEffectTimeline(m_Timeline, rows, GetTimelineDuration(),
			m_TimelineSeek.value_or(m_CurrentTime), minimumDuration);
		if (result.Selection)
			SelectTrack({ static_cast<ESelectedVFXTrackType>(*result.Selection >> 32), static_cast<int>(*result.Selection & 0xffffffffu) });
		if (result.Seek) ScrubTime(*result.Seek);
		if (result.Commit) ApplyTimelineEdit(*result.Commit);
	}

	bool VFXEditorPanel::CanExecuteTimelineAction(EditorUI::TimelineAction action) const
	{
		using enum EditorUI::TimelineAction;
		auto* effect = m_EffectHandle.Get();
		if (!m_Open || !effect || m_Timeline.Drag) return false;
		if (action == Undo) return m_TimelineHistory.CanUndo();
		if (action == Redo) return m_TimelineHistory.CanRedo();
		if (action == Delete) return VisitVFXTrack(*effect, m_SelectedTrack, [](auto&, std::size_t) {});
		return true;
	}

	void VFXEditorPanel::ExecuteTimelineAction(EditorUI::TimelineAction action)
	{
		using enum EditorUI::TimelineAction;
		if (!CanExecuteTimelineAction(action)) return;
		const float time = m_TimelineSeek.value_or(m_CurrentTime);
		switch (action)
		{
		case PlayPause:
			if (!m_IsPlaying && time >= GetTimelineDuration()) RestartSimulation();
			m_IsPlaying = !m_IsPlaying;
			if (!m_IsPlaying) StopPreviewSounds();
			break;
		case Previous: ScrubTime(time - 1.0f / EditorUI::EffectTimelineFPS); break;
		case Next: ScrubTime(time + 1.0f / EditorUI::EffectTimelineFPS); break;
		case First: ScrubTime(0.0f); break;
		case Last: ScrubTime(GetTimelineDuration()); break;
		case Loop:
			m_IsLooping = !m_IsLooping;
			m_EffectHandle.Get()->SetLooping(m_IsLooping);
			m_IsDirty = true;
			break;
		case Fit: m_Timeline.Fit(); break;
		case Delete: DeleteSelectedTrack(); break;
		case Undo: m_TimelineHistory.Undo(); break;
		case Redo: m_TimelineHistory.Redo(); break;
		case Save: SaveEffect(); break;
		}
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
			if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return;
			auto& track = tracks[m_SelectedTrack.index];

			if (EditorUI::BeginSettingsTable("SpriteTrackInspectorTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
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

				EditorUI::EndSettingsTable();
			}

			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
			{
				DeleteSelectedTrack();
			}
		}
		// 2. Particle Track Inspector
		else if (m_SelectedTrack.type == ESelectedVFXTrackType::Particle)
		{
			auto& tracks = effect->GetParticleTracks();
			if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return;
			auto& track = tracks[m_SelectedTrack.index];

			if (EditorUI::BeginSettingsTable("ParticleTrackInspectorTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
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

				EditorUI::EndSettingsTable();
			}

			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
			{
				DeleteSelectedTrack();
			}
		}
		// 3. Light Flash Track Inspector
		else if (m_SelectedTrack.type == ESelectedVFXTrackType::Light)
		{
			auto& tracks = effect->GetLightTracks();
			if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return;
			auto& track = tracks[m_SelectedTrack.index];

			if (EditorUI::BeginSettingsTable("LightTrackInspectorTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
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

				EditorUI::EndSettingsTable();
			}

			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
			{
				DeleteSelectedTrack();
			}
		}
		// 4. Mesh Track Inspector
		else if (m_SelectedTrack.type == ESelectedVFXTrackType::Mesh)
		{
			auto& tracks = effect->GetMeshTracks();
			if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return;
			auto& track = tracks[m_SelectedTrack.index];

			if (EditorUI::BeginSettingsTable("MeshTrackInspectorTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
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

				EditorUI::EndSettingsTable();
			}

			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
			{
				DeleteSelectedTrack();
			}
		}
		// 5. Sound Track Inspector
		else if (m_SelectedTrack.type == ESelectedVFXTrackType::Sound)
		{
			auto& tracks = effect->GetSoundTracks();
			if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return;
			auto& track = tracks[m_SelectedTrack.index];

			const auto object = reflection::ObjectView::From(track);
			const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
			const auto* type = registry.FindType(object);
			const auto drawRegistered = [&](const char* key, std::string_view name, auto&& fallback)
			{
				EditorUI::PropertyRow(key);
				if (type)
				{
					const auto property = std::find_if(type->Properties.begin(), type->Properties.end(),
						[&](const auto& p) { return p.Name == name; });
					if (property != type->Properties.end())
						return ReflectionPropertyDrawer::DrawValue(*property, object, &registry);
				}
				ImGui::SetNextItemWidth(-FLT_MIN);
				return fallback();
			};
			EditorUI::PropertyTableStyle style;
			style.LabelWidth = ImGui::GetFontSize() * 7.0f;
			if (EditorUI::BeginPropertyTable("SoundTrackInspectorTable", style))
			{
				if (drawRegistered("VFXEditor.PropTrackName", "trackName", [&] {
					char nameBuf[128];
					strncpy_s(nameBuf, track.trackName.c_str(), sizeof(nameBuf) - 1);
					if (!ImGui::InputText("##Name", nameBuf, sizeof(nameBuf))) return false;
					track.trackName = nameBuf;
					return true;
				})) m_IsDirty = true;

				if (drawRegistered("VFXEditor.PropEnabled", "enabled",
					[&] { return ImGui::Checkbox("##Enabled", &track.enabled); }))
					InvalidateTimelinePreview();

				EditorUI::PropertyRow("VFXEditor.PropStartTime");
				ImGui::SetNextItemWidth(-FLT_MIN);

				const auto row = VFXRowId(ESelectedVFXTrackType::Sound, static_cast<std::size_t>(m_SelectedTrack.index));
				float startTime = m_SoundStartEdit && m_SoundStartEdit->Row == row
					? m_SoundStartEdit->Start
					: track.startTime;

				ImGui::PushID(m_SelectedTrack.index);
				ImGui::DragFloat("##StartTime", &startTime, 0.02f, 0.0f, 30.0f, "%.2fs", ImGuiSliderFlags_AlwaysClamp);
				const bool active = ImGui::IsItemActive();
				const bool commit = ImGui::IsItemDeactivatedAfterEdit();
				ImGui::PopID();

				const EditorUI::TimelineEdit edit{ EditorUI::TimelineDragMode::Move, row, startTime, startTime };
				if (active)
					m_SoundStartEdit = edit;
				else
					m_SoundStartEdit.reset();

				if (commit && std::isfinite(startTime) && startTime != track.startTime)
					ApplyTimelineEdit(edit);

				const float maxSoundTime = std::max(30.0f, GetTimelineDuration());

				EditorUI::PropertyRow("VFXEditor.PropSoundDuration");
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::DragFloat("##SoundDuration", &track.playDuration, 0.02f, 0.0f, maxSoundTime, "%.2fs",
					ImGuiSliderFlags_AlwaysClamp))
					InvalidateTimelinePreview();

				EditorUI::PropertyRow("VFXEditor.PropFadeOutDuration");
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::DragFloat("##FadeOutDuration", &track.fadeOutDuration, 0.01f, 0.0f, maxSoundTime, "%.2fs",
					ImGuiSliderFlags_AlwaysClamp))
					InvalidateTimelinePreview();

				if (drawRegistered("VFXEditor.PropAudioClip", "audioClip",
					[&] { return Editor::DrawInspectorResourcePicker(track.audioClip, true, true); }))
					InvalidateTimelinePreview();

				EditorUI::PropertyRow("VFXEditor.PropVolume");
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::DragFloat("##Volume", &track.volume, 0.02f, 0.0f, 2.0f, "%.2f")) m_IsDirty = true;

				EditorUI::PropertyRow("VFXEditor.PropPitch");
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::DragFloat("##Pitch", &track.pitch, 0.02f, 0.1f, 3.0f, "%.2f")) m_IsDirty = true;

				EditorUI::EndPropertyTable();
			}

			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
			{
				DeleteSelectedTrack();
			}
		}
		// 6. Trail Track Inspector
		else if (m_SelectedTrack.type == ESelectedVFXTrackType::Trail)
		{
			auto& tracks = effect->GetTrailTracks();
			if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return;
			auto& track = tracks[m_SelectedTrack.index];

			if (EditorUI::BeginSettingsTable("TrailTrackInspectorTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 110.0f);
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

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropLoop"), [&]() {
					if (ImGui::Checkbox("##Loop", &track.loop)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropTrailWidth"), [&]() {
					if (ImGui::DragFloat("##TrailWidth", &track.trailWidth, 0.02f, 0.01f, 10.0f, "%.2f")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropLifetime"), [&]() {
					if (ImGui::DragFloat("##Lifetime", &track.lifetime, 0.02f, 0.02f, 5.0f, "%.2fs")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropMinVertexDistance"), [&]() {
					if (ImGui::DragFloat("##MinDist", &track.minVertexDistance, 0.01f, 0.005f, 2.0f, "%.3f")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropTaperRatio"), [&]() {
					if (ImGui::DragFloat("##Taper", &track.taperRatio, 0.02f, 0.0f, 1.0f, "%.2f")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropOffsetA"), [&]() {
					if (ImGui::DragFloat3("##OffsetA", &track.localOffsetA.x, 0.02f)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropOffsetB"), [&]() {
					if (ImGui::DragFloat3("##OffsetB", &track.localOffsetB.x, 0.02f)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropColorStart"), [&]() {
					if (ImGui::ColorEdit4("##ColorStart", &track.colorStart.x, ImGuiColorEditFlags_Float)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropColorEnd"), [&]() {
					if (ImGui::ColorEdit4("##ColorEnd", &track.colorEnd.x, ImGuiColorEditFlags_Float)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropTexture"), [&]() {
					if (Editor::DrawInspectorResourcePicker(track.textureHandle, true, true)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropMaterial"), [&]() {
					if (Editor::DrawInspectorResourcePicker(track.materialHandle, true, true)) m_IsDirty = true;
				});

				EditorUI::EndSettingsTable();
			}

			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
			{
				DeleteSelectedTrack();
			}
		}
		// 7. Decal Track Inspector
		else if (m_SelectedTrack.type == ESelectedVFXTrackType::Decal)
		{
			auto& tracks = effect->GetDecalTracks();
			if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return;
			auto& track = tracks[m_SelectedTrack.index];

			if (EditorUI::BeginSettingsTable("DecalTrackInspectorTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 110.0f);
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

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropFadeStartTime"), [&]() {
					if (ImGui::DragFloat("##FadeStart", &track.fadeStartTime, 0.02f, 0.0f, 30.0f, "%.2fs")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropPosition"), [&]() {
					if (ImGui::DragFloat3("##Position", &track.localPosition.x, 0.05f)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropRotation"), [&]() {
					if (ImGui::DragFloat3("##Rotation", &track.localRotation.x, 1.0f)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropSize"), [&]() {
					if (ImGui::DragFloat2("##Size", &track.size.x, 0.05f, 0.05f, 50.0f, "%.2f")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropColorTint"), [&]() {
					if (ImGui::ColorEdit4("##ColorTint", &track.colorTint.x, ImGuiColorEditFlags_Float)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropTexture"), [&]() {
					if (Editor::DrawInspectorResourcePicker(track.textureHandle, true, true)) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropMaterial"), [&]() {
					if (Editor::DrawInspectorResourcePicker(track.materialHandle, true, true)) m_IsDirty = true;
				});

				EditorUI::EndSettingsTable();
			}

			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
			{
				DeleteSelectedTrack();
			}
		}
		// 8. Camera Shake Track Inspector
		else if (m_SelectedTrack.type == ESelectedVFXTrackType::CameraShake)
		{
			auto& tracks = effect->GetCameraShakeTracks();
			if (m_SelectedTrack.index < 0 || m_SelectedTrack.index >= static_cast<int>(tracks.size())) return;
			auto& track = tracks[m_SelectedTrack.index];

			if (EditorUI::BeginSettingsTable("CameraShakeInspectorTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 110.0f);
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
					if (ImGui::DragFloat("##Duration", &track.duration, 0.02f, 0.05f, 10.0f, "%.2fs")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropAmplitude"), [&]() {
					if (ImGui::DragFloat("##Amplitude", &track.amplitude, 0.02f, 0.01f, 5.0f, "%.2f")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropFrequency"), [&]() {
					if (ImGui::DragFloat("##Frequency", &track.frequency, 0.5f, 1.0f, 100.0f, "%.1f Hz")) m_IsDirty = true;
				});

				DrawPropertyRow(LOCAL_CSTR("VFXEditor.PropShakeRadius"), [&]() {
					if (ImGui::DragFloat("##Radius", &track.radius, 0.5f, 0.0f, 100.0f, "%.1fm (0=Global)")) m_IsDirty = true;
				});

				EditorUI::EndSettingsTable();
			}

			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_TRASH " ") + LOCAL_CSTR("EditorCommon.Delete")).c_str()))
			{
				DeleteSelectedTrack();
			}
		}
	}

	void VFXEditorPanel::StopPreviewSounds()
	{
		if (auto* scene = m_PreviewWidget.GetScene(); scene && m_PreviewEntity)
			scene->GetComponent<Engine::CompositeVFXComponent>(*m_PreviewEntity).StopSoundTracks();
	}
}
