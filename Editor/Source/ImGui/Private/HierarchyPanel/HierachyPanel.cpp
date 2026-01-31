#pragma once

#include "HierarchyPanel.h"
#include "imgui.h"

#include "SceneManager.h"
#include "Scene.h"
#include "Layer.h"
#include "GameObject.h"


void HierarchyPanel::Draw(class GameObject*& selectedObject)
{
	ImGui::Begin("Hierarchy");
	Scene* currentScene = SceneManager::Get().GetCurrentScene();

	if (currentScene)
	{
		string sceneName = WStrToStr(currentScene->GetName());
		ImGui::Text("Scene: %s", sceneName.c_str());
		const vector<Layer*>& layers = currentScene->GetAllLayers();
		for (const Layer* layer : layers)
		{
			if (ImGui::CollapsingHeader(WStrToStr(layer->GetName()).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				const vector<GameObject*> gameObjects = layer->GetAllGameObjects();
				for (GameObject* gameObject : gameObjects)
				{
					if (gameObject->GetParent() == nullptr)
					{
						DrawGameObjectNode(gameObject, selectedObject);
					}
				}
			}
		}
	}
	else
	{
		ImGui::Text("No scene loaded.");

		if (ImGui::Button("Create New Scene"))
		{
			SceneManager::Get().NewScene();
			selectedObject = nullptr;
		}
	}
	ImGui::End();
}

void HierarchyPanel::DrawGameObjectNode(class GameObject* gameObject, class GameObject*& selectedObject)
{
	ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

	if (gameObject == selectedObject)
		nodeFlags |= ImGuiTreeNodeFlags_Selected;


	const vector<GameObject*>& children = gameObject->GetAllChilds();
	if (children.empty())
		nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

	bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)gameObject->GetID(), nodeFlags, "%S", gameObject->GetName().c_str());

	if (ImGui::IsItemClicked())
	{
		selectedObject = gameObject;
	}

	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("GAMEOBJECT_PAYLOAD", &gameObject, sizeof(GameObject*));
		ImGui::Text("%S", gameObject->GetName().c_str());
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT_PAYLOAD"))
		{
			GameObject* const* droppedObject = (GameObject* const*)payload->Data;
			if ((*droppedObject) != gameObject && (*droppedObject)->GetParent() != gameObject)
			{
				(*droppedObject)->GetParent()? (*droppedObject)->GetParent()->RemoveChild() : EResult::Success;
				gameObject->AddChild(*droppedObject);
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (opened && !children.empty())
	{
		for (GameObject* child : children)
		{
			DrawGameObjectNode(child, selectedObject);
		}
		ImGui::TreePop();
	}
}