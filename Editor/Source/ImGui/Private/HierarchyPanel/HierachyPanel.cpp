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
	bool layerOpen = false;

	if (isRenaming)
	{
		// [수정 모드]
		// 1. TreeNode 그리기 (모양 유지를 위해 빈 라벨 사용, ID는 유지)
		char idStr[32]; sprintf_s(idStr, "###Layer_%d", layerIndex);

		// InputText가 겹쳐질 수 있도록 허용
		ImGui::SetNextItemAllowOverlap();
		layerOpen = ImGui::TreeNodeEx(idStr, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_DrawLinesFull);

		// 2. InputText 위치 잡기 (TreeNode 텍스트 위치로 커서 이동)
		ImGui::SameLine();

		// TreeNode의 화살표+패딩 만큼 커서를 이동시켜야 자연스럽습니다.
		ImVec2 textPos = ImGui::GetCursorScreenPos();
		float indent = ImGui::GetTreeNodeToLabelSpacing(); // 화살표 너비
		ImGui::SetCursorScreenPos(ImVec2(textPos.x + indent, textPos.y));

		// 3. InputText 그리기
		// 너비 계산 (전체 - 우측 버튼 영역)
		float w = ImGui::GetContentRegionAvail().x - 60.0f;
		ImGui::SetNextItemWidth(w);

		// 포커스 설정 (수정 모드 진입 직후 한 번만)
		if (ImGui::IsWindowAppearing() || ImGui::IsMouseClicked(0))
		{
			// InputText가 자동으로 잡겠지만, 명시적으로 잡으려면 아래 사용
			ImGui::SetKeyboardFocusHere(0);
		}

		// Enter 키를 누르면 저장(true 반환)
		if (ImGui::InputText("##Rename", s_RenamingBuf, sizeof(s_RenamingBuf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
		{
			// 이름 저장 로직 (string -> wstring 변환 필요)
			layer->SetName(StrToWStr(s_RenamingBuf)); // 실제 API 사용 시 주석 해제
			s_RenamingLayerIdx = -1; // 수정 모드 종료
		}

		// 4. 수정 취소/완료 처리
		// ESC 키: 취소
		if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			s_RenamingLayerIdx = -1;
		}
		// 다른 곳 클릭 시: 저장하고 종료 (원하는 동작에 따라 변경 가능)
		if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0))
		{
			layer->SetName(StrToWStr(s_RenamingBuf)); // 저장하려면 주석 해제
			s_RenamingLayerIdx = -1;
		}
	}
	else
	{
		// [일반 모드]
		char label[256];
		sprintf_s(label, "%s###Layer_%d", layerName.c_str(), layerIndex);

		ImGui::SetNextItemAllowOverlap();
		layerOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_DrawLinesFull);

#pragma region Drag & Drop
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover))
		{
			// 현재 레이어 인덱스를 페이로드로 보냄
			ImGui::SetDragDropPayload("LAYER_PAYLOAD", &layerIndex, sizeof(uint32));
			ImGui::Text("Move Layer %d", layerIndex);
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget())
		{
#pragma region Layer Order
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("LAYER_PAYLOAD"))
			{
				uint32 sourceLayerIndex = *(uint32*)payload->Data;

				// Scene의 ReorderLayer 함수 호출 (Source -> Target 순서 변경)
				if (sourceLayerIndex != layerIndex)
				{
					scene->ReorderLayer(sourceLayerIndex, layerIndex);
				}
			}
#pragma endregion

#pragma region Object
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GAMEOBJECT_PAYLOAD"))
			{
				GameObject* droppedObj = *(GameObject**)payload->Data;

				if (droppedObj)
				{
					Safe_AddRef(droppedObj);
					if (droppedObj->GetParent() != nullptr)
					{
						droppedObj->GetParent()->RemoveChild(droppedObj);
						scene->AddGameObject(droppedObj, layerIndex);
					}
					else
					{
						scene->MoveGameObjectLayer(droppedObj, layerIndex);
					}
					Safe_Release(droppedObj);
				}
			}
#pragma endregion

			ImGui::EndDragDropTarget();
		}
#pragma endregion


		// [수정 버튼 (펜 아이콘)] - 일반 모드일 때만 표시
		ImGui::SameLine();

		float nodeStartX = ImGui::GetItemRectMin().x;
		float arrowSpacing = ImGui::GetTreeNodeToLabelSpacing();
		float textWidth = ImGui::CalcTextSize(layerName.c_str()).x;
		float padding = 10.0f;

		ImGui::SetCursorPosX(nodeStartX + arrowSpacing + textWidth + padding);

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));

#ifdef ICON_FA_PEN
		const char* editIcon = ICON_FA_PEN;
#else
		const char* editIcon = "(E)";
#endif
		// 버튼 클릭 시 수정 모드로 전환
		if (ImGui::Button(editIcon, ImVec2(30.0f, ImGui::GetFrameHeight())))
		{
			s_RenamingLayerIdx = (int)layerIndex;
			strcpy_s(s_RenamingBuf, layerName.c_str()); // 현재 이름을 버퍼에 복사
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rename Layer");

		ImGui::PopStyleColor(3);
	}

#pragma endregion

#pragma region Checkboxes
	ImGui::SameLine();

	// 1. 사이즈 및 위치 계산
	float itemHeight = ImGui::GetFrameHeight();
	float visibleBtnWidth = 30.0f;                  // Visible 버튼 너비
	float activeBoxWidth = itemHeight;              // 체크박스는 보통 정사각형 (높이와 같음)
	float spacing = 5.0f;                           // 컨트롤 간 간격
	float rightPadding = 5.0f;                      // 윈도우 우측 여백
	float windowWidth = ImGui::GetWindowContentRegionMax().x;

	float activeBoxPos = windowWidth - visibleBtnWidth - spacing - activeBoxWidth - rightPadding;
	ImGui::SetCursorPosX(activeBoxPos);

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
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y));

	string gameObjectName = WStrToStr(gameObject->GetName());
	ImGui::AlignTextToFramePadding();
	ImGui::SetNextItemAllowOverlap();
	bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)gameObject->GetID(), nodeFlags, "%s", gameObjectName.c_str());
	ImGui::PopStyleVar();

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
			ImGui::Text("%s (+%d objects)", gameObjectName.c_str(), (int)selectionManager.GetSelectionContext().size() - 1);
		}
		else
		{
			ImGui::Text("%s", gameObjectName.c_str());
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
				if (IsSuccess(SceneManager::Get().GetCurrentScene()->RemoveGameObject(obj)))
				{
					gameObject->AddChild(obj);
				}
				Safe_Release(obj);
			}
		}
		ImGui::EndDragDropTarget();
	}
#pragma endregion

#pragma region Checkboxes
	ImGui::SameLine();

	// 1. 사이즈 및 위치 계산
	float itemHeight = ImGui::GetFrameHeight();
	float visibleBtnWidth = 30.0f;                  // Visible 버튼 너비
	float activeBoxWidth = itemHeight;              // 체크박스는 보통 정사각형 (높이와 같음)
	float spacing = 5.0f;                           // 컨트롤 간 간격
	float rightPadding = 5.0f;                      // 윈도우 우측 여백
	float windowWidth = ImGui::GetWindowContentRegionMax().x;

	float activeBoxPos = windowWidth - visibleBtnWidth - spacing - activeBoxWidth - rightPadding;
	ImGui::SetCursorPosX(activeBoxPos);

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

	if (ImGui::Button(visIcon, ImVec2(visibleBtnWidth, itemHeight)))
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
	static bool s_IsRenamingScene = false;
	static char s_RenamingBuf[256] = "";
	string sceneName = WStrToStr(scene->GetName());

	ImGui::Separator();

	if (s_IsRenamingScene)
	{
		// [수정 모드]
		// 1. 텍스트 박스 너비 계산 (전체 너비 사용)
		float w = ImGui::GetContentRegionAvail().x;
		ImGui::SetNextItemWidth(w);

		// 2. 포커스 자동 설정 (진입 시 즉시 입력 가능하도록)
		if (ImGui::IsWindowAppearing() || ImGui::IsMouseClicked(0))
		{
			ImGui::SetKeyboardFocusHere(0);
		}

		// 3. InputText 그리기
		// Enter 키(true 반환)를 누르면 저장
		if (ImGui::InputText("##RenameScene", s_RenamingBuf, sizeof(s_RenamingBuf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
		{
			scene->SetName(StrToWStr(s_RenamingBuf));
			s_IsRenamingScene = false;
		}

		// 4. 취소/완료 처리
		// ESC 키: 취소
		if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			s_IsRenamingScene = false;
		}
		// 외부 클릭: 저장하고 종료 (일반적인 동작)
		if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0))
		{
			scene->SetName(StrToWStr(s_RenamingBuf));
			s_IsRenamingScene = false;
		}
	}
	else
	{
		// [일반 모드]
		// 1. 씬 이름 출력 (조금 강조하기 위해 폰트가 있다면 PushFont 사용 가능)
		ImGui::Text("Scene: %s", sceneName.c_str());
		ImGui::SameLine();

		// 2. 수정 버튼 (펜 아이콘) 위치 계산
		// 텍스트 바로 옆에 붙이기
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));       // 배경 투명
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f)); // 글자색 회색
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f)); // 호버 효과

		// 아이콘 설정 (FontAwesome이 있다면 ICON_FA_PEN 사용)
#ifdef ICON_FA_PEN
		const char* editIcon = ICON_FA_PEN;
#else
		const char* editIcon = "(E)"; // 텍스트 대체 아이콘
#endif

		// 버튼 클릭 시 수정 모드로 전환
		if (ImGui::Button(editIcon, ImVec2(30.0f, ImGui::GetFrameHeight())))
		{
			s_IsRenamingScene = true;
			strcpy_s(s_RenamingBuf, sceneName.c_str()); // 현재 이름을 버퍼에 복사
		}

		// 툴팁 표시
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Rename Scene");
		}

		ImGui::PopStyleColor(3);
	}
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
