#pragma once

#include "imgui.h"
#include "EditorUI.h"
#include "ToolBar.h"

#include "SceneManager.h"
#include "ShortcutManager.h"

#include "LocalizationManager.h"

#include "ImGuiPanels.h"
#include "ImGuiManager.h"

#include "Application.h"

#include <archive/JsonArchive.h>
#include "FileDialogs.h"

#include "SelectionManager.h"
#include "AssetManager.h"

#include "PIEManager.h"
#include "HotReloadManager.h"
#include "ModuleHotReloadManager.h"
#include "ShaderHotReloadManager.h"
#include "NotificationManager.h"
#include "SceneDocument.h"
#include "EditorSettingsService.h"
#include "ModuleBuildManager.h"
#include <cmath>


namespace
{
	float GetWindowControlWidth()
	{
		float width = ImGui::GetFontSize() * 2.5f;
		for (const char* icon : { ICON_FA_MINUS, ICON_FA_WINDOW_RESTORE, ICON_FA_WINDOW_MAXIMIZE, ICON_FA_XMARK })
			width = std::max(width, EditorUI::GetIconButtonSize(icon, true).x);
		return std::ceil(width);
	}

	float GetMainMenuRightEdge()
	{
		const float contentRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
		return std::floor(std::min(contentRight, ImGui::GetWindowDrawList()->GetClipRectMax().x));
	}

	void DrawActionTooltip(const char* titleKey, const char* actionId, const std::string& details = {})
	{
		std::string tooltip = LOCAL(titleKey);
		const char* hint = ShortcutManager::Get().GetShortcutHint(actionId);

		if (hint && hint[0] != '\0')
		{
			tooltip += " (";
			tooltip += hint;
			tooltip += ')';
		}

		if (!details.empty())
		{
			tooltip += '\n';
			tooltip += details;
		}

		ImGui::SetTooltip("%s", tooltip.c_str());
	}
}

float ToolBar::Draw(SDL_Window* window)
{
	m_MainMenuBarLayout = {};
	const auto framePadding = ImGui::GetStyle().FramePadding;
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(framePadding.x, std::max(framePadding.y, ImGui::GetFontSize() * 0.15f)));
	if (ImGui::BeginMainMenuBar())
	{
		const float controlWidth = GetWindowControlWidth();
		const float available = GetMainMenuRightEdge() - ImGui::GetCursorScreenPos().x;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float reserved = window ? controlWidth * 3.0f + spacing : 0.0f;
		const char* menuKeys[] = { "EditorMenu.File", "EditorMenu.Edit", "Viewport.Scene", "EditorMenu.Window", "EditorMenu.Help" };
		float menuWidth = ImGui::CalcTextSize(ICON_FA_CUBES " BamEngine").x + ImGui::GetStyle().ItemSpacing.x * 3.0f;
		for (const char* key : menuKeys)
			menuWidth += ImGui::CalcTextSize(LOCAL_CSTR(key)).x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
		const auto drawMenus = [&] { DrawFileMenu(); DrawEditMenu(); DrawSceneMenu(); DrawWindowMenu(); DrawHelpMenu(); };
		if (menuWidth + reserved <= available)
		{
			ImGui::TextUnformatted(ICON_FA_CUBES " BamEngine");
			drawMenus();
		}
		else if (available >= (window ? controlWidth : 0.0f) + ImGui::CalcTextSize(ICON_FA_BARS).x + spacing * 2.0f &&
			ImGui::BeginMenu(ICON_FA_BARS "###MainMenuOverflow"))
		{
			drawMenus();
			ImGui::EndMenu();
		}

		const ImVec2 viewportPos = ImGui::GetMainViewport()->Pos;
		const ImVec2 barPos = ImGui::GetWindowPos();
		m_MainMenuBarLayout.Top = barPos.y - viewportPos.y;
		m_MainMenuBarLayout.Bottom = m_MainMenuBarLayout.Top + ImGui::GetWindowHeight();
		m_MainMenuBarLayout.MenusEndX = ImGui::GetCursorScreenPos().x - viewportPos.x;
		m_MainMenuBarLayout.WindowButtonsBeginX = barPos.x - viewportPos.x + ImGui::GetWindowWidth();
		DrawWindowControls(window);

		ImGui::EndMainMenuBar();
	}

	ImGui::PopStyleVar();
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	const float playBarH = GetPlayBarHeight();
	const float footerBarH = GetFooterBarHeight();

	ImGuiWindowFlags barFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1.0f, 1.0f));

	// 1. 상단 Play Bar
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y));
	ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, playBarH));
	ImGui::SetNextWindowViewport(vp->ID);
	const bool playBarVisible = ImGui::Begin("##PlayBarRow", nullptr, barFlags);
	const float playBarBottom = ImGui::GetWindowPos().y + ImGui::GetWindowHeight();
	if (playBarVisible) DrawPlayControls();
	ImGui::End();
	ImGui::PopStyleVar(2);

	// 2. 하단 슬림 Footer Bar (WindowMinSize 제한 해제 및 패딩 축소)
	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 2.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 1.0f));
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - footerBarH));
	ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, footerBarH));
	ImGui::SetNextWindowViewport(vp->ID);
	if (ImGui::Begin("##FooterBarRow", nullptr, barFlags))
	{
		DrawFooterControls();
	}
	ImGui::End();
	NotificationManager::Get().DrawHistoryPopup();
	ImGui::PopStyleVar(3);
	ImGui::PopStyleVar(2);
	return playBarBottom;
}

bool ToolBar::IsMainWindowDragArea(const SDL_Point& point) const
{
	return m_MainMenuBarLayout.IsDragArea(static_cast<float>(point.x), static_cast<float>(point.y));
}


#pragma region FileMenu
void ToolBar::DrawFileMenu()
{
	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.File").c_str()))
	{
		ACTION_MENU_ITEM(LOCAL_CSTR("Action.File.Exit"), "File.Exit");
		if (false)
		{
			SDL_Event quit_event;
			quit_event.type = SDL_EVENT_QUIT;
			SDL_PushEvent(&quit_event);
		}
		ACTION_MENU_ITEM(LOCAL_CSTR("Action.File.ImportAsset"), "File.ImportAsset");
		if (false)
		{
			ImportAsset();
		}
		if (ImGui::MenuItem(LOCAL_CSTR("ResourceEditor.LoadResource"), ""))
		{
			LoadResourceFile();
		}

		auto& sm = ShortcutManager::Get();
		if (ImGui::MenuItem(LOCAL_LABEL("Settings.Title").c_str(), sm.GetShortcutHint("Editor.OpenSettings")))
		{
			if (auto* panel = dynamic_cast<UnifiedSettingsPanel*>(ImGuiManager::Get().GetImGuiPanel(L"UnifiedSettings")))
				panel->OpenWithCategory(EUnifiedSettingsCategory::General);
		}
		ImGui::EndMenu();
	}
}

void ToolBar::ImportAsset()
{
	wstring filePath;
	if (!FileDialogs::OpenFileDialog(filePath, {
	{ StrToWStr(LOCAL("FileDialog.AssetFiles")), L"*.*" }
		}, L"", EFileDialogContext::ResourceImport))
		return;

	const filesystem::path sourcePath(filePath);
	wstring extension = sourcePath.extension().wstring();
	for (wchar_t& character : extension)
		character = static_cast<wchar_t>(std::towlower(character));

	const bool isModel = extension == L".fbx" || extension == L".obj" || extension == L".gltf" || extension == L".glb";
	const filesystem::path destination = isModel ? sourcePath.parent_path() : sourcePath;

	if (isModel)
	{
		AssetManager::Get().ImportAsync(sourcePath, destination);
		return;
	}

	const EResult result = AssetManager::Get().Import(sourcePath, destination);
	if (result != EResult::Success)
	{
		BAM_LOG(Error, "Editor", "Asset", "Toolbar import failed: source={}, result={}",
			sourcePath.string(), ResultToString(result));
		return;
	}

	if (auto* panel = dynamic_cast<ContentBrowserPanel*>(ImGuiManager::Get().GetImGuiPanel(L"Content Browser")))
		panel->RequestRefresh();
}

void ToolBar::LoadResourceFile()
{
	wstring filePath;
	if (!FileDialogs::OpenFileDialog(filePath, {
	{ StrToWStr(LOCAL("FileDialog.ResourceFiles")), L"*.bam*;*.json;*.beve" }
		}, L"", EFileDialogContext::ResourceOpen))
		return;

	auto& manager = ResourceManager::Get();
	const auto handle = manager.LoadFile(filePath);

	if (!manager.IsValid(handle))
	{
		BAM_LOG(Error, "Editor", "Asset", "Toolbar resource load failed: path={}", WStrToStr(filePath));
		NotificationManager::Get().AddNotification(LOCAL("Notification.LoadResourceFailed"),
			LOCAL_FMT("Notification.ResourceLoadFailed", WStrToStr(filePath)), ELogLevel::Error, 3.0f);
		return;
	}

	BAM_LOG(Info, "Editor", "Asset", "Toolbar resource loaded: path={}", WStrToStr(filePath));
	NotificationManager::Get().AddNotification(LOCAL("Notification.LoadResourceSucceeded"),
		LOCAL_FMT("Notification.ResourceLoadSucceeded", WStrToStr(filePath)), ELogLevel::Info, 3.0f);
}
#pragma endregion

#pragma region EditMenu
void ToolBar::DrawEditMenu()
{
	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.Edit").c_str()))
	{
		ACTION_MENU_ITEM(LOCAL_CSTR("EditorCommon.Undo"), "Edit.Undo");
		ACTION_MENU_ITEM(LOCAL_CSTR("EditorCommon.Redo"), "Edit.Redo");
		ImGui::Separator();
		ACTION_MENU_ITEM(LOCAL_CSTR("EditorCommon.Copy"), "Edit.Copy");
		ACTION_MENU_ITEM(LOCAL_CSTR("EditorCommon.Paste"), "Edit.Paste");
		ACTION_MENU_ITEM(LOCAL_CSTR("Hierarchy.Duplicate"), "Edit.Duplicate");
		ACTION_MENU_ITEM(LOCAL_CSTR("EditorCommon.Delete"), "Edit.Delete");
		ImGui::Separator();
		if (ImGui::MenuItem(LOCAL_LABEL("Settings.CategoryShortcuts").c_str()))
		{
			if (auto* panel = dynamic_cast<UnifiedSettingsPanel*>(ImGuiManager::Get().GetImGuiPanel(L"UnifiedSettings")))
				panel->OpenWithCategory(EUnifiedSettingsCategory::Shortcuts);
		}
		ImGui::EndMenu();
	}
}
#pragma endregion

#pragma region SceneMenu
void ToolBar::DrawSceneMenu()
{
	if (ImGui::BeginMenu(LOCAL_LABEL("Viewport.Scene").c_str()))
	{
		ACTION_MENU_ITEM(LOCAL_CSTR("Action.File.NewScene"), "File.NewScene");
		ACTION_MENU_ITEM(LOCAL_CSTR("Action.File.SaveScene"), "File.SaveScene");
		auto* document = Application::Get().GetSceneDocument();
		const bool canSave = PIEManager::Get().IsEditing() && document && document->CanSave();

		ACTION_MENU_ITEM(LOCAL_LABEL("EditorDocument.SaveAs").c_str(), "File.SaveSceneAs");
		if (false)
			SaveScene(true);

		if (document && document->HasUnsavedChanges())
			ImGui::TextUnformatted(LOCAL_CSTR("EditorDocument.Modified"));
		ACTION_MENU_ITEM(LOCAL_CSTR("Action.File.LoadScene"), "File.LoadScene");
		ImGui::Separator();
		ACTION_MENU_ITEM(LOCAL_CSTR("Action.File.CloseScene"), "File.CloseScene");
		if (false)
		{
			ImGuiManager::Get().RequestCloseScene();
		}
		ImGui::EndMenu();
	}
}
void ToolBar::NewScene()
{
	ImGuiManager::Get().RequestNewScene();
}

void ToolBar::SaveScene(bool saveAs)
{
	if (!PIEManager::Get().IsEditing())
	{
		NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
			LOCAL("EditorDocument.SaveUnavailable"), ELogLevel::Warn);
		return;
	}

	auto& controller = ImGuiManager::Get().GetDocumentController();

	if (controller.IsBusy())
	{
		NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
			LOCAL("EditorDocument.OperationBusy"), ELogLevel::Warn);
		return;
	}

	const auto id = Application::Get().GetSceneDocumentId();

	if (!controller.RequestSave(id, saveAs))
	{
		NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
			LOCAL("EditorDocument.SaveUnavailable"), ELogLevel::Warn);
	}
}
void ToolBar::LoadScene()
{
	auto& gui = ImGuiManager::Get();

	if (!PIEManager::Get().IsEditing())
	{
		NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
			LOCAL("EditorDocument.SceneActionUnavailable"), ELogLevel::Warn);
		return;
	}

	if (gui.GetDocumentController().IsBusy())
	{
		NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
			LOCAL("EditorDocument.OperationBusy"), ELogLevel::Warn);
		return;
	}

	std::wstring filePath;
	if (!FileDialogs::OpenFileDialog(filePath,
		{ { StrToWStr(LOCAL("FileDialog.SceneFiles")), L"*.bamscene.json;*.json" } },
		L"", EFileDialogContext::SceneOpen))
	{
		return;
	}

	gui.RequestOpenScene(std::filesystem::path(filePath));
}
#pragma endregion

#pragma region WindowMenu

void ToolBar::DrawWindowMenu()
{
	auto& gui = ImGuiManager::Get();

	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.Window").c_str()))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("Settings.CategoryGraphic").c_str()))
		{
			if (auto* panel = dynamic_cast<UnifiedSettingsPanel*>(ImGuiManager::Get().GetImGuiPanel(L"UnifiedSettings")))
				panel->OpenWithCategory(EUnifiedSettingsCategory::Graphic);
		}

		if (ImGui::MenuItem(LOCAL_LABEL("ToolBar.NewViewportPanel").c_str()))
			AddNewViewportPanel();

		ImGui::Separator();

		if (ImGui::BeginMenu(LOCAL_LABEL("ToolBar.Panels").c_str()))
		{
			const auto drawCategory = [&gui](EPanelMenuCategory category, const string& label)
				{
					if (!ImGui::BeginMenu(label.c_str()))
						return;

					for (auto* panel : gui.GetImGuiPanels())
					{
						if (panel->GetPanelMenuCategory() != category)
							continue;

						bool open = panel->IsOpen();
						ImGui::PushID(panel);
						const string panelLabel = panel->GetDisplayName() + "###Panel";
						if (ImGui::MenuItem(panelLabel.c_str(), nullptr, &open))
							panel->SetOpen(open);
						ImGui::PopID();
					}

					ImGui::EndMenu();
				};

			drawCategory(EPanelMenuCategory::Viewport, LOCAL_LABEL("EditorMenu.PanelViewports"));
			drawCategory(EPanelMenuCategory::SceneEditing, LOCAL_LABEL("EditorMenu.PanelSceneEditing"));
			drawCategory(EPanelMenuCategory::Assets, LOCAL_LABEL("EditorMenu.PanelAssets"));
			drawCategory(EPanelMenuCategory::GraphAnimation, LOCAL_LABEL("EditorMenu.PanelGraphAnimation"));
			drawCategory(EPanelMenuCategory::Diagnostics, LOCAL_LABEL("EditorMenu.PanelDiagnostics"));
			drawCategory(EPanelMenuCategory::Other, LOCAL_LABEL("EditorCommon.Other"));

			ImGui::EndMenu();
		}



		ImGui::EndMenu();
	}

	DrawNewViewportPopup();
}
void ToolBar::AddNewViewportPanel()
{
	m_newViewportDesc = ViewportPanelDesc();
	strcpy_s(m_NewViewportNameBuf, LOCAL_CSTR("Viewport.NewName"));
	m_ShowNewViewportPopup = true;
}
void ToolBar::DrawNewViewportPopup()
{
	if (m_ShowNewViewportPopup)
	{
		ImGui::OpenPopup("NewViewportPopup");
		m_ShowNewViewportPopup = false;
	}
	if (ImGui::BeginPopup("NewViewportPopup"))
	{
		ImGui::InputText(LOCAL_LABEL("EditorCommon.Name").c_str(), m_NewViewportNameBuf, sizeof(m_NewViewportNameBuf));
		ImGui::InputInt(LOCAL_LABEL("EditorCommon.Width").c_str(), (int*)&m_newViewportDesc.RenderTargetWidth);
		ImGui::InputInt(LOCAL_LABEL("EditorCommon.Height").c_str(), (int*)&m_newViewportDesc.RenderTargetHeight);
		const char* cameraTypes[] = { "Perspective", "Orthographic" };
		//int currentCameraType = (int)m_newViewportDesc.CameraType;
		//if (ImGui::Combo("Camera Type", &currentCameraType, cameraTypes, IM_ARRAYSIZE(cameraTypes)))
		//{
		//	m_newViewportDesc.CameraType = (EViewportCameraType)currentCameraType;
		//}
		//const char* viewportModes[] = { "Wireframe", "Solid", "Textured" };
		//int currentViewportMode = (int)m_newViewportDesc.ViewportMode;
		//if (ImGui::Combo("Viewport Mode", &currentViewportMode, viewportModes, IM_ARRAYSIZE(viewportModes)))
		//{
		//	m_newViewportDesc.ViewportMode = (EViewportMode)currentViewportMode;
		//}
		const EditorUI::DialogAction actions[] = { { "EditorCommon.Cancel" }, { "EditorCommon.Create" } };
		const int action = EditorUI::DialogButtonBar("CreateViewportActions", actions, IM_ARRAYSIZE(actions));
		if (action == 1)
		{
			m_newViewportDesc.Name = StrToWStr(m_NewViewportNameBuf);
			auto* panel = new BaseViewportPanel();
			if (const EResult result = panel->Initialize(&m_newViewportDesc); result == EResult::Success)
				ImGui::CloseCurrentPopup();
			else
			{
				Safe_Release(panel);
				BAM_LOG(Error, "Editor", "UI", "Viewport initialization failed: {}", ResultToString(result));
			}
		}
		if (action == 0)
		{
			ImGui::CloseCurrentPopup();
		}
		
		ImGui::EndPopup();
	}
}

#pragma endregion

#pragma region HelpMenu
void ToolBar::ApplyPendingLanguageChange()
{
	if (!m_PendingLanguage) return;
	const auto language = *m_PendingLanguage;
	m_PendingLanguage.reset();
	if (language == LocalizationManager::Get().GetCurrentLanguage()) return;
	const auto result = EditorSettingsService::Get().SetEditorLanguage(language);
	ImGuiManager::Get().NotifySettingsChange(result);
}

void ToolBar::DrawHelpMenu()
{
	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.Help").c_str()))
	{
		if (ImGui::BeginMenu(LOCAL_LABEL("Settings.Language").c_str()))
		{
			const auto current = LocalizationManager::Get().GetCurrentLanguage();

			if (ImGui::MenuItem(LOCAL_LABEL("Settings.LanguageEnglish").c_str(), nullptr, current == ELocalizationLanguage::English))
			{
				if (current != ELocalizationLanguage::English)
					m_PendingLanguage = ELocalizationLanguage::English;
			}

			if (ImGui::MenuItem(LOCAL_LABEL("Settings.LanguageKorean").c_str(), nullptr, current == ELocalizationLanguage::Korean))
			{
				if (current != ELocalizationLanguage::Korean)
					m_PendingLanguage = ELocalizationLanguage::Korean;
			}

			ImGui::EndMenu();
		}

		ImGui::EndMenu();
	}
}

#pragma endregion

#pragma region Play Controls
float ToolBar::GetPlayBarHeight() const
{
	const auto size = EditorUI::GetIconButtonSize(ICON_FA_PLAY);
	const auto grid = EditorUI::MeasureIconGrid(size, 3, ImGui::GetMainViewport()->WorkSize.x - 16.0f);
	return std::ceil(grid.y + 8.0f);
}
void ToolBar::DrawPlayControls()
{
	PIEManager& pie = PIEManager::Get();
	EPlayState state = pie.GetState();

	const auto buttonSize = EditorUI::GetIconButtonSize(ICON_FA_PLAY);

	if (state == EPlayState::Pause)
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 windowPos = ImGui::GetWindowPos();
		ImVec2 windowSize = ImGui::GetWindowSize();
		ImU32 pauseColor = ImGui::GetColorU32(ImVec4(1.0f, 0.8f, 0.2f, 0.3f));
		drawList->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), pauseColor);
	}

	const auto grid = EditorUI::MeasureIconGrid(buttonSize, 3, ImGui::GetContentRegionAvail().x);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (ImGui::GetContentRegionAvail().x - grid.x) * 0.5f));
	EditorUI::ToolbarLayout toolbar;
	EditorUI::IconButtonOptions options;
	options.Size = buttonSize;
	options.Enabled = state == EPlayState::Edit;
	options.ShortcutHint = ShortcutManager::Get().GetShortcutHint("PIE.Play");
	if (toolbar.Icon("PlayBtn", ICON_FA_PLAY, "ToolBar.PlayTooltip", options)) pie.StartPlay();
	options.Enabled = state != EPlayState::Edit;
	options.ShortcutHint = ShortcutManager::Get().GetShortcutHint("PIE.PauseResume");
	if (toolbar.Icon("PauseResumeBtn", state == EPlayState::Pause ? ICON_FA_PLAY : ICON_FA_PAUSE,
		state == EPlayState::Pause ? "ToolBar.ResumeTooltip" : "ToolBar.PauseTooltip", options))
	{
		if (state == EPlayState::Pause) pie.Resume();
		else pie.Pause();
	}
	options.ShortcutHint = ShortcutManager::Get().GetShortcutHint("PIE.Stop");
	if (toolbar.Icon("StopBtn", ICON_FA_STOP, "ToolBar.StopTooltip", options)) pie.Stop();
}
#pragma endregion

#pragma region Footer
float ToolBar::GetFooterBarHeight() const
{
	return ImGui::GetFontSize() + 6.0f;
}
void ToolBar::DrawFooterControls()
{
	auto& buildManager = HotReloadManager::Get().GetModuleBuilder();
	auto& moduleReloader = HotReloadManager::Get().GetModuleReloader();
	auto& shaderReloader = HotReloadManager::Get().GetShaderReloader();
	// ── 1. 좌측: 백그라운드 비동기 작업 진행률 표시 (화면 중앙 대신 하단 푸터에 얌전하게 표시) ──
	size_t activeTasks = AssetManager::IsCreated() ? AssetManager::Get().GetActiveTaskCount() : 0;
	if (activeTasks > 0)
	{
		ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x);
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s",
			(ICON_FA_ARROW_ROTATE_RIGHT " " + LOCAL_FMT("ContentBrowser.Importing", (int)activeTasks)).c_str());
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", LOCAL_FMT("ContentBrowser.Importing", (int)activeTasks).c_str());
		}
	}
	// ── 2. 우측 컨트롤 (알림 벨 버튼 + 빌드/핫리로드 제어 버튼들) ──
	const float btnHeight = ImGui::GetFontSize() + 2.0f;
	const float btnWidth = btnHeight * 1.25f;
	const ImVec2 btnSize(btnWidth, btnHeight);
	const float pairGap = 3.0f;
	const float groupGap = 10.0f;
	// 버튼 6개 (알림 벨 + 컴파일 2개 + 핫리로드 3개)의 전체 너비 계산
	const float totalWidth = btnWidth * 6.0f + pairGap * 2.0f + groupGap * 3.0f;
	const float startX = std::max(ImGui::GetStyle().WindowPadding.x, ImGui::GetWindowWidth() - totalWidth - ImGui::GetStyle().WindowPadding.x);
	ImGui::SetCursorPosX(startX);
	// [신규] 0. 지나간 알림 히스토리 토글 벨 버튼
	auto& notifMgr = NotificationManager::Get();
	const bool historyOpen = notifMgr.IsHistoryPopupOpen();
	const size_t notifCount = notifMgr.GetHistoryCount();
	if (historyOpen)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.85f, 1.0f));
	else if (notifCount > 0)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.28f, 0.9f));
	else
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.18f, 0.20f, 0.85f));
	if (ImGui::Button(ICON_FA_BELL, btnSize))
	{
		notifMgr.ToggleHistoryPopup();
	}
	ImGui::PopStyleColor();
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s (%d)", LOCAL_CSTR("Notification.HistoryTooltip"), static_cast<int>(notifCount));
	}
	ImGui::SameLine(0.0f, groupGap);

	auto drawToggleIconBtn = [&](const char* id, const char* icon, bool& state) -> bool
		{
			ImGui::PushID(id);
			if (state)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.48f, 0.82f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.06f, 0.53f, 0.98f, 1.0f));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.18f, 0.20f, 0.85f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.28f, 0.33f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.24f, 0.29f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			}

			const bool clicked = ImGui::Button(icon, btnSize);
			ImGui::PopStyleColor(state ? 3 : 4);
			if (clicked) state = !state;
			ImGui::PopID();
			return clicked;
		};

	// 1. C++ GameModule Compile
	const bool compiling = buildManager.IsCompiling();
	ImGui::PushID("CompileModuleBtn");
	if (compiling) ImGui::BeginDisabled();
	if (ImGui::Button(compiling ? ICON_FA_SPINNER : ICON_FA_HAMMER, btnSize))
		buildManager.RequestBuild();
	if (compiling) ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
	{
		DrawActionTooltip(compiling ? "ToolBar.ModuleCompiling" : "ToolBar.ModuleCompile", "Build.CompileModule", LOCAL("ToolBar.ModuleCompileTooltip"));
	}
	ImGui::PopID();

	ImGui::SameLine(0.0f, pairGap);

	// 2. Auto-Compile Toggle
	bool autoCompile = buildManager.IsAutoCompileEnabled();
	if (drawToggleIconBtn("AutoCompileBtn", ICON_FA_BOLT, autoCompile))
		buildManager.SetAutoCompileEnabled(autoCompile);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s: %s\n%s", LOCAL_CSTR("ToolBar.AutoCompile"), LOCAL_CSTR(autoCompile ? "EditorCommon.On" : "EditorCommon.Off"), LOCAL_CSTR("ToolBar.AutoCompileTooltip"));
	}

	ImGui::SameLine(0.0f, groupGap);

	// 3. Hot Reload Module
	ImGui::PushID("HotReloadBtn");
	if (ImGui::Button(ICON_FA_ARROWS_ROTATE, btnSize))
		moduleReloader.ReloadModule();
	if (ImGui::IsItemHovered())
	{
		DrawActionTooltip("ToolBar.ModuleReload", "Build.ReloadModule",
			LOCAL_FMT("ToolBar.ReloadModuleTooltip", moduleReloader.GetReloadIndex()));
	}
	ImGui::PopID();

	ImGui::SameLine(0.0f, pairGap);

	// 4. Auto-Reload Toggle
	bool autoReload = moduleReloader.IsAutoReloadEnabled();
	if (drawToggleIconBtn("AutoReloadBtn", ICON_FA_LINK, autoReload))
		moduleReloader.SetAutoReload(autoReload);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s: %s\n%s", LOCAL_CSTR("ToolBar.AutoReload"), LOCAL_CSTR(autoReload ? "EditorCommon.On" : "EditorCommon.Off"), LOCAL_CSTR("ToolBar.AutoReloadTooltip"));
	}

	ImGui::SameLine(0.0f, groupGap);

	// 5. Shader Reload
	ImGui::PushID("ShaderReloadBtn");
	if (ImGui::Button(ICON_FA_FIRE, btnSize))
		shaderReloader.RequestRecompileAll();
	if (ImGui::IsItemHovered())
	{
		DrawActionTooltip("ToolBar.ShaderReload", "Build.ReloadShaders", LOCAL("ToolBar.ShaderReloadTooltip"));
	}
	ImGui::PopID();
}

#pragma endregion

#pragma region Window Controls
void ToolBar::DrawWindowControls(SDL_Window* window)
{
	if (!window) return;

	const float right = GetMainMenuRightEdge();
	const float available = right - ImGui::GetCursorScreenPos().x;
	const float closeMinimum = std::ceil(EditorUI::GetIconButtonSize(ICON_FA_XMARK, true).x);
	if (available < closeMinimum) return;

	const float buttonWidth = GetWindowControlWidth();
	const float closeWidth = std::min(available, buttonWidth);
	const float closeX = right - closeWidth;
	const float moreWidth = std::ceil(ImGui::CalcTextSize(ICON_FA_WINDOW_MAXIMIZE).x + ImGui::GetStyle().ItemSpacing.x * 2.0f);
	const bool showAll = available >= buttonWidth * 3.0f;
	const bool showMore = !showAll && available >= closeWidth + moreWidth;
	const float controlsX = showAll ? right - buttonWidth * 3.0f : closeX - (showMore ? moreWidth : 0.0f);
	m_MainMenuBarLayout.WindowButtonsBeginX = controlsX - ImGui::GetMainViewport()->Pos.x;
	const auto setButtonX = [](float x)
	{
		const auto cursor = ImGui::GetCursorScreenPos();
		ImGui::SetCursorScreenPos(ImVec2(x, cursor.y));
	};

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

	const bool maximized = (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0;
	const char* maximizeKey = maximized ? "ToolBar.WindowRestore" : "ToolBar.WindowMaximize";
	const auto toggleMaximize = [&]
	{
		if (maximized) SDL_RestoreWindow(window);
		else SDL_MaximizeWindow(window);
	};

	EditorUI::IconButtonOptions options;
	options.Compact = true;
	options.Size = ImVec2(buttonWidth, ImGui::GetFrameHeight());
	if (showAll)
	{
		setButtonX(controlsX);
		if (EditorUI::IconButton("WindowMinimize", ICON_FA_MINUS, "ToolBar.WindowMinimize", options))
			SDL_MinimizeWindow(window);
		setButtonX(closeX - buttonWidth);
		const char* icon = maximized ? ICON_FA_WINDOW_RESTORE : ICON_FA_WINDOW_MAXIMIZE;
		if (EditorUI::IconButton("WindowMaximize", icon, maximizeKey, options)) toggleMaximize();
	}
	else if (showMore)
	{
		setButtonX(controlsX);
		if (ImGui::BeginMenu(ICON_FA_WINDOW_MAXIMIZE "###WindowActions"))
		{
			if (ImGui::MenuItem(LOCAL_LABEL("ToolBar.WindowMinimize").c_str())) SDL_MinimizeWindow(window);
			if (ImGui::MenuItem(LOCAL_LABEL(maximizeKey).c_str())) toggleMaximize();
			ImGui::EndMenu();
		}
	}

	setButtonX(closeX);
	options.Size.x = closeWidth;
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
	if (EditorUI::IconButton("WindowClose", ICON_FA_XMARK, "EditorCommon.Close", options))
	{
		SDL_Event quitEvent{};
		quitEvent.type = SDL_EVENT_QUIT;
		SDL_PushEvent(&quitEvent);
	}
	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar();
}
#pragma endregion
