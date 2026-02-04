#pragma once

#include "HierarchyPanel.h"

#include "ImGuiManager.h"
#include "SelectionManager.h"

#include "SceneManager.h"
#include "Scene.h"
#include "Layer.h"
#include "GameObject.h"
#include "Transform.h"


static bool CheckboxTristate(const char* label, bool* v, bool is_mixed)
{
	bool pressed = false;
	if (is_mixed)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
		pressed = ImGui::Checkbox(label, v);
		ImGui::PopItemFlag();
	}
	else
	{
		pressed = ImGui::Checkbox(label, v);
	}
	return pressed;
}

static uintptr_t GetNodeID(GameObject* obj)
{
	return (uintptr_t)obj->GetID();
}

// 2. Scene은 메모리 주소를 ID로 사용
static uintptr_t GetNodeID(Scene* scene)
{
	return (uintptr_t)scene;
}

// 3. Layer도 메모리 주소를 ID로 사용
static uintptr_t GetNodeID(Layer* layer)
{
	return (uintptr_t)layer;
}


template<typename T>
bool HierarchyPanel::DrawRenameBox(T* target, ImGuiTreeNodeFlags flags, bool isSelected)
{
	void* targetId = (void*)GetNodeID(target);
	string name = WStrToStr(target->GetName());

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y));
	ImGui::AlignTextToFramePadding();
	if (m_RenamingId == targetId)
	{
		ImGui::Bullet();
		ImGui::SameLine();

		if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))
			ImGui::SetKeyboardFocusHere();

		if (ImGui::InputText("##Rename", m_RenameBuffer, sizeof(m_RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
		{
			target->SetName(StrToWStr(m_RenameBuffer));
			m_RenamingId = nullptr;
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			m_RenamingId = nullptr;
		}
		else if (ImGui::IsItemDeactivated())
		{
			target->SetName(StrToWStr(m_RenameBuffer));
			m_RenamingId = nullptr;
		}
		ImGui::PopStyleVar();
		return false;
	}

	// 2. [일반 표시 모드]
	ImGui::SetNextItemAllowOverlap();

	// 가져온 ID(targetId)를 사용하여 TreeNode 생성
	bool opened = ImGui::TreeNodeEx(targetId, flags, "%s", name.c_str());

	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		m_RenamingId = targetId;                 // 리네임 대상 설정
		strcpy_s(m_RenameBuffer, name.c_str());  // 현재 이름 버퍼에 복사
	}

	// 3. [F2 키 트리거]
	if (isSelected && ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_F2))
		{
			m_RenamingId = targetId;
			strcpy_s(m_RenameBuffer, name.c_str());
		}
	}

	ImGui::SameLine();

	// ---------------------------------------------------------------------
		// 수정 버튼 (펜 아이콘)
		// ---------------------------------------------------------------------
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));

#ifdef ICON_FA_PEN
	const char* editIcon = ICON_FA_PEN;
#else
	const char* editIcon = "(E)";
#endif
	// 작은 버튼
	if (ImGui::Button(editIcon, ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight())))
	{
		// [핵심] 리네임 시작 트리거
		m_RenamingId = targetId;                 // ID 등록
		strcpy_s(m_RenameBuffer, name.c_str()); // 이름 복사
	}

	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rename Scene");

	ImGui::PopStyleColor(3);
	
	ImGui::PopStyleVar();
	return opened;
}

void HierarchyPanel::Draw()
{
	ImGui::Begin("Hierarchy");
	
	Scene* currentScene = SceneManager::Get().GetCurrentScene();
	if (currentScene)
	{
		DrawSceneTitle(currentScene);
		
		DrawAddGameObjectButton(currentScene);

		const vector<Layer*>& layers = currentScene->GetAllLayers();
		for (auto* layer : layers)
		{
			DrawLayerItem(currentScene, layer);
		}
	}
	else
	{
		ImGui::Text("No scene loaded.");

		if (ImGui::Button("Create New Scene"))
		{
			SceneManager::Get().NewScene();
			SelectionManager::Get().ClearSelection();
		}
	}

	if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("Add Layer"))
		{
			currentScene->CreateLayer(L"New Layer");
		}
		ImGui::EndPopup();
	}
	
	if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
	{
		if (!ImGui::IsAnyItemHovered())
		{
			SelectionManager::Get().ClearSelection();
		}
	}
	else
	{
		// ... (씬 없을 때 처리) ...
	}
	ImGui::End();
}

void HierarchyPanel::DrawLayerItem(Scene* scene, Layer* layer)
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
	ImGui::AlignTextToFramePadding();
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
	ImGui::Text("%02d", layerIndex);
	ImGui::PopStyleColor();
	ImGui::SameLine();
#pragma endregion

#pragma region Layer Name (Inline Input)
	static int s_RenamingLayerIdx = -1;
	static char s_RenamingBuf[256] = "";
	string layerName = WStrToStr(layer->GetName());

	// 현재 이 레이어가 수정 중인지 확인
	bool isRenaming = (s_RenamingLayerIdx == (int)layerIndex);
	ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Framed;
	bool layerOpen = DrawRenameBox(layer, nodeFlags, false);;
#pragma endregion
#pragma region Checkboxes
	if (!isRenaming)
	{

		ImGui::SameLine();

		// 1. 사이즈 및 위치 계산
		float itemHeight = ImGui::GetFrameHeight();
		float visibleBtnWidth = itemHeight;                  // Visible 버튼 너비
		float activeBoxWidth = itemHeight;              // 체크박스는 보통 정사각형 (높이와 같음)
		float spacing = itemHeight / 6.f;                           // 컨트롤 간 간격
		float rightPadding = itemHeight / 2.f;                      // 윈도우 우측 여백
		float windowWidth = ImGui::GetWindowContentRegionMax().x;

		float activeBoxPos = windowWidth - (visibleBtnWidth * 3) - (spacing * 3) - activeBoxWidth - rightPadding;
		ImGui::SetCursorPosX(activeBoxPos);

		// 스타일 보정 (작은 화살표 버튼을 위해 패딩 조절)
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // 배경 투명

		// 1. 위로 이동 (Up Arrow)
		ImGui::PushID("MoveUp");
		// ArrowButton 대신 Button을 써서 크기를 강제함 (frameHeight)
		if (ImGui::Button(ICON_FA_UP_LONG, ImVec2(visibleBtnWidth, itemHeight)))
		{
			SceneManager::Get().GetCurrentScene()->ReorderLayer(layerIndex, layerIndex - 1);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move Up");
		ImGui::PopID();

		ImGui::SameLine(); // 간격 0

		// 2. 아래로 이동 (Down)
		ImGui::PushID("MoveDown");
		if (ImGui::Button(ICON_FA_DOWN_LONG, ImVec2(visibleBtnWidth, itemHeight)))
		{
			SceneManager::Get().GetCurrentScene()->ReorderLayer(layerIndex, layerIndex + 1);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move Down");
		ImGui::PopID();

		ImGui::PopStyleVar(2);   // FramePadding, ItemSpacing 복구
		ImGui::PopStyleColor();  // Button Color 복구

		ImGui::SameLine();

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

		if (isMixedActive)
			ImGui::PopItemFlag();

		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Layer Active");

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // 투명 버튼

		bool isVisible = layer->IsVisible();
#ifdef ICON_FA_EYE
		const char* visIcon = isVisible ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
#else
		const char* visIcon = isVisible ? "(O)" : "(-)";
#endif

		if (ImGui::Button(visIcon, ImVec2(visibleBtnWidth, itemHeight)))
		{
			layer->SetVisible(!isVisible);
		}

		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Layer Visibility");

		ImGui::PopStyleColor();
	}
#pragma endregion


#pragma region Draw Objects
	if (layerOpen)
	{
		const vector<GameObject*>& gameObjects = layer->GetAllGameObjects();
		for (GameObject* gameObject : gameObjects)
		{
			if (gameObject->GetParent() == nullptr)
			{
				DrawGameObjectNode(gameObject);
			}
		}
		ImGui::TreePop();
	}
#pragma endregion
	ImGui::PopID();
}

void HierarchyPanel::DrawGameObjectNode(class GameObject* gameObject)
{
	ImGui::PushID((void*)(uintptr_t)gameObject->GetID());
	float fontSize = ImGui::GetFontSize();
	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, fontSize);
	ImGui::Indent();
	
	SelectionManager& selectionManager = SelectionManager::Get();
	auto& selectionContext = selectionManager.GetSelectionContext();

	bool isSelected = false;
	for (auto* sel : selectionContext) {
		if (sel == gameObject) { isSelected = true; break; }
	}

	ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	nodeFlags |= ImGuiTreeNodeFlags_DrawLinesFull;
	if (isSelected)
		nodeFlags |= ImGuiTreeNodeFlags_Selected;

	const vector<GameObject*>& children = gameObject->GetAllChilds();
	if (children.empty())
		nodeFlags |= ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen;

#pragma region Flag Check
	bool hasAnyChildActive = false;
	bool hasAnyChildVisible = false;
	bool hasAllChildActive = true;

	const auto& childObjects = gameObject->GetAllChilds();
	if (childObjects.empty())
	{
		hasAnyChildActive = false;
	}
	else
	{
		for (auto* obj : childObjects)
		{
			if (obj->IsActive()) hasAnyChildActive = true;
			else hasAllChildActive = false;
			if (obj->IsVisible()) hasAnyChildVisible = true;
		}
	}
#pragma endregion
		
#pragma region Namebox
	bool opened = DrawRenameBox(gameObject, nodeFlags, isSelected);
#pragma region Selection Control
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        if (ImGui::GetIO().KeyCtrl)
        {
            // Ctrl 키 누른 상태: 토글 (기존 유지)
            selectionManager.ToggleSelection(gameObject);
        }
        else
        {
            // Ctrl 키 없음
            if (!isSelected)
            {
                // 선택되지 않은 녀석을 클릭함 -> 즉시 얘만 선택 (기존 선택 다 취소)
                selectionContext.clear();
                selectionContext.push_back(gameObject);
            }
            // ELSE: 이미 선택된 녀석을 클릭함 -> 아무것도 안 함 (드래그 준비 상태 유지)
            // 여기서 selectionContext를 clear() 해버리면 멀티 셀렉션이 날아갑니다.
        }
    }

    // [추가됨] 2. 마우스 뗐을 때 (MouseUp)
    // 드래그를 하지 않고 그냥 클릭만 했다면, 이때 나머지 선택을 해제합니다.
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        if (!ImGui::GetIO().KeyCtrl && !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            // Ctrl도 안 눌렀고, 드래그도 안 함 -> 단순 클릭으로 간주
            // 이제서야 "나 말고 나머지 선택 해제"를 수행
            if (isSelected) 
            {
                // 단, 현재 선택된 갯수가 1개보다 많을 때만 정리 (성능 최적화)
                if (selectionContext.size() > 1)
                {
                    selectionContext.clear();
                    selectionContext.push_back(gameObject);
                }
            }
        }
    }
#pragma endregion
	
#pragma region Context Menu
	
	if (ImGui::BeginPopupContextItem())
	{
		if(ImGui::MenuItem("Delete Object"))
		{
			//if (!selectionManager.IsSelected(gameObject))
			//{
			//	selectionManager.ClearSelection();
			//	selectionManager.GetSelectionContext().push_back(gameObject);
			//}
			//char label[64];
			//sprintf_s(label, "Delete Selected (%d)", (int)selectionContext.size());
			//
			//if (ImGui::MenuItem(label))
			//{
			//	for (GameObject* obj : selectionContext)
			//	{
			//		obj->SetDead();
			//	}
			//
			//	selectionContext.clear();
			//}
		}
		ImGui::EndPopup();
	}
#pragma endregion

	
#pragma region Drap & Drop

	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("GAMEOBJECT_PAYLOAD", &gameObject, sizeof(GameObject*));
		if (selectionManager.IsSelected(gameObject) && selectionManager.GetSelectionContext().size() > 1)
		{
			ImGui::Text("%s (+%d objects)", WStrToStr(gameObject->GetName()).c_str(), (int)selectionManager.GetSelectionContext().size() - 1);
		}
		else
		{
			ImGui::Text("%s", WStrToStr(gameObject->GetName()).c_str());
		}

		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT_PAYLOAD"))
		{
			GameObject* droppedObject = *(GameObject**)payload->Data;
			vector<GameObject*> objectsToMove;

			if (selectionManager.IsSelected(droppedObject))
			{
				objectsToMove = selectionManager.GetSelectionContext();
			}
			else
			{
				objectsToMove.push_back(droppedObject);
			}

			for (GameObject* obj : objectsToMove)
			{
				if (!obj || obj == gameObject) continue;
				if (gameObject->IsDescendant(obj) || obj->GetParent() == gameObject) continue;

				Safe_AddRef(obj);
				if (obj->GetParent() == nullptr)
				{
					SceneManager::Get().GetCurrentScene()->RemoveGameObject(obj);
				}
				gameObject->AddChild(obj);
				Safe_Release(obj);
			}
		}
		ImGui::EndDragDropTarget();
	}
#pragma endregion

#pragma endregion

#pragma region Checkboxes
	ImGui::SameLine();

	// 1. 사이즈 및 위치 계산
	float itemHeight = ImGui::GetFrameHeight();
	float buttonSize = itemHeight;
	float spacing = itemHeight / 6.f;                           // 컨트롤 간 간격
	float rightPadding = itemHeight / 2.f;                      // 윈도우 우측 여백
	float windowWidth = ImGui::GetWindowContentRegionMax().x;

	float totalRightWidth = itemHeight + (buttonSize * 3) + (spacing * 3) + rightPadding;
	float activeBoxPos = windowWidth - totalRightWidth;
	ImGui::SetCursorPosX(activeBoxPos);

	// 스타일 보정 (작은 화살표 버튼을 위해 패딩 조절)
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // 배경 투명
	
	// 1. 위로 이동 (Up Arrow)
	ImGui::PushID("MoveUp");
	// ArrowButton 대신 Button을 써서 크기를 강제함 (frameHeight)
	if (ImGui::Button(ICON_FA_UP_LONG, ImVec2(buttonSize, itemHeight)))
	{
		GameObject* parent = gameObject->GetParent();
		if (parent)
		{
			parent->MoveChild(gameObject, -1);
		}
		else
		{
			// [루트 객체 처리]
			SceneManager::Get().GetCurrentScene()->MoveGameObjectOrder(gameObject, -1);
		}
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move Up");
	ImGui::PopID();
	
	ImGui::SameLine(); // 간격 0
	
	// 2. 아래로 이동 (Down)
	ImGui::PushID("MoveDown");
	if (ImGui::Button(ICON_FA_DOWN_LONG, ImVec2(buttonSize, itemHeight)))
	{
		GameObject* parent = gameObject->GetParent();
		if (parent)
		{
			parent->MoveChild(gameObject, 1);
		}
		else
		{
			// [루트 객체 처리]
			SceneManager::Get().GetCurrentScene()->MoveGameObjectOrder(gameObject, 1);
		}
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move Down");
	ImGui::PopID();
	
	ImGui::PopStyleVar(2);   // FramePadding, ItemSpacing 복구
	ImGui::PopStyleColor();  // Button Color 복구
	
	ImGui::SameLine();

	bool isObjectActive = hasAllChildActive;
	bool isMixedActive = hasAnyChildActive && !hasAllChildActive;

	if (isMixedActive)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
	}

	bool gameObjectActive = gameObject->IsActive();
	if (ImGui::Checkbox("##ObjectActive", &gameObjectActive))
	{
		gameObject->SetActive(gameObjectActive);
	}

	if (isMixedActive)
		ImGui::PopItemFlag();

	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Object Active");

	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // 투명 버튼

	bool isVisible = gameObject->IsVisible();
#ifdef ICON_FA_EYE
	const char* visIcon = isVisible ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
#else
	const char* visIcon = isVisible ? "(O)" : "(-)";
#endif

	if (ImGui::Button(visIcon, ImVec2(buttonSize, itemHeight)))
	{
		gameObject->SetVisible(!isVisible);
	}

	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Object Visibility");

	ImGui::PopStyleColor();

#pragma endregion

#pragma region Draw Child
	if (opened && !children.empty())
	{
		for (GameObject* child : children)
		{
			DrawGameObjectNode(child);
		}
		ImGui::TreePop();
	}
#pragma endregion
	ImGui::Unindent();
	ImGui::PopStyleVar(); // IndentSpacing Pop
	ImGui::PopID();
}

void HierarchyPanel::DrawAddGameObjectButton(Scene* scene)
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
			CreateEmptyObject(scene);
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Cube"))
		{
			CreatePrimitive(scene, L"Cube", L"Cube");
		}

		if (ImGui::MenuItem("Sphere"))
		{
			CreatePrimitive(scene, L"Sphere", L"Sphere");
		}

		if (ImGui::MenuItem("Quad"))
		{
			CreatePrimitive(scene, L"Quad", L"Quad");
		}

		ImGui::EndPopup();
	}
	ImGui::Separator();
}

void HierarchyPanel::DrawSceneTitle(Scene* scene)
{
	if (!scene) return;

	// 1. 씬의 ID와 이름 가져오기
	// (아까 만든 GetNodeID 오버로딩 함수 사용, 혹은 (void*)scene 직접 캐스팅)
	void* sceneId = (void*)GetNodeID(scene);
	string sceneName = WStrToStr(scene->GetName());

	ImGui::Separator();

	// 2. [이름 변경 모드] 인가? (m_RenamingId 공유)
	if (m_RenamingId == sceneId)
	{
		// 텍스트 박스 너비 계산
		float w = ImGui::GetContentRegionAvail().x;

		// 아이콘/버튼 공간을 고려해 조금 줄일 수도 있음
		// float w = ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight(); 

		ImGui::SetNextItemWidth(w);

		// 포커스 자동 설정
		if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))
		{
			ImGui::SetKeyboardFocusHere(0);
		}

		// InputText 그리기
		if (ImGui::InputText("##RenameScene", m_RenameBuffer, sizeof(m_RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
		{
			scene->SetName(StrToWStr(m_RenameBuffer));
			m_RenamingId = nullptr; // 종료
		}
		else if (ImGui::IsItemDeactivated())
		{
			scene->SetName(StrToWStr(m_RenameBuffer));
			m_RenamingId = nullptr;
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			m_RenamingId = nullptr;
		}
	}
	else
	{
		// 3. [일반 모드] (타이틀바 형식)

		// (선택 사항) 폰트를 키워서 강조
		// ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]); 
		ImGui::Text("Scene: %s", sceneName.c_str());
		// ImGui::PopFont();

		ImGui::SameLine();

		// ---------------------------------------------------------------------
		// 수정 버튼 (펜 아이콘)
		// ---------------------------------------------------------------------
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));

#ifdef ICON_FA_PEN
		const char* editIcon = ICON_FA_PEN;
#else
		const char* editIcon = "(E)";
#endif
		// 작은 버튼
		if (ImGui::Button(editIcon, ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight())))
		{
			// [핵심] 리네임 시작 트리거
			m_RenamingId = sceneId;                 // ID 등록
			strcpy_s(m_RenameBuffer, sceneName.c_str()); // 이름 복사
		}

		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rename Scene");

		ImGui::PopStyleColor(3);
	}

	ImGui::Separator();
}

void HierarchyPanel::CreateEmptyObject(Scene* scene)
{
	GameObject* newGameObject = GameObject::Create();
	newGameObject->SetName(L"New GameObject");
	newGameObject->AddComponent<Transform>();
	scene->AddGameObject(newGameObject);
	Safe_Release(newGameObject);
}

void HierarchyPanel::CreatePrimitive(Scene* scene, const wstring& name, const wstring& meshName)
{

}
