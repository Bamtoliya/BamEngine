#pragma once

#include "imgui.h"
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


void ToolBar::Draw(SDL_Window* window)
{
	if (ImGui::BeginMainMenuBar())
	{
		ImGui::SetCursorPos(ImVec2(8.0f, ImGui::GetCursorPosY())); // 좌측 여백
		ImGui::Text(ICON_FA_CUBES " BamEngine");
		ImGui::SameLine(0.0f, 15.0f); // 엔진 이름과 File 메뉴 사이 간격 띄우기

		DrawFileMenu();
		DrawEditMenu();
		DrawSceneMenu();
		DrawWindowMenu();
		DrawHelpMenu();

		DrawWindowControls(window);

		ImGui::EndMainMenuBar();
	}

	const ImGuiViewport* vp = ImGui::GetMainViewport();
	const float playBarH = GetPlayBarHeight();
	const float footerBarH = GetFooterBarHeight();

	ImGuiWindowFlags barFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));

	// 1. 상단 Play Bar
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y));
	ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, playBarH));
	ImGui::SetNextWindowViewport(vp->ID);
	if (ImGui::Begin("##PlayBarRow", nullptr, barFlags))
	{
		DrawPlayControls();
	}
	ImGui::End();
	ImGui::PopStyleVar();

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
}


#pragma region FileMenu
void ToolBar::DrawFileMenu()
{
	if (ImGui::BeginMenu(LOCAL_CSTR("UI_FILE")))
	{
		ACTION_MENU_ITEM(LOCAL_CSTR("UI_EXIT"), "File.Exit");
		if (false)
		{
			SDL_Event quit_event;
			quit_event.type = SDL_EVENT_QUIT;
			SDL_PushEvent(&quit_event);
		}
		ACTION_MENU_ITEM(LOCAL_CSTR("UI_IMPORT_ASSET"), "File.ImportAsset");
		if (false)
		{
			ImportAsset();
		}
		if (ImGui::MenuItem(LOCAL_CSTR("UI_LOAD_RESOURCE"), ""))
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
		}, L"Resources", EFileDialogContext::ResourceOpen))
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
	if (ImGui::BeginMenu(LOCAL_CSTR("UI_EDIT")))
	{
		ACTION_MENU_ITEM(LOCAL_CSTR("UI_UNDO"), "Edit.Undo");
		ACTION_MENU_ITEM(LOCAL_CSTR("UI_REDO"), "Edit.Redo");
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
	if (ImGui::BeginMenu(LOCAL_CSTR("UI_SCENE")))
	{
		ACTION_MENU_ITEM(LOCAL_CSTR("UI_NEW_SCENE"), "File.NewScene");
		ACTION_MENU_ITEM(LOCAL_CSTR("UI_SAVE_SCENE"), "File.SaveScene");
		auto* document = Application::Get().GetSceneDocument();
		const bool canSave = PIEManager::Get().IsEditing() && document && document->CanSave();

		ACTION_MENU_ITEM(LOCAL_LABEL("EditorDocument.SaveAs").c_str(), "File.SaveSceneAs");
		if (false)
			SaveScene(true);

		if (document && document->HasUnsavedChanges())
			ImGui::TextUnformatted(LOCAL_CSTR("EditorDocument.Modified"));
		ACTION_MENU_ITEM(LOCAL_CSTR("UI_LOAD_SCENE"), "File.LoadScene");
		ImGui::Separator();
		ACTION_MENU_ITEM(LOCAL_CSTR("UI_CLOSE_SCENE"), "File.CloseScene");
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
		L"Resources\\Scenes", EFileDialogContext::SceneOpen))
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

	if (ImGui::BeginMenu(LOCAL_CSTR("UI_WINDOW")))
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
			for (auto* panel : gui.GetImGuiPanels())
			{
				bool open = panel->IsOpen();

				ImGui::PushID(panel);
				const string label = panel->GetDisplayName() + "###Panel";

				if (ImGui::MenuItem(label.c_str(), nullptr, &open))
					panel->SetOpen(open);

				ImGui::PopID();
			}

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
		if (ImGui::Button(LOCAL_LABEL("EditorCommon.Create").c_str()))
		{
			m_newViewportDesc.Name = StrToWStr(m_NewViewportNameBuf);
			BaseViewportPanel* newPanel = new BaseViewportPanel();
			newPanel->Initialize(&m_newViewportDesc);
			ImGuiManager::Get().AddImGuiPanel(newPanel);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button(LOCAL_LABEL("EditorCommon.Cancel").c_str()))
		{
			ImGui::CloseCurrentPopup();
		}
		
		ImGui::EndPopup();
	}
}

#pragma endregion

#pragma region HelpMenu
void ToolBar::DrawHelpMenu()
{
	if (ImGui::BeginMenu(LOCAL_LABEL("UI_HELP").c_str()))
	{
		if (ImGui::BeginMenu(LOCAL_LABEL("UI_LANGUAGE").c_str()))
		{
			const auto current = LocalizationManager::Get().GetCurrentLanguage();

			if (ImGui::MenuItem(LOCAL_LABEL("UI_ENGLISH").c_str(), nullptr, current == ELocalizationLanguage::English))
			{
				const auto result = EditorSettingsService::Get().SetEditorLanguage(ELocalizationLanguage::English);
				ImGuiManager::Get().NotifySettingsChange(result);
			}

			if (ImGui::MenuItem(LOCAL_LABEL("UI_KOREAN").c_str(), nullptr, current == ELocalizationLanguage::Korean))
			{
				const auto result = EditorSettingsService::Get().SetEditorLanguage(ELocalizationLanguage::Korean);
				ImGuiManager::Get().NotifySettingsChange(result);
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
	return ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetStyle().ItemSpacing.y;
}
void ToolBar::DrawPlayControls()
{
	PIEManager& pie = PIEManager::Get();
	EPlayState state = pie.GetState();

	const float btnWidth = 60.f;
	const float spacing = ImGui::GetStyle().ItemSpacing.x;

	if (state == EPlayState::Pause)
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 windowPos = ImGui::GetWindowPos();
		ImVec2 windowSize = ImGui::GetWindowSize();
		ImU32 pauseColor = ImGui::GetColorU32(ImVec4(1.0f, 0.8f, 0.2f, 0.3f));
		drawList->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y), pauseColor);
	}

	float totalWidth = btnWidth * 3.0f + spacing * 2.0f;
	float startX = (ImGui::GetWindowWidth() - totalWidth) * 0.5f;
	if (startX < 0.0f) startX = 0.0f;
	ImGui::SetCursorPosX(startX);

	// ▶ Play
	ImGui::PushID("PlayBtn");
	if (state != EPlayState::Edit) ImGui::BeginDisabled();
	if (ImGui::Button(ICON_FA_PLAY, ImVec2(btnWidth, 0))) pie.StartPlay();
	if (state != EPlayState::Edit) ImGui::EndDisabled();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (%s)", LOCAL_CSTR("ToolBar.PlayTooltip"), ShortcutManager::Get().GetShortcutHint("PIE.Play"));
	ImGui::PopID();

	ImGui::SameLine();

	// ⏸ Pause / Resume
	ImGui::PushID("PauseResumeBtn");
	if (state == EPlayState::Edit) ImGui::BeginDisabled();
	if (state == EPlayState::Pause)
	{
		if (ImGui::Button(ICON_FA_PLAY, ImVec2(btnWidth, 0))) pie.Resume();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (%s)", LOCAL_CSTR("ToolBar.ResumeTooltip"), ShortcutManager::Get().GetShortcutHint("PIE.PauseResume"));
	}
	else
	{
		if (ImGui::Button(ICON_FA_PAUSE, ImVec2(btnWidth, 0))) pie.Pause();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (%s)", LOCAL_CSTR("ToolBar.PauseTooltip"), ShortcutManager::Get().GetShortcutHint("PIE.PauseResume"));
	}
	if (state == EPlayState::Edit) ImGui::EndDisabled();
	ImGui::PopID();

	ImGui::SameLine();

	// ■ Stop
	ImGui::PushID("StopBtn");
	if (state == EPlayState::Edit) ImGui::BeginDisabled();
	if (ImGui::Button(ICON_FA_STOP, ImVec2(btnWidth, 0))) pie.Stop();
	if (state == EPlayState::Edit) ImGui::EndDisabled();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (%s)", LOCAL_CSTR("ToolBar.StopTooltip"), ShortcutManager::Get().GetShortcutHint("PIE.Stop"));
	ImGui::PopID();
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
		const char* title = LOCAL_CSTR(compiling ? "ToolBar.ModuleCompiling" : "ToolBar.ModuleCompile");
		ImGui::SetTooltip("%s (%s)\n%s", title, ShortcutManager::Get().GetShortcutHint("Build.CompileModule"), LOCAL_CSTR("ToolBar.ModuleCompileTooltip"));
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
		ImGui::SetTooltip("%s (%s)\n%s", LOCAL_CSTR("ToolBar.ModuleReload"), ShortcutManager::Get().GetShortcutHint("Build.ReloadModule"),
			LOCAL_FMT("ToolBar.ReloadModuleTooltip", moduleReloader.GetReloadIndex()).c_str());
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
		ImGui::SetTooltip("%s (%s)\n%s", LOCAL_CSTR("ToolBar.ShaderReload"), ShortcutManager::Get().GetShortcutHint("Build.ReloadShaders"), LOCAL_CSTR("ToolBar.ShaderReloadTooltip"));
	}
	ImGui::PopID();
}

#pragma endregion

#pragma region Window Controls
void ToolBar::DrawWindowControls(SDL_Window* window)
{
	if (!window) return;

	// 버튼 1개의 너비와 3개의 총 너비 계산
	const float buttonWidth = 45.0f;
	const float totalButtonAreaWidth = buttonWidth * 3.0f;

	// 💡 우측 끝으로 커서 이동 (드래그 할 수 있는 빈 공간을 확보합니다)
	ImGui::SameLine(ImGui::GetWindowWidth() - totalButtonAreaWidth);

	// 타이틀바와 어울리도록 버튼 배경을 투명하게 만듭니다.
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f); // 윈도우 버튼은 각져야 예쁨

	ImVec2 btnSize(buttonWidth, ImGui::GetFrameHeight());

	// [1] 최소화 버튼 (-)
	if (ImGui::Button(ICON_FA_MINUS, btnSize))
	{
		SDL_MinimizeWindow(window);
	}
	ImGui::SameLine(0, 0); // 버튼 사이 간격 0

	// [2] 최대화 / 창 모드 버튼 (ㅁ)
	bool isMaximized = (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0;
	if (ImGui::Button(isMaximized ? ICON_FA_WINDOW_RESTORE : ICON_FA_WINDOW_MAXIMIZE, btnSize))
	{
		if (isMaximized) SDL_RestoreWindow(window);
		else SDL_MaximizeWindow(window);
	}
	ImGui::SameLine(0, 0);

	// [3] 닫기 버튼 (X) - 마우스 올렸을 때만 붉은색
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
	if (ImGui::Button(ICON_FA_XMARK, btnSize))
	{
		SDL_Event quitEvent;
		quitEvent.type = SDL_EVENT_QUIT;
		SDL_PushEvent(&quitEvent);
	}
	ImGui::PopStyleColor(2); // 닫기 버튼 색상 롤백

	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
}
#pragma endregion