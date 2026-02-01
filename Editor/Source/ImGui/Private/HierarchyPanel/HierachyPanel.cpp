#pragma once

#include "HierarchyPanel.h"

#include "SceneManager.h"
#include "Scene.h"
#include "Layer.h"
#include "GameObject.h"
#include "Transform.h"


void HierarchyPanel::Draw(class GameObject*& selectedObject)
{
	ImGui::Begin("Hierarchy");
	Scene* currentScene = SceneManager::Get().GetCurrentScene();

	if (currentScene)
	{
		string sceneName = WStrToStr(currentScene->GetName());
		ImGui::Text("Scene: %s", sceneName.c_str());

		DrawAddGameObjectButton(currentScene, selectedObject);

		const vector<Layer*>& layers = currentScene->GetAllLayers();
		for (auto* layer : layers)
		{
			DrawLayerItem(layer, selectedObject, currentScene);
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

	if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("Add Layer"))
		{
			// 새 레이어 생성 (이름은 임시로 지정, 필요 시 카운팅하여 New Layer 1, 2 등으로 확장 가능)
			currentScene->CreateLayer(L"New Layer");
		}
		ImGui::EndPopup();
	}
	// ----------------------------------------------------

	// 빈 공간 좌클릭 시 선택 해제 (기존 코드 유지)
	if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
	{
		ClearSelection();
		selectedObject = nullptr;
	}
	else
	{
		// ... (씬 없을 때 처리) ...
	}
	ImGui::End();
}

void HierarchyPanel::DrawLayerItem(Layer* layer, GameObject*& selectedObject, Scene* scene)
{
	uint32 layerIndex = layer->GetIndex();
	ImGui::PushID(layerIndex);

#pragma region Flag Check
	bool hasAnyObjectActive = false;
	bool hasAnyObjectVisible = false;
	bool hasAllObjectActive = true;

	const auto& layerObjects = layer->GetAllGameObjects();
	if (layerObjects.empty())
	{
		hasAllObjectActive = false;
	}
	else
	{
		for (auto* obj : layerObjects)
		{
			if (obj->IsActive()) hasAnyObjectActive = true;
			else hasAllObjectActive = false;
			if (obj->IsVisible()) hasAnyObjectVisible = true;
		}
	}
#pragma endregion

#pragma region LayerIndex
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
	ImGui::Text("%02d", layerIndex);
	ImGui::PopStyleColor();
	ImGui::SameLine();
#pragma endregion

#pragma region Active CheckBox
	bool isLayerActive = hasAllObjectActive;
	bool isMixedActive = hasAnyObjectActive && !hasAllObjectActive;

	if (isMixedActive)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
	}
	if (ImGui::Checkbox("##LayerActive", &isLayerActive))
	{
		layer->SetActive(isLayerActive);
	}
	if(isMixedActive)
		ImGui::PopItemFlag();
	ImGui::SameLine();
#pragma endregion

#pragma region Layer Name Box
	f32 headerY = ImGui::GetCursorPosY();
	string layerName = WStrToStr(layer->GetName());
	char label[256];
	sprintf_s(label, "%s###Layer_%d", layerName.c_str(), layer->GetIndex());
	bool layerOpen = ImGui::CollapsingHeader(layerName.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_DrawLinesToNodes);
	f32 nextItemY = ImGui::GetCursorPosY();
#pragma endregion


#pragma region Visible CheckBox
	ImGui::SetCursorPosY(headerY);
	ImGui::Dummy(ImVec2(0.0f, 0.0f));
	float visibleButtonWidth = 30.0f; // 버튼 크기
	float windowWidth = ImGui::GetWindowContentRegionMax().x;
	ImGui::SetCursorPosX(windowWidth - visibleButtonWidth - 5.0f); // 우측 끝에서 5픽셀 안쪽
	ImGui::Dummy(ImVec2(0.0f, 0.0f));

	// 버튼 배경을 투명하게 해서 헤더 위에 글자만 떠 있는 것처럼 보이게 함
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f)); // 호버 시 살짝 밝게
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.2f));  // 클릭 시


	bool isLayerVisible = layer->IsVisible();
	if (hasAnyObjectVisible && !isLayerVisible)
	{
		layer->SetVisible(true);
	}

	if (ImGui::Button(isLayerVisible ? "(O)" : "(-)", ImVec2(visibleButtonWidth, ImGui::GetFrameHeight())))
	{
		bool toggle = !isLayerVisible;
		layer->SetVisible(toggle);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Toggle Layer Visibility");
	}
	ImGui::PopStyleColor(3);
#pragma endregion

#pragma region Draw Objects
	ImGui::SetCursorPosY(nextItemY);
	ImGui::Dummy(ImVec2(0.0f, 0.0f));
	if (layerOpen)
	{
		const vector<GameObject*>& gameObjects = layer->GetAllGameObjects();
		for (GameObject* gameObject : gameObjects)
		{
			if (gameObject->GetParent() == nullptr)
			{
				DrawGameObjectNode(gameObject, selectedObject);
			}
		}
	}
#pragma endregion
	ImGui::PopID();
}

void HierarchyPanel::DrawGameObjectNode(class GameObject* gameObject, class GameObject*& selectedObject)
{
	ImGui::Indent(30.f);
	ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;
	nodeFlags |= ImGuiTreeNodeFlags_DrawLinesToNodes;

	if (gameObject == selectedObject)
		nodeFlags |= ImGuiTreeNodeFlags_Selected;


	const vector<GameObject*>& children = gameObject->GetAllChilds();
	if (children.empty())
		nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;


	bool isActive = gameObject->IsActive();
	if (ImGui::Checkbox("##Active", &isActive))
	{
		gameObject->SetActive(isActive);
	}
	ImGui::SameLine();
	float headerY = ImGui::GetCursorPosY();
		
	string gameObjectName = WStrToStr(gameObject->GetName());
	bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)gameObject->GetID(), nodeFlags, "%s", gameObjectName.c_str());

	if (ImGui::IsItemClicked())
	{
		selectedObject = gameObject;
	}

	float nextItemY = ImGui::GetCursorPosY();

	ImGui::SetCursorPosY(headerY);
	ImGui::Dummy(ImVec2(0.0f, 0.0f));
	float visibleButtonWidth = 30.0f;
	float windowMaxX = ImGui::GetWindowContentRegionMax().x;
	ImGui::SetCursorPosX(windowMaxX - visibleButtonWidth - 5.0f);
	ImGui::Dummy(ImVec2(0.0f, 0.0f));

	// 투명 버튼 스타일 적용
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.2f));

	bool isVisible = gameObject->IsVisible();

	ImGui::PushID((void*)(uintptr_t)gameObject->GetID());
	if (ImGui::Button(isVisible ? "(O)" : "(-)", ImVec2(visibleButtonWidth, ImGui::GetFrameHeight())))
	{
		bool toggle = !isVisible;
		gameObject->SetVisible(toggle);
	}
	// 툴팁 표시
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Visibility");
	ImGui::PopID();
	ImGui::PopStyleColor(3);

	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("GAMEOBJECT_PAYLOAD", &gameObject, sizeof(GameObject*));
		ImGui::Text("%s", gameObjectName.c_str());
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

	ImGui::SetCursorPosY(nextItemY);
	ImGui::Dummy(ImVec2(0.0f, 0.0f));

	if (opened && !children.empty())
	{
		for (GameObject* child : children)
		{
			DrawGameObjectNode(child, selectedObject);
		}
		ImGui::TreePop();
	}
	ImGui::Unindent();
}

void HierarchyPanel::DrawAddGameObjectButton(Scene* scene, GameObject*& selectedObject)
{
	string buttonText = LocalizationManager::Get().GetText("Hirarchy_AddGameObject");
	if (ImGui::Button(buttonText.c_str(), ImVec2(-1, 0)))
	{
		ImGui::OpenPopup("AddGameObjectPopup");
	}


	if (ImGui::BeginPopup("AddGameObjectPopup"))
	{
		if (ImGui::MenuItem("Empty Object"))
		{
			CreateEmptyObject(scene, selectedObject);
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Cube"))
		{
			CreatePrimitive(scene, selectedObject, L"Cube", L"Cube");
		}

		if (ImGui::MenuItem("Sphere"))
		{
			CreatePrimitive(scene, selectedObject, L"Sphere", L"Sphere");
		}

		if (ImGui::MenuItem("Quad"))
		{
			CreatePrimitive(scene, selectedObject, L"Quad", L"Quad");
		}

		ImGui::EndPopup();
	}
	ImGui::Separator();
}

bool HierarchyPanel::IsSelected(GameObject* gameObject) const
{
	return find(m_SelectedObjects.begin(), m_SelectedObjects.end(), gameObject) != m_SelectedObjects.end();
}

void HierarchyPanel::ClearSelection()
{
	m_SelectedObjects.clear();
	m_LastSelectedObject = nullptr;
}

void HierarchyPanel::CreateEmptyObject(Scene* scene, GameObject*& selectedObject)
{
	GameObject* newGameObject = GameObject::Create();
	newGameObject->SetName(L"New GameObject");
	newGameObject->AddComponent<Transform>();
	scene->AddGameObject(newGameObject);
	Safe_Release(newGameObject);
}

void HierarchyPanel::CreatePrimitive(Scene* scene, GameObject*& selectedObject, const wstring& name, const wstring& meshName)
{

}
