#pragma once
#include "InputManager.h"

#include "HierarchyPanel.h"
#include "Application.h"
#include "TransformPropertyEditor.h"

#include "ImGuiManager.h"
#include "SelectionManager.h"
#include "ResourceManager.h"

#include "SceneManager.h"
#include "Scene.h"

#include "EntityFactory.h"
#include "CoreComponents.h"


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

// 2. Scene은 메모리 주소를 ID로 사용
static uintptr_t GetNodeID(Scene* scene)
{
	return (uintptr_t)scene;
}

template<typename T>
bool HierarchyPanel::DrawRenameBox(T* target, ImGuiTreeNodeFlags flags, bool isSelected, function<void()> onDragDrop, function<void()> onContextMenu)
{
	void* targetId = (void*)GetNodeID(target);
	string name = WStrToStr(target->GetName());

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y));
	ImGui::AlignTextToFramePadding();
	if (m_RenamingId == targetId)
	{
		ImGui::Bullet();
		ImGui::SameLine();

		
		if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive() && !MOUSE_BUTTON_DOWN(EMouseButton::Left))
			ImGui::SetKeyboardFocusHere();

		if (ImGui::InputText("##Rename", m_RenameBuffer, sizeof(m_RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
		{
			target->SetName(StrToWStr(m_RenameBuffer));
			m_RenamingId = nullptr;
		}
		else if (KEY_DOWN(EKeyCode::Escape))
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

	if (m_IsBoxSelecting)
	{
		// 1. 현재 노드의 화면상 영역 가져오기
		ImVec2 itemMin = ImGui::GetItemRectMin();
		ImVec2 itemMax = ImGui::GetItemRectMax();

		// 2. 선택 박스 영역 계산
		ImVec2 boxMin = ImVec2(std::min(m_BoxStartPos.x, m_BoxEndPos.x), std::min(m_BoxStartPos.y, m_BoxEndPos.y));
		ImVec2 boxMax = ImVec2(std::max(m_BoxStartPos.x, m_BoxEndPos.x), std::max(m_BoxStartPos.y, m_BoxEndPos.y));

		// 3. AABB(Axis-Aligned Bounding Box) 교차 검사
		// 두 사각형이 겹치는지 확인
		bool overlap = (itemMin.x < boxMax.x && itemMax.x > boxMin.x) &&
			(itemMin.y < boxMax.y && itemMax.y > boxMin.y);

		if (overlap)
		{
			// 1. 시각적 피드백 (하이라이트)
			ImGui::GetWindowDrawList()->AddRectFilled(itemMin, itemMax, IM_COL32(255, 255, 255, 50));
		}
	}

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		
	}

	// [선택 정리 로직] 마우스를 뗐을 때 나머지 선택 해제 (단순 클릭 시)
	if (ImGui::IsItemHovered() && MOUSE_BUTTON_UP(EMouseButton::Right))
	{
		
	}

	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		
	}

	if (onContextMenu)
	{
		onContextMenu();
	}

	if (onDragDrop)
	{
		onDragDrop();
	}

	if (ImGui::IsItemHovered() && MOUSE_BUTTON_DOUBLE_CLICK(EMouseButton::Left))
	{
		m_RenamingId = targetId;                 // 리네임 대상 설정
		strcpy_s(m_RenameBuffer, name.c_str());  // 현재 이름 버퍼에 복사
	}

	// 3. [F2 키 트리거]
	if (isSelected && ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive())
	{
		if (KEY_DOWN(EKeyCode::F2))
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

	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Hierarchy.RenameScene"));

	ImGui::PopStyleColor(3);
	
	ImGui::PopStyleVar();
	return opened;
}

void HierarchyPanel::Draw()
{
    m_EntityTree.ReleaseFinishedDrag();
    if (!m_Open) return;
    const string windowTitle = LOCAL("UI_HIERARCHY") + "###HierarchyPanel";
    if (!ImGui::Begin(windowTitle.c_str(), &m_Open))
    {
        ImGui::End();
        return;
    }
    Scene* currentScene = SceneManager::IsCreated() ? SceneManager::Get().GetCurrentScene() : nullptr;
    Application::Get().GetEditHistory().AllowShortcuts(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
    if (currentScene)
    {
        DrawSceneTitle(currentScene);

		const float clearButtonSize = ImGui::GetFrameHeight();
		const bool hasFilter = m_SearchFilter[0] != '\0';
		if (hasFilter) ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - clearButtonSize - ImGui::GetStyle().ItemSpacing.x);
		else ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##HierarchySearch", LOCAL_CSTR("EditorCommon.Search"), m_SearchFilter, sizeof(m_SearchFilter));
		if (hasFilter)
		{
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_XMARK "##ClearHierarchySearch", ImVec2(clearButtonSize, clearButtonSize)))
				m_SearchFilter[0] = '\0';
		}
		ImGui::Spacing();

        DrawEntityHierarchy(currentScene);
        DrawSceneContextMenu(currentScene);
    }
    else
    {
        m_LastHierarchyScene = nullptr;
        m_LastHierarchyPrimary = entt::null;
        m_HierarchyMessage.clear();
        ImGui::TextUnformatted(LOCAL_CSTR("UI_NO_SCENE_LOADED"));
        if (SceneManager::IsCreated() && ImGui::Button(LOCAL_CSTR("UI_CREATE_NEW_SCENE")))
        {
            const EResult result = SceneManager::Get().NewScene();
            if (result == EResult::Success) SelectionManager::Get().ClearSelection();
            else ENGINE_LOG_ERROR("Failed to create scene: {}", ResultToString(result));
        }
    }
    if (InputManager::Get().IsMouseButtonDown(EMouseButton::Left) && ImGui::IsWindowHovered() &&
        !ImGui::IsAnyItemHovered() && !InputManager::Get().IsCtrlPressed())
        SelectionManager::Get().ClearSelection();
    ImGui::End();
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
		if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive() && !MOUSE_BUTTON_DOWN(EMouseButton::Left))
		{
			ImGui::SetKeyboardFocusHere(0);
		}

		// InputText 그리기
		const bool accepted = ImGui::InputText("##RenameScene", m_RenameBuffer, sizeof(m_RenameBuffer),
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
		if (KEY_DOWN(EKeyCode::Escape))
		{
			m_RenamingId = nullptr;
		}
		else if (accepted || ImGui::IsItemDeactivated())
		{
			const auto name = StrToWStr(m_RenameBuffer);
			if (scene->GetName() != name && Application::Get().GetEditHistory().Enabled())
			{
				SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "UI_HISTORY_RENAME");
				scene->SetName(name);
			}
			m_RenamingId = nullptr;
		}
	}
	else
	{
		// 3. [일반 모드] (타이틀바 형식)

		// (선택 사항) 폰트를 키워서 강조
		// ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]); 
		ImGui::Text("%s", LOCAL_FMT("Hierarchy.SceneName", sceneName.c_str()).c_str());
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

		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Hierarchy.RenameScene"));

		ImGui::PopStyleColor(3);
	}

	ImGui::Separator();
}

void HierarchyPanel::DrawSceneContextMenu(Scene* scene)
{
	if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.AddEntity").c_str()))
		{
			EntityFactory::CreateEmptyEntity(scene);
		}
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.AddCanvas").c_str()))
			SelectionManager::Get().SetSelectedEntity(&EntityFactory::CreateCanvasEntity(scene));
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.AddPrimitive").c_str()))
		{
			try
			{
				Entity& entity = EntityFactory::CreatePrimitiveEntity(scene, L"Cube", L"CubeMesh", L"Resources/Material/GBufferMaterial");
				SelectionManager::Get().SetSelectedEntity(&entity);
			}
			catch (const std::exception& error)
			{
				ENGINE_LOG_ERROR("Failed to create Cube: {}", error.what());
			}
		}
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.AddSprite").c_str()))
		{
			try
			{
				Entity& entity = EntityFactory::CreateSpriteEntity(scene, L"Sprite", L"Resources/Material/SpriteMaterial");
				SelectionManager::Get().SetSelectedEntity(&entity);
			}
			catch (const std::exception& error)
			{
				ENGINE_LOG_ERROR("Failed to create Sprite: {}", error.what());
			}
		}
		if (ImGui::BeginMenu(LOCAL_LABEL("Hierarchy.Light").c_str()))
		{
			if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.AddDirectionalLight").c_str()))
			{
				try
				{
					Entity& entity = EntityFactory::CreateLightEntity(scene, L"Light", L"Directional");
					SelectionManager::Get().SetSelectedEntity(&entity);
				}
				catch (const std::exception& error)
				{
					ENGINE_LOG_ERROR("Failed to create Directional Light: {}", error.what());
				}

			}

			if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.AddPointLight").c_str()))
			{
				try
				{
					Entity& entity = EntityFactory::CreateLightEntity(scene, L"Light", L"Point");
					SelectionManager::Get().SetSelectedEntity(&entity);
				}
				catch (const std::exception& error)
				{
					ENGINE_LOG_ERROR("Failed to create Point Light: {}", error.what());
				}

			}

			if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.AddSpotLight").c_str()))
			{
				try
				{
					Entity& entity = EntityFactory::CreateLightEntity(scene, L"Light", L"Spot");
					SelectionManager::Get().SetSelectedEntity(&entity);
				}
				catch (const std::exception& error)
				{
					ENGINE_LOG_ERROR("Failed to create Spot Light: {}", error.what());
				}

			}

			if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.AddSkyLight").c_str()))
			{
				try
				{
					Entity& entity = EntityFactory::CreateLightEntity(scene, L"Light", L"Sky");
					SelectionManager::Get().SetSelectedEntity(&entity);
				}
				catch (const std::exception& error)
				{
					ENGINE_LOG_ERROR("Failed to create Sky Light: {}", error.what());
				}

			}

			ImGui::EndMenu();
		}
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.AddTerrain").c_str()))
		{
			try
			{
				Entity& entity = EntityFactory::CreateTerrainEntity(scene, L"Terrain");
				SelectionManager::Get().SetSelectedEntity(&entity);
			}
			catch (const std::exception& error)
			{
				ENGINE_LOG_ERROR("Failed to create Terrain: {}", error.what());
			}
		}

		ImGui::EndPopup();
	}
}
void HierarchyPanel::DrawEntityHierarchy(Scene* scene)
{
    if (!scene) return;
    auto& selection = SelectionManager::Get();
    std::vector<entt::entity> selected;
    for (auto* entity : selection.GetSelectedEntities())
        if (entity && entity->GetScene() == scene) selected.push_back(entity->GetEntityHandle());
    const auto primary = selected.empty() ? entt::null : selected.back();
    const bool changedScene = m_LastHierarchyScene != scene;
    const auto reveal = m_RevealEntity != entt::null ? m_RevealEntity :
        changedScene || primary != m_LastHierarchyPrimary ? primary : entt::null;
    if (changedScene) m_HierarchyMessage.clear();
    m_LastHierarchyScene = scene;
    m_LastHierarchyPrimary = primary;
    m_RevealEntity = entt::null;
	const auto action = m_EntityTree.Draw(*scene, selected, reveal, selection.IsPrimarySelectionLocked(), m_SearchFilter);
    if (action.Clicked != entt::null)
    {
        auto* entity = scene->FindEntity(action.Clicked);
        if (action.Toggle) selection.ToggleEntitySelection(entity);
        else selection.SetSelectedEntity(entity);
    }
    if (action.Delete != entt::null)
    {
        auto& history = Application::Get().GetEditHistory();
        auto before = history.Enabled() ? SceneEditSnapshot::Capture(*scene) : SceneEditSnapshot{};
        auto* entity = scene->FindEntity(action.Delete);
        if (before && entity)
        {
            selection.ClearSelection();
            if (scene->DestroyEntity(*entity))
                history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "UI_HISTORY_DELETE");
        }
    }
	if (action.MoveUp != entt::null || action.MoveDown != entt::null)
	{
		const auto target = action.MoveUp != entt::null ? action.MoveUp : action.MoveDown;
		const int dir = action.MoveUp != entt::null ? -1 : 1;
		auto& history = Application::Get().GetEditHistory();
		auto before = history.Enabled() ? SceneEditSnapshot::Capture(*scene) : SceneEditSnapshot{};
		if (scene->MoveSiblingOrder(target, dir))
		{
			if (before) history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "UI_HISTORY_REORDER");
			m_RevealEntity = target;
		}
	}
	if (action.ToggleVisible != entt::null)
	{
		auto* flagComp = scene->GetRegistry().try_get<Engine::FlagComponent>(action.ToggleVisible);
		if (flagComp)
		{
			auto& history = Application::Get().GetEditHistory();
			auto before = history.Enabled() ? SceneEditSnapshot::Capture(*scene) : SceneEditSnapshot{};
			flagComp->ToggleVisible();
			scene->UpdateHierarchyFlags();
			if (before) history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "UI_HISTORY_PROPERTY");
		}
	}
	if (action.ToggleActive != entt::null)
	{
		auto* flagComp = scene->GetRegistry().try_get<Engine::FlagComponent>(action.ToggleActive);
		if (flagComp)
		{
			auto& history = Application::Get().GetEditHistory();
			auto before = history.Enabled() ? SceneEditSnapshot::Capture(*scene) : SceneEditSnapshot{};
			flagComp->SetActive(!flagComp->IsActive());
			scene->UpdateHierarchyFlags();
			if (before) history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "UI_HISTORY_PROPERTY");
		}
	}
    if (action.RejectedDrop)
        m_HierarchyMessage = "UI_HIERARCHY_BAD_DROP";
    if (action.Move)
    {
        auto& history = Application::Get().GetEditHistory();
        auto before = history.Enabled() ? SceneEditSnapshot::Capture(*scene) : SceneEditSnapshot{};
        auto* child = scene->FindEntity(action.Move->Child);
        auto* parent = scene->FindEntity(action.Move->Parent);
        const bool editLocked = child && TransformPropertyEditor::HasAnyLock(
            scene->GetRegistry(), child->GetEntityHandle());
        const auto result = !editLocked && before && child && (parent || action.Move->Parent == entt::null) ?
            scene->ReparentEntity(*child, parent) : EntityReparentResult::InvalidEntity;
        switch (result)
        {
        case EntityReparentResult::Success:
            history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "UI_HISTORY_REPARENT");
            m_RevealEntity = action.Move->Child;
            [[fallthrough]];
        case EntityReparentResult::Unchanged: m_HierarchyMessage.clear(); break;
        case EntityReparentResult::Cycle:
            m_HierarchyMessage = "UI_HIERARCHY_CYCLE";
            break;
        case EntityReparentResult::InvalidTransform:
            m_HierarchyMessage = "UI_HIERARCHY_TRANSFORM";
            break;
        case EntityReparentResult::DepthLimit:
            m_HierarchyMessage = "UI_HIERARCHY_DEPTH"; break;
        default:
            m_HierarchyMessage = "UI_HIERARCHY_EDIT_FAILED"; break;
        }
        if (editLocked) m_HierarchyMessage = "UI_TRANSFORM_UNLOCK_REPARENT";
    }
    if (!m_HierarchyMessage.empty()) ImGui::TextDisabled("%s", LOCAL_CSTR(m_HierarchyMessage));
}

void HierarchyPanel::DrawEntityName(const wstring& name, bool isSelected)
{

}