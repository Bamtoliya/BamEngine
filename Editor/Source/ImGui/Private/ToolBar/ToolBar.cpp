#pragma once

#include "imgui.h"
#include "ToolBar.h"

#include "Scene.h"
#include "SceneManager.h"
#include "Transform.h"

#include "LocalizationManager.h"


void ToolBar::Draw()
{
	if (ImGui::BeginMainMenuBar())
	{
		DrawFileMenu();
		DrawEditMenu();
		DrawSceneMenu();
		DrawWindowMenu();
		DrawHelpMenu();
		ImGui::EndMainMenuBar();
	}
}

#pragma region FileMenu
void ToolBar::DrawFileMenu()
{
	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("Eixt", "Alt+F4"))
		{
			SDL_Event quit_event;
			quit_event.type = SDL_EVENT_QUIT;
			SDL_PushEvent(&quit_event);
		}
		ImGui::EndMenu();
	}
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
			SceneManager::Get().NewScene();
			Scene* currentScene = SceneManager::Get().GetCurrentScene();
			if (currentScene)
			{
				currentScene->SetName(L"Untitled Scene");
				currentScene->CreateLayer(L"Default");

				GameObject* gameObject = GameObject::Create();
				currentScene->AddGameObject(gameObject);
				gameObject->AddComponent<Transform>();
				Safe_Release(gameObject);
			}
		}

		if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
		{

		}

		if (ImGui::MenuItem("Load Scene", "Ctrl+O"))
		{

		}

		ImGui::EndMenu();
	}
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
		ImGui::EndMenu();
	}

	if(m_DisplaySettingsWindow)
		DrawDisplaySettingsWindow();
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
	}
	ImGui::End();
}
#pragma endregion