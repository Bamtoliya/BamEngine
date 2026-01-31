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
		ImGui::EndMenu();
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