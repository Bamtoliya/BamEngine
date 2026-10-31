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
	const float menuBarH = ImGui::GetFrameHeight();
	const float playBarH = ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;

	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y));
	ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, GetPlayBarHeight()));

	ImGuiWindowFlags flags = 0;
	flags |= ImGuiWindowFlags_NoDocking;
	flags |= ImGuiWindowFlags_NoTitleBar;
	flags |= ImGuiWindowFlags_NoResize;
	flags |= ImGuiWindowFlags_NoMove;
	flags |= ImGuiWindowFlags_NoScrollbar;
	flags |= ImGuiWindowFlags_NoSavedSettings;
	flags |= ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));

	if (ImGui::Begin("##PlayBarRow", nullptr, flags))
	{
		DrawPlayControls();
	}
	ImGui::End();

	ImGui::PopStyleVar(3);
}

float ToolBar::GetPlayBarHeight() const
{
	return ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetStyle().ItemSpacing.y;
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

		auto* settingsPanel = ImGuiManager::Get().GetImGuiPanel(L"GraphicsSettings");

		if (ImGui::MenuItem(LOCAL_LABEL("UI_SETTINGS").c_str(), nullptr, false,
			settingsPanel != nullptr) && settingsPanel)
		{
			TODO("Open overall settings panel, May this button not be here");
			settingsPanel->Open();
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

	const bool isModel = extension == L".fbx" || extension == L".obj" || extension == L".gltf";
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
		if (ImGui::MenuItem(LOCAL_CSTR("Shortcuts.Title")))
		{
			if (auto* panel = ImGuiManager::Get().GetImGuiPanel(L"ShortcutSettings"))
				panel->Open();
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
		{ { StrToWStr(LOCAL("FileDialog.SceneFiles")), L"*.json" } },
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
		auto* settingsPanel = gui.GetImGuiPanel(L"GraphicsSettings");
		const bool settingsOpen = settingsPanel && settingsPanel->IsOpen();

		if (ImGui::MenuItem(LOCAL_LABEL("ToolBar.DisplaySettings").c_str(),
			nullptr, settingsOpen, settingsPanel != nullptr) && settingsPanel)
		{
			settingsPanel->ToggleOpen();
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

#pragma region PlayControls

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

	ImGui::SameLine(0.0f, 15.0f); // 재생 제어 버튼들과 간격 띄우기
	// ■ C++ GameModule Compile
	auto& buildManager = HotReloadManager::Get().GetModuleBuilder();
	const bool compiling = buildManager.IsCompiling();
	ImGui::PushID("CompileModuleBtn");
	if (compiling) ImGui::BeginDisabled();
	const std::string compileBtnText = compiling ?
		(std::string(ICON_FA_SPINNER) + " " + LOCAL_LABEL("ToolBar.ModuleCompiling")) :
		(std::string(ICON_FA_HAMMER) + " " + LOCAL_LABEL("ToolBar.ModuleCompile"));
	if (ImGui::Button(compileBtnText.c_str()))
	{
		buildManager.RequestBuild();
	}
	if (compiling) ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
	{
		ImGui::SetTooltip("%s (%s)", LOCAL_CSTR("ToolBar.ModuleCompileTooltip"), ShortcutManager::Get().GetShortcutHint("Build.CompileModule"));
	}
	ImGui::PopID();
	ImGui::SameLine(0.0f, 5.0f);
	bool autoCompile = buildManager.IsAutoCompileEnabled();
	if (ImGui::Checkbox(LOCAL_LABEL("ToolBar.AutoCompile").c_str(), &autoCompile))
		buildManager.SetAutoCompileEnabled(autoCompile);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", LOCAL_CSTR("ToolBar.AutoCompileTooltip"));
	}
	ImGui::SameLine(0.0f, 10.0f);
	// ■ Hot Reload
	ImGui::PushID("HotReloadBtn");
	if (ImGui::Button((ICON_FA_ARROWS_ROTATE " " + LOCAL_LABEL("ToolBar.ModuleReload")).c_str()))
	{
		HotReloadManager::Get().GetModuleReloader().ReloadModule();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s (%s)", LOCAL_FMT("ToolBar.ReloadModuleTooltip", HotReloadManager::Get().GetModuleReloader().GetReloadIndex()).c_str(), ShortcutManager::Get().GetShortcutHint("Build.ReloadModule"));
	}
	ImGui::PopID();

	ImGui::SameLine(0.0f, 5.0f);
	auto& moduleReloader = HotReloadManager::Get().GetModuleReloader();
	bool autoReload = moduleReloader.IsAutoReloadEnabled();

	if (ImGui::Checkbox(LOCAL_LABEL("ToolBar.AutoReload").c_str(), &autoReload))
		moduleReloader.SetAutoReload(autoReload);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", LOCAL_CSTR("ToolBar.AutoReloadTooltip"));
	}

	ImGui::SameLine();
	// 셰이더 전체 재컴파일 및 핫스왑 버튼
	if (ImGui::Button(LOCAL_LABEL("ToolBar.ShaderReload").c_str()))
	{
		HotReloadManager::Get().GetShaderReloader().RequestRecompileAll();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s (%s)", LOCAL_CSTR("ToolBar.ShaderReloadTooltip"), ShortcutManager::Get().GetShortcutHint("Build.ReloadShaders"));
	}
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