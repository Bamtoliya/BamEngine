#pragma once

#include "imgui.h"
#include "ToolBar.h"

#include "Scene.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "Transform.h"

#include "LocalizationManager.h"

#include "ViewportPanels.h"
#include "InspectorPanel.h"
#include "HierarchyPanel.h"
#include "ImGuiManager.h"

#include "Application.h"

#include "Archives.h"
#include "FileDialogs.h"

#include "SelectionManager.h"
#include "AssetManager.h"
#include "ContentBrowserPanel.h"

#include "MeshFilter.h"
#include "MeshRenderer.h"

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
	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("Exit", "Alt+F4"))
		{
			SDL_Event quit_event;
			quit_event.type = SDL_EVENT_QUIT;
			SDL_PushEvent(&quit_event);
		}
		if (ImGui::MenuItem("Import Asset", ""))
		{
			ImportAsset();
		}
		if (ImGui::MenuItem("Load Resource", ""))
		{
			LoadResourceFile();
		}

		if (ImGui::MenuItem("Settings", ""))
		{

		}
		ImGui::EndMenu();
	}
}

void ToolBar::ImportAsset()
{
	wstring filePath;
	if (!FileDialogs::OpenFileDialog(filePath, { {L"Asset Files", L""}}, filePath))
		return;
	AssetManager::Get().Import(filePath, filePath);
	//static_cast<ContentBrowserPanel*>(ImGuiManager::Get().GetImGuiPanel(L"Content Browser"))->RequestRefresh();
}

void ToolBar::LoadResourceFile()
{
	wstring filePath;
	if (!FileDialogs::OpenFileDialog(filePath, { {L"Resource Files", L""} }, filePath))
		return;
	ResourceManager::Get().LoadFile(filePath);
}


#pragma endregion


#pragma region EditMenu
void ToolBar::DrawEditMenu()
{
	if (ImGui::BeginMenu("Edit"))
	{
		ImGui::EndMenu();
	}
}
#pragma endregion


#pragma region SceneMenu
void ToolBar::DrawSceneMenu()
{
	if (ImGui::BeginMenu("Scene"))
	{
		if (ImGui::MenuItem("New Scene", "Ctrl+N"))
		{
			SelectionManager::Get().ClearSelection();
			NewScene();
		}

		if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
		{
			SaveScene();
		}

		if (ImGui::MenuItem("Load Scene", "Ctrl+O"))
		{
			SelectionManager::Get().ClearSelection();
			LoadScene();
		}

		if (ImGui::MenuItem("Close Scene"))
		{
			SelectionManager::Get().ClearSelection();
			SceneManager::Get().CloseScene();
		}

		ImGui::EndMenu();
	}
}
void ToolBar::NewScene()
{
	SceneManager::Get().NewScene();
	Scene* currentScene = SceneManager::Get().GetCurrentScene();
	//if (currentScene)
	//{
	//	currentScene->SetName(L"Untitled Scene");
	//	currentScene->CreateLayer(L"Default");

	//	GameObject* gameObject = GameObject::Create();
	//	gameObject->SetName(L"Camera");
	//	Camera* camera = static_cast<Camera*>(gameObject->AddComponent(L"Camera"));
	//	CameraManager::Get().AddCamera(camera);
	//	CameraManager::Get().SetMainCamera(camera);
	//	currentScene->AddGameObject(gameObject);
	//	Safe_Release(gameObject);


	//	GameObject* plane = GameObject::Create();
	//	plane->SetName(L"Plane");
	//	MeshFilter* meshFilter = static_cast<MeshFilter*>(plane->AddComponent(L"MeshFilter"));
	//	meshFilter->SetMeshHandle(ResourceManager::Get().GetResourceHandle<Mesh>(L"Resources/Mesh/Plane.bammesh"));
	//	MeshRenderer* meshRenderer = static_cast<MeshRenderer*>(plane->AddComponent(L"MeshRenderer"));
	//	meshRenderer->SetMaterial(ResourceManager::Get().GetResourceHandle<Material>(L"Resources/Material/DefaultMaterial.bammat"));
	//	currentScene->AddGameObject(plane);
	//	Safe_Release(plane);


	//}
}

void ToolBar::SaveScene()
{
	wstring filePath;
	if (!FileDialogs::SaveFileDialog(filePath, { {L"Scene Files (*.json)", L"*.json"} }, L"NewScene.json", L"json"))
		return;

	JsonArchive archive(EArchiveMode::Write);
	SceneManager::Get().SaveScene(archive, filePath);
}
void ToolBar::LoadScene()
{
	wstring filePath;
	if (!FileDialogs::OpenFileDialog(filePath, { {L"Scene Files (*.json)", L"*.json"} }))
		return;

	JsonArchive archive(EArchiveMode::Read);
	SceneManager::Get().LoadScene(archive, filePath);
}
#pragma endregion


#pragma region WindowMenu

void ToolBar::DrawWindowMenu()
{
	if (ImGui::BeginMenu("Window"))
	{
		if (ImGui::MenuItem("Display Settings"))
		{
			m_DisplaySettingsWindow = !m_DisplaySettingsWindow;
		}

		if (ImGui::MenuItem("New Viewport Panel"))
		{
			AddNewViewportPanel();
		}

		ImGui::Separator();

		if (ImGui::BeginMenu("Panels"))
		{
			const auto& panels = ImGuiManager::Get().GetImGuiPanels();

			for (const auto& panel : panels)
			{
				bool IsOpen = panel->IsOpen();
				if (ImGui::MenuItem(WStrToStr(panel->GetName()).c_str(), nullptr, &IsOpen))
				{
					panel->SetOpen(IsOpen);
				}
			}


			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}

	if(m_DisplaySettingsWindow)
		DrawDisplaySettingsWindow();

	DrawNewViewportPopup();
}
void ToolBar::DrawDisplaySettingsWindow()
{
	if (ImGui::Begin("Display Settings", &m_DisplaySettingsWindow)) // 창 이름
	{
		static int width = g_WindowWidth;
		static int height = g_WindowHeight;
		static bool isFullscreen = false;

		// 현재 상태 가져오기 (처음 한 번만 동기화하거나, 매번 갱신하거나 선택)
		// 여기서는 UI 조작 값을 우선하도록 단순화했습니다.

		ImGui::Text("Resolution");
		ImGui::InputInt("Width", &width);
		ImGui::InputInt("Height", &height);

		ImGui::Checkbox("Fullscreen", &isFullscreen);

		if (ImGui::Button("Apply Resolution"))
		{
			// 실제 적용
			Application::Get().SetResolution(width, height, isFullscreen);
		}

		ImGui::Separator();

		// [옵션] UI 스케일 조절 (4K 모니터 대응용)
		static float uiScale = 1.0f;
		if (ImGui::DragFloat("UI Scale", &uiScale, 0.01f, 0.5f, 3.0f))
		{
			ImGui::GetIO().FontGlobalScale = uiScale; // 간단한 스케일링 방법
		}

		ImGui::Separator();
		static int32 targetFPS = TimeManager::Get().GetTargetFPS();
		ImGui::DragInt("FPS", &targetFPS);
		if (ImGui::Button("Apply FPS"))
		{
			TimeManager::Get().SetTargetFPS(targetFPS);
		}
	}
	ImGui::End();
}
void ToolBar::AddNewViewportPanel()
{
	m_newViewportDesc = tagViewportPanelDesc();
	strcpy_s(m_NewViewportNameBuf, "New Viewport");
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
		ImGui::InputText("Name", m_NewViewportNameBuf, sizeof(m_NewViewportNameBuf));
		ImGui::InputInt("Width", (int*)&m_newViewportDesc.RenderTargetWidth);
		ImGui::InputInt("Height", (int*)&m_newViewportDesc.RenderTargetHeight);
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
		if (ImGui::Button("Create"))
		{
			m_newViewportDesc.Name = StrToWStr(m_NewViewportNameBuf);
			BaseViewportPanel* newPanel = new BaseViewportPanel();
			newPanel->Initialize(&m_newViewportDesc);
			ImGuiManager::Get().AddImGuiPanel(newPanel);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
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
	if (ImGui::BeginMenu("Help"))
	{
		string buffer = LocalizationManager::Get().GetText("PROP_LANGUAGE");
		if (ImGui::BeginMenu(buffer.c_str()))
		{
			if (ImGui::MenuItem("English"))
				LocalizationManager::Get().SetCurrentLanguage(ELocalizationLanguage::English);
			if (ImGui::MenuItem("Korean"))
				LocalizationManager::Get().SetCurrentLanguage(ELocalizationLanguage::Korean);
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
}

#pragma endregion

#pragma region PlayControls

void ToolBar::DrawPlayControls()
{
	Application& app = Application::Get();
	EPlayState state = app.GetPlayState();

	const float btnWidth = 60.f;
	const float spacing = ImGui::GetStyle().ItemSpacing.x;

	// ── Pause 상태 시각화: 배경색 변경 ──────────────────────────────────
	if (state == EPlayState::Pause)
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 windowPos = ImGui::GetWindowPos();
		ImVec2 windowSize = ImGui::GetWindowSize();

		// 주황색 배경 (Pause 상태 강조)
		ImU32 pauseColor = ImGui::GetColorU32(ImVec4(1.0f, 0.8f, 0.2f, 0.3f)); // RGBA
		drawList->AddRectFilled(
			windowPos,
			ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
			pauseColor);
	}
	// ─────────────────────────────────────────────────────────────────────

	float totalWidth = btnWidth * 3.0f + spacing * 2.0f;
	float startX = (ImGui::GetWindowWidth() - totalWidth) * 0.5f;
	if (startX < 0.0f) startX = 0.0f;
	ImGui::SetCursorPosX(startX);

	// ── ▶ Play (Edit 상태일 때만 활성) ───────────────
	ImGui::PushID("PlayBtn");
	if (state != EPlayState::Edit) ImGui::BeginDisabled();
	if (ImGui::Button(ICON_FA_PLAY, ImVec2(btnWidth, 0)))
		app.EnterPlayMode();
	if (state != EPlayState::Edit) ImGui::EndDisabled();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play (Enter PIE)");
	ImGui::PopID();

	ImGui::SameLine();

	ImGui::PushID("PauseResumeBtn");
	if (state == EPlayState::Edit) ImGui::BeginDisabled();
	if (state == EPlayState::Pause)
	{
		if (ImGui::Button(ICON_FA_PLAY, ImVec2(btnWidth, 0)))
			app.ResumePlayMode();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Resume: 게임 로직 재개");
	}
	else
	{
		if (ImGui::Button(ICON_FA_PAUSE, ImVec2(btnWidth, 0)))
			app.PausePlayMode();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pause: 게임 정지, 에디터 조작 가능");
	}
	if (state == EPlayState::Edit) ImGui::EndDisabled();
	ImGui::PopID();

	ImGui::SameLine();

	ImGui::PushID("StopBtn");
	if (state == EPlayState::Edit) ImGui::BeginDisabled();
	if (ImGui::Button(ICON_FA_STOP, ImVec2(btnWidth, 0)))
		app.StopPlayMode();
	if (state == EPlayState::Edit) ImGui::EndDisabled();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop (restore scene)");
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
