#include "GraphicSettingPanel.h"
#include "Application.h"
#include "EditorSettings.h"
#include "ImGuiManager.h"
#include "TimeManager.h"
#include "EditorSettingsService.h"
#include "NotificationManager.h"
#include "BaseViewportPanel.h"

#pragma region Constructor & Destructor
GraphicSettingPanel::GraphicSettingPanel()
{
	m_Name = L"GraphicsSettings";
	m_DisplayNameKey = "GraphicsSettings.Title";
	m_Open = false;
}

EResult GraphicSettingPanel::Initialize(void* arg)
{
	if (!arg)
		return EResult::InvalidArgument;

	const auto& desc = *static_cast<const GraphicSettingPanelDesc*>(arg);

	if (!desc.Window || !EditorSettingsService::IsCreated())
		return EResult::InvalidArgument;

	m_Window = desc.Window;
	return EResult::Success;
}

void GraphicSettingPanel::Free()
{
	m_Window = nullptr;
	IImGui::Free();
}
#pragma endregion

void GraphicSettingPanel::ClearDraftChanges()
{
	m_WindowDirty = false;
	m_UIScaleDirty = false;
	m_TargetFPSDirty = false;
	m_VSyncDirty = false;
	m_ViewportDrafts.clear();
}

void GraphicSettingPanel::RefreshDrafts()
{
	if (!m_Window)
		return;

	if (!m_WindowDirty)
	{
		Application::Get().GetWindowSize(&m_WindowWidth, &m_WindowHeight);
		m_Fullscreen = (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN) != 0;
	}

	if (!m_UIScaleDirty)
		m_UIScale = ImGuiManager::Get().GetUIScale();

	if (!m_TargetFPSDirty)
		m_TargetFPS = static_cast<int32>(TimeManager::Get().GetTargetFPS());

	auto& settings = EditorSettingsService::Get();

	if (!m_VSyncDirty && !settings.IsVSyncChangePending())
	{
		if (const auto actual = settings.GetEditorVSync())
			m_VSync = *actual;
	}
}

void GraphicSettingPanel::CaptureState(EditorWorkspace& workspace) const
{
	IImGui::CaptureState(workspace);
	workspace.DisplaySettingsOpen = IsOpen();
}

void GraphicSettingPanel::RestoreState(const EditorWorkspace& workspace)
{
	IImGui::RestoreState(workspace);

	if (!workspace.PanelOpenStates.contains(GetName()))
		SetOpen(workspace.DisplaySettingsOpen);

	ClearDraftChanges();
}

void GraphicSettingPanel::Update(f32 dt)
{
	auto& settings = EditorSettingsService::Get();

	if (m_StartupSettingsNoticePending)
	{
		m_StartupSettingsNoticePending = false;

		if (settings.IsPreferencesSaveBlocked())
		{
			NotificationManager::Get().AddNotification(LOCAL("EditorSettings.Title"),
				LOCAL("EditorSettings.PreferencesStartupFallback"), ELogLevel::Warn);
		}

		if (settings.IsWorkspaceSaveBlocked())
		{
			NotificationManager::Get().AddNotification(LOCAL("EditorSettings.Title"),
				LOCAL("EditorSettings.WorkspaceStartupFallback"), ELogLevel::Warn);
		}
	}

	if (const auto result = settings.TakeVSyncRequestResult())
	{
		if (result->Applied)
		{
			m_VSyncDirty = false;

			if (const auto actual = settings.GetEditorVSync())
				m_VSync = *actual;
		}

		if (result->Applied && result->SaveStatus == EEditorSettingsSaveStatus::Saved)
		{
			NotificationManager::Get().AddNotification(LOCAL("GraphicsSettings.Title"),
				LOCAL("GraphicsSettings.VSyncSaved"), ELogLevel::Info);
		}
		else
		{
			ImGuiManager::Get().NotifySettingsChange(*result);
		}
	}

	if (const auto result = settings.TakeViewportResolutionResult())
	{
		if (result->Change.Applied)
			m_ViewportDrafts.erase(result->Name);

		if (!result->RenderTargetsValid)
		{
			NotificationManager::Get().AddNotification(LOCAL("GraphicsSettings.Title"),
				LOCAL("GraphicsSettings.ViewportRestoreFailed"), ELogLevel::Error);
		}
		else if (result->Change.Applied && result->Change.SaveStatus == EEditorSettingsSaveStatus::Saved)
		{
			NotificationManager::Get().AddNotification(LOCAL("GraphicsSettings.Title"),
				LOCAL("GraphicsSettings.ViewportSaved"), ELogLevel::Info);
		}
		else
		{
			ImGuiManager::Get().NotifySettingsChange(result->Change);
		}
	}

	const auto result = EditorSettingsService::Get().TakeWindowRequestResult();

	if (!result)
		return;

	const auto status = result->Status;
	const bool refresh = status == EEditorWindowRequestStatus::Confirmed ||
		status == EEditorWindowRequestStatus::Cancelled ||
		status == EEditorWindowRequestStatus::ConfirmationTimedOut;

	if (refresh && result->ActualState.HasWindowSize)
	{
		m_WindowWidth = result->ActualState.WindowWidth;
		m_WindowHeight = result->ActualState.WindowHeight;
		m_Fullscreen = result->ActualState.WindowFullscreen;
		m_WindowDirty = false;
	}

	if (status == EEditorWindowRequestStatus::Confirmed)
	{
		if (result->SaveStatus == EEditorSettingsSaveStatus::Saved)
		{
			NotificationManager::Get().AddNotification(LOCAL("EditorSettings.Title"),
				LOCAL("EditorSettings.WindowConfirmed"), ELogLevel::Info);
		}
		else
		{
			ImGuiManager::Get().NotifySettingsChange({ true, result->SaveStatus });
		}

		return;
	}

	const char* key = "EditorSettings.WindowRequestFailed";
	auto level = ELogLevel::Error;

	if (status == EEditorWindowRequestStatus::Cancelled)
	{
		key = "EditorSettings.WindowReverted";
		level = ELogLevel::Info;
	}
	else if (status == EEditorWindowRequestStatus::ConfirmationTimedOut)
	{
		key = "EditorSettings.WindowConfirmationTimedOut";
		level = ELogLevel::Warn;
	}
	else if (status == EEditorWindowRequestStatus::TimedOut)
	{
		key = "EditorSettings.WindowApplyTimedOut";
	}
	else if (status == EEditorWindowRequestStatus::RestoreFailed)
	{
		key = "EditorSettings.WindowRestoreFailed";
	}

	NotificationManager::Get().AddNotification(LOCAL("EditorSettings.Title"), LOCAL(key), level);
}

void GraphicSettingPanel::Draw()
{
	if (!m_Window)
		return;

	if (!m_Open)
	{
		ClearDraftChanges();
		return;
	}

	RefreshDrafts();

	const bool dirty = m_WindowDirty || m_UIScaleDirty || m_TargetFPSDirty || m_VSyncDirty || !m_ViewportDrafts.empty();
	const ImGuiWindowFlags flags = dirty ? ImGuiWindowFlags_UnsavedDocument : ImGuiWindowFlags_None;

	// 기존 화면 설정 창의 ID를 유지해 저장된 도킹 배치를 재사용한다.
	const string title = LOCAL("GraphicsSettings.Title") + "###ToolBar.DisplaySettings";

	if (ImGui::Begin(title.c_str(), &m_Open, flags))
	{
		auto& settings = EditorSettingsService::Get();
		DrawSettingsStorageStatus();

		const bool windowPending = settings.IsWindowChangePending();

		ImGui::BeginDisabled(windowPending || settings.IsVSyncChangePending() || settings.IsViewportResolutionChangePending());

		ImGui::TextUnformatted(LOCAL_CSTR("Viewport.Resolution"));

		ImGui::BeginDisabled(m_Fullscreen);
		m_WindowDirty |= ImGui::InputInt(LOCAL_LABEL("EditorCommon.Width").c_str(), &m_WindowWidth);
		m_WindowDirty |= ImGui::InputInt(LOCAL_LABEL("EditorCommon.Height").c_str(), &m_WindowHeight);
		ImGui::EndDisabled();

		m_WindowDirty |= ImGui::Checkbox(LOCAL_LABEL("ToolBar.Fullscreen").c_str(), &m_Fullscreen);

		const EditorWindowRequest request{ m_WindowWidth, m_WindowHeight, m_Fullscreen };
		const bool validSize = request.HasValidValues();

		if (!validSize)
			ImGui::TextUnformatted(LOCAL_CSTR("GraphicsSettings.InvalidWindowSize"));

		ImGui::BeginDisabled(!validSize);

		if (ImGui::Button(LOCAL_LABEL("ToolBar.ApplyResolution").c_str()))
		{
			const auto result = settings.RequestWindowChange(request);

			if (result != EResult::Success)
			{
				NotificationManager::Get().AddNotification(LOCAL("EditorSettings.Title"),
					LOCAL("EditorSettings.WindowRequestRejected"), ELogLevel::Error);
			}
		}

		ImGui::EndDisabled();
		ImGui::EndDisabled();

		if (windowPending)
			ImGui::TextUnformatted(LOCAL_CSTR("EditorSettings.WindowApplying"));

		if (settings.IsWindowCaptureBlocked() && !windowPending)
			ImGui::TextWrapped("%s", LOCAL_CSTR("EditorSettings.WindowCaptureBlocked"));

		ImGui::Separator();

		m_UIScaleDirty |= ImGui::DragFloat(LOCAL_LABEL("ToolBar.UIScale").c_str(), &m_UIScale,
			0.01f, EditorPreferences::MinUIScale, EditorPreferences::MaxUIScale,
			"%.2fx", ImGuiSliderFlags_AlwaysClamp);

		if (ImGui::Button(LOCAL_LABEL("GraphicsSettings.ApplyUIScale").c_str()))
		{
			const auto result = EditorSettingsService::Get().SetEditorUIScale(m_UIScale);
			ImGuiManager::Get().NotifySettingsChange(result);

			if (result.Applied)
				m_UIScaleDirty = false;
		}

		ImGui::Separator();

		m_TargetFPSDirty |= ImGui::DragInt(LOCAL_LABEL("ToolBar.TargetFPS").c_str(), &m_TargetFPS,
			1.0f, 0, EditorPreferences::MaxTargetFPS, "%d", ImGuiSliderFlags_AlwaysClamp);

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", LOCAL_CSTR("ToolBar.TargetFPSHint"));

		if (ImGui::Button(LOCAL_LABEL("ToolBar.ApplyFPS").c_str()))
		{
			const auto result = EditorSettingsService::Get().SetEditorTargetFPS(m_TargetFPS);
			ImGuiManager::Get().NotifySettingsChange(result);

			if (result.Applied)
				m_TargetFPSDirty = false;
		}

		ImGui::Separator();
		DrawVSyncSettings();
		ImGui::Separator();
		DrawViewportSettings();
		ImGui::Separator();

		ImGui::BeginDisabled(settings.IsWindowChangePending() || settings.IsVSyncChangePending() || settings.IsViewportResolutionChangePending());

		if (ImGui::Button(LOCAL_LABEL("GraphicsSettings.ReloadApplied").c_str()))
		{
			ClearDraftChanges();
			RefreshDrafts();
		}

		ImGui::EndDisabled();
	}

	ImGui::End();
}

void GraphicSettingPanel::DrawVSyncSettings()
{
	auto& settings = EditorSettingsService::Get();
	const auto actual = settings.GetEditorVSync();
	const bool blocked = !actual || settings.IsWindowChangePending() || settings.IsVSyncChangePending() || settings.IsViewportResolutionChangePending();

	ImGui::BeginDisabled(blocked);

	m_VSyncDirty |= ImGui::Checkbox(LOCAL_LABEL("GraphicsSettings.VSync").c_str(), &m_VSync);

	const bool supported = actual && (*actual == m_VSync || settings.SupportsEditorVSync(m_VSync));

	ImGui::BeginDisabled(!supported);

	if (ImGui::Button(LOCAL_LABEL("GraphicsSettings.ApplyVSync").c_str()))
	{
		const auto result = settings.RequestVSyncChange(m_VSync);

		if (result != EResult::Success)
		{
			const char* key = result == EResult::NotImplemented
				? "GraphicsSettings.VSyncUnsupported" : "GraphicsSettings.VSyncRequestRejected";

			NotificationManager::Get().AddNotification(LOCAL("GraphicsSettings.Title"), LOCAL(key), ELogLevel::Warn);
		}
	}

	ImGui::EndDisabled();
	ImGui::EndDisabled();

	ImGui::TextWrapped("%s", LOCAL_CSTR("GraphicsSettings.VSyncHint"));

	if (actual && !supported)
		ImGui::TextWrapped("%s", LOCAL_CSTR("GraphicsSettings.VSyncUnsupported"));

	if (actual && *actual != settings.GetPreferences().VSync)
		ImGui::TextWrapped("%s", LOCAL_CSTR("GraphicsSettings.VSyncPreferenceMismatch"));

	if (settings.IsVSyncChangePending())
		ImGui::TextUnformatted(LOCAL_CSTR("GraphicsSettings.VSyncPending"));
}

void GraphicSettingPanel::DrawWindowConfirmation()
{
	auto& settings = EditorSettingsService::Get();
	const auto title = LOCAL("EditorSettings.WindowConfirmTitle") + "###EditorWindowConfirmation";

	if (settings.IsAwaitingWindowConfirmation() && !ImGui::IsPopupOpen(title.c_str()))
		ImGui::OpenPopup(title.c_str());

	const auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

	if (!ImGui::BeginPopupModal(title.c_str(), nullptr, flags))
		return;

	if (!settings.IsAwaitingWindowConfirmation())
	{
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	ImGui::TextWrapped("%s",
		LOCAL_FMT("EditorSettings.WindowConfirmMessage", settings.GetWindowConfirmationSeconds()).c_str());

	if (ImGui::Button(LOCAL_LABEL("EditorSettings.KeepWindowChange").c_str()))
	{
		const auto result = settings.ConfirmWindowChange();

		if (result == EResult::Success || !settings.IsAwaitingWindowConfirmation())
			ImGui::CloseCurrentPopup();
	}

	ImGui::SameLine();

	if (ImGui::Button(LOCAL_LABEL("EditorCommon.Cancel").c_str()))
	{
		if (settings.CancelWindowChange() == EResult::Success)
			ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void GraphicSettingPanel::DrawViewportSettings()
{
	auto& settings = EditorSettingsService::Get();
	auto states = settings.GetViewportResolutionStates();

	std::erase_if(m_ViewportDrafts, [&](const auto& entry)
		{
			return std::none_of(states.begin(), states.end(), [&](const auto& snapshot)
				{
					return snapshot.Name == entry.first;
				});
		});

	ImGui::PushID("ViewportResolutionSettings");
	ImGui::TextUnformatted(LOCAL_CSTR("GraphicsSettings.ViewportTitle"));

	if (states.empty())
	{
		m_SelectedViewportName.clear();
		ImGui::TextWrapped("%s", LOCAL_CSTR("GraphicsSettings.NoViewport"));
		ImGui::PopID();
		return;
	}

	auto selected = std::find_if(states.begin(), states.end(), [this](const auto& snapshot)
		{
			return snapshot.Name == m_SelectedViewportName;
		});

	if (selected == states.end())
		selected = states.begin();

	m_SelectedViewportName = selected->Name;

	const bool blocked = settings.IsWindowChangePending() || settings.IsVSyncChangePending() ||
		settings.IsViewportResolutionChangePending();

	ImGui::BeginDisabled(blocked);

	if (ImGui::BeginCombo(LOCAL_LABEL("GraphicsSettings.TargetViewport").c_str(),
		selected->DisplayName.c_str()))
	{
		for (auto current = states.begin(); current != states.end(); ++current)
		{
			const bool isSelected = current->Name == m_SelectedViewportName;
			const auto label = current->DisplayName + "###ResolutionTarget." + WStrToStr(current->Name);

			if (ImGui::Selectable(label.c_str(), isSelected))
			{
				m_SelectedViewportName = current->Name;
				selected = current;
			}

			if (isSelected)
				ImGui::SetItemDefaultFocus();
		}

		ImGui::EndCombo();
	}

	const auto& actual = selected->State;
	const auto found = m_ViewportDrafts.find(selected->Name);
	auto draft = found != m_ViewportDrafts.end() ? found->second : actual;
	bool changed = false;

	static constexpr const char* modeKeys[] = {
		"Viewport.FreePanelSize", "GraphicsSettings.ViewportPreset", "GraphicsSettings.ViewportCustom"
	};
	const std::array<std::string, 7> aspects = {
		LOCAL("Viewport.FreeAspect"), "16:9", "16:10", "4:3", "21:9", "1:1", "9:16"
	};

	ImGui::TextDisabled("%s", LOCAL_CSTR("GraphicsSettings.ViewportApplied"));

	const auto actualMode = glm::clamp(actual.ResolutionMode, 0, 2);
	ImGui::TextUnformatted(LOCAL_FMT("GraphicsSettings.ViewportModeValue", LOCAL(modeKeys[actualMode])).c_str());

	if (actualMode != static_cast<int32>(EViewportResolutionMode::Free))
	{
		const bool preset = actualMode == static_cast<int32>(EViewportResolutionMode::Preset);
		const auto width = preset ? actual.PresetWidth : actual.CustomWidth;
		const auto height = preset ? actual.PresetHeight : actual.CustomHeight;
		ImGui::TextUnformatted(LOCAL_FMT("GraphicsSettings.ViewportReferenceSize", width, height).c_str());
	}

	const auto actualAspect = glm::clamp(actual.AspectRatioMode, 0, 6);
	ImGui::TextUnformatted(LOCAL_FMT("GraphicsSettings.ViewportAspectValue", aspects[actualAspect]).c_str());

	ImGui::Separator();
	ImGui::TextDisabled("%s", LOCAL_CSTR("GraphicsSettings.ViewportDraft"));

	const auto draftMode = glm::clamp(draft.ResolutionMode, 0, 2);

	if (ImGui::BeginCombo(LOCAL_LABEL("GraphicsSettings.ViewportMode").c_str(), LOCAL_CSTR(modeKeys[draftMode])))
	{
		for (int32 index = 0; index < 3; ++index)
		{
			if (ImGui::Selectable(LOCAL_LABEL(modeKeys[index]).c_str(), draft.ResolutionMode == index))
			{
				draft.ResolutionMode = index;
				changed = true;
			}
		}

		ImGui::EndCombo();
	}

	if (draft.ResolutionMode == static_cast<int32>(EViewportResolutionMode::Preset))
	{
		std::string preview = LOCAL("GraphicsSettings.ViewportPreset");

		for (const auto& preset : g_ResolutionPresets)
		{
			if (draft.PresetWidth == static_cast<int32>(preset.Width) &&
				draft.PresetHeight == static_cast<int32>(preset.Height))
			{
				preview = LOCAL(preset.NameKey);
				break;
			}
		}

		if (ImGui::BeginCombo(LOCAL_LABEL("GraphicsSettings.ViewportPreset").c_str(), preview.c_str()))
		{
			for (const auto& preset : g_ResolutionPresets)
			{
				const bool isSelected = draft.PresetWidth == static_cast<int32>(preset.Width) &&
					draft.PresetHeight == static_cast<int32>(preset.Height);

				if (ImGui::Selectable(LOCAL_LABEL(preset.NameKey).c_str(), isSelected))
				{
					draft.PresetWidth = static_cast<int32>(preset.Width);
					draft.PresetHeight = static_cast<int32>(preset.Height);
					changed = true;
				}
			}

			ImGui::EndCombo();
		}
	}
	else if (draft.ResolutionMode == static_cast<int32>(EViewportResolutionMode::Custom))
	{
		bool customChanged = ImGui::InputInt(LOCAL_LABEL("EditorCommon.Width").c_str(), &draft.CustomWidth);
		customChanged |= ImGui::InputInt(LOCAL_LABEL("EditorCommon.Height").c_str(), &draft.CustomHeight);

		if (customChanged)
		{
			draft.CustomWidth = glm::clamp(draft.CustomWidth,
				BaseViewportPanel::MinResolutionDimension, BaseViewportPanel::MaxResolutionWidth);
			draft.CustomHeight = glm::clamp(draft.CustomHeight,
				BaseViewportPanel::MinResolutionDimension, BaseViewportPanel::MaxResolutionHeight);
			changed = true;
		}
	}

	const auto draftAspect = glm::clamp(draft.AspectRatioMode, 0, 6);

	if (ImGui::BeginCombo(LOCAL_LABEL("Viewport.AspectRatio").c_str(), aspects[draftAspect].c_str()))
	{
		for (int32 index = 0; index < static_cast<int32>(aspects.size()); ++index)
		{
			const auto label = aspects[index] + "###ViewportAspect." + std::to_string(index);

			if (ImGui::Selectable(label.c_str(), draft.AspectRatioMode == index))
			{
				draft.AspectRatioMode = index;
				changed = true;
			}
		}

		ImGui::EndCombo();
	}

	if (changed)
		m_ViewportDrafts.insert_or_assign(selected->Name, draft);

	if (ImGui::Button(LOCAL_LABEL("GraphicsSettings.ApplyViewport").c_str()))
	{
		if (settings.RequestViewportResolutionChange(selected->Name, draft) != EResult::Success)
		{
			NotificationManager::Get().AddNotification(LOCAL("GraphicsSettings.Title"),
				LOCAL("GraphicsSettings.ViewportRequestRejected"), ELogLevel::Warn);
		}
	}

	if (ImGui::Button(LOCAL_LABEL("GraphicsSettings.ReloadViewport").c_str()))
		m_ViewportDrafts.erase(selected->Name);

	ImGui::EndDisabled();

	if (settings.IsViewportResolutionChangePending())
		ImGui::TextUnformatted(LOCAL_CSTR("GraphicsSettings.ViewportPending"));

	ImGui::PopID();
}

void GraphicSettingPanel::DrawSettingsStorageStatus()
{
	const auto& settings = EditorSettingsService::Get();
	const bool preferencesBlocked = settings.IsPreferencesSaveBlocked();
	const bool workspaceBlocked = settings.IsWorkspaceSaveBlocked();

	if (!preferencesBlocked && !workspaceBlocked)
		return;

	const auto drawStatus = [](const char* messageKey, const std::filesystem::path& path)
		{
			ImGui::TextWrapped("%s", LOCAL_CSTR(messageKey));

			std::string pathText = LOCAL("EditorSettings.PathUnavailable");

			if (!path.empty())
			{
				const auto utf8 = path.generic_u8string();
				pathText.assign(utf8.begin(), utf8.end());
			}

			ImGui::TextWrapped("%s", LOCAL_FMT("EditorSettings.SettingsFilePath", pathText).c_str());
		};

	if (preferencesBlocked)
		drawStatus("EditorSettings.PreferencesSaveBlocked", settings.GetPreferencesPath());

	if (workspaceBlocked)
		drawStatus("EditorSettings.WorkspaceSaveBlocked", settings.GetWorkspacePath());

	ImGui::TextWrapped("%s", LOCAL_CSTR("EditorSettings.SettingsRecoveryHint"));
	ImGui::Separator();
}