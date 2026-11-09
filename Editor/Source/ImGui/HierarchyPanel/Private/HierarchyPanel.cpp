#pragma once
#include "InputManager.h"
#include "EditorUI.h"

#include "HierarchyPanel.h"
#include "Application.h"
#include "TransformPropertyEditor.h"

#include "ImGuiManager.h"
#include "SelectionManager.h"
#include "ResourceManager.h"

#include "SceneManager.h"
#include "Scene.h"

#include "EntityFactory.h"
#include "WorldComponents.h"
#include "ToolBar.h"

#include "FileDialogs.h"
#include <archive/JsonArchive.h>
#include "NotificationManager.h"
#include "EntityClipboard.h"
#include "PrefabModeManager.h"
#include "PrefabInstanceComponent.h"
#include "SceneViewportPanel.h"
#include "InspectorPanel.h"
#include "UIViewportPanel.h"


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

		// 3. BoundingBox(Axis-Aligned Bounding Box) 교차 검사
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
    const string windowTitle = LOCAL("Hierarchy.Title") + "###HierarchyPanel";
    if (!BeginPanelWindow(windowTitle.c_str(), m_Open))
    {
        ImGui::End();
        return;
    }
    Scene* currentScene = SceneManager::IsCreated() ? SceneManager::Get().GetCurrentScene() : nullptr;
    m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    m_Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    Application::Get().GetEditHistory().AllowShortcuts(m_Focused);
    if (currentScene)
    {
        DrawSceneTitle(currentScene);


		EditorUI::SearchBar("HierarchySearch", m_SearchFilter, sizeof(m_SearchFilter));
		ImGui::Spacing();

		DrawEntityHierarchy(currentScene);
		DrawSceneContextMenu(currentScene);
    }
    else
    {
        m_LastHierarchyScene = nullptr;
        m_LastHierarchyPrimary = entt::null;
		m_RangeAnchorId = 0;
        m_HierarchyMessage.clear();
        ImGui::TextUnformatted(LOCAL_CSTR("EditorDocument.NoSceneLoaded"));
		if (SceneManager::IsCreated() && ImGui::Button(LOCAL_CSTR("EditorDocument.CreateNewScene")))
		{
			ToolBar::NewScene();
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
				SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "History.Command.RenameScene");
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

		// ---------------------------------------------------------------------
		// 엔티티 생성 버튼 (+) : 씬 이름 행 우측 끝에 배치
		// ---------------------------------------------------------------------
		const float buttonSize = ImGui::GetFrameHeight();
		const float rightX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - buttonSize;
		ImGui::SameLine(rightX);

		if (ImGui::Button(ICON_FA_PLUS "##AddEntityBtn", ImVec2(buttonSize, buttonSize)))
		{
			ImGui::OpenPopup("AddEntityPopup");
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Hierarchy.AddEntityTooltip"));

		ImGui::PopStyleColor(3);

		if (ImGui::BeginPopup("AddEntityPopup"))
		{
			DrawEntityCreationMenu(scene);
			ImGui::EndPopup();
		}
	}

	ImGui::Separator();
}

void HierarchyPanel::DrawEntityCreationMenu(Scene* scene)
{
	if (!scene) return;

	if (ImGui::MenuItem((ICON_FA_FILE " " + LOCAL_LABEL("Hierarchy.CreateEmpty")).c_str()))
	{
		try { EntityFactory::CreateEmptyEntity(scene); }
		catch (const std::exception& error) { EDITOR_LOG_ERROR("Entity creation failed: {}", error.what()); m_HierarchyMessage = "Hierarchy.EditFailed"; }
	}
	ImGui::Separator();

	if (ImGui::BeginMenu((ICON_FA_CUBES " " + LOCAL_LABEL("Hierarchy.3DObjects")).c_str()))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.Cube").c_str()))
			EntityFactory::CreatePrimitiveEntity(scene, L"Cube", L"CubeMesh", L"Resources/Material/DefaultMaterial.bammat.json");
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.Sphere").c_str()))
			EntityFactory::CreatePrimitiveEntity(scene, L"Sphere", L"SphereMesh", L"Resources/Material/DefaultMaterial.bammat.json");
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.Cylinder").c_str()))
			EntityFactory::CreatePrimitiveEntity(scene, L"Cylinder", L"CylinderMesh", L"Resources/Material/DefaultMaterial.bammat.json");
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.Quad").c_str()))
			EntityFactory::CreatePrimitiveEntity(scene, L"Quad", L"QuadMesh", L"Resources/Material/DefaultMaterial.bammat.json");
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu((ICON_FA_LIGHTBULB " " + LOCAL_LABEL("Hierarchy.Lights")).c_str()))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.AddDirectionalLight").c_str()))
			EntityFactory::CreateLightEntity(scene, L"Directional Light", L"Directional");
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.AddPointLight").c_str()))
			EntityFactory::CreateLightEntity(scene, L"Point Light", L"Point");
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.AddSpotLight").c_str()))
			EntityFactory::CreateLightEntity(scene, L"Spot Light", L"Spot");
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.AddSkyLight").c_str()))
			EntityFactory::CreateLightEntity(scene, L"Sky Light", L"Sky");
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu((ICON_FA_IMAGE " " + LOCAL_LABEL("Hierarchy.2DObjects")).c_str()))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.Sprite").c_str()))
			EntityFactory::CreateSpriteEntity(scene, L"Sprite", L"Resources/Material/SpriteMaterial.bammat.json");
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu((ICON_FA_WINDOW_MAXIMIZE " " + LOCAL_LABEL("Hierarchy.UI")).c_str()))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.AddCanvas").c_str()))
			EntityFactory::CreateCanvasEntity(scene);
		ImGui::EndMenu();
	}

	if (ImGui::MenuItem((ICON_FA_MOUNTAIN " " + LOCAL_LABEL("Hierarchy.AddTerrain")).c_str()))
	{
		EntityFactory::CreateTerrainEntity(scene);
	}
}

void HierarchyPanel::DrawSceneContextMenu(Scene* scene)
{
	if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		DrawEntityCreationMenu(scene);
		ImGui::Separator();

		const bool hasClipboard = EntityClipboard::HasData();
		if (ImGui::MenuItem((std::string(ICON_FA_PASTE " ") + LOCAL_LABEL("EditorCommon.Paste")).c_str(), ShortcutManager::Get().GetShortcutHint("Edit.Paste"), false, hasClipboard))
		{
			EntityClipboard::Paste(scene, nullptr);
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
	if (changedScene)
	{
		m_HierarchyMessage.clear();
		m_RangeAnchorId = 0;
	}
	if (selected.empty()) m_RangeAnchorId = 0;
    m_LastHierarchyScene = scene;
    m_LastHierarchyPrimary = primary;
    m_RevealEntity = entt::null;
	auto action = m_EntityTree.Draw(*scene, selected, reveal, selection.IsPrimarySelectionLocked(), m_SearchFilter);
	if (action.Clicked != entt::null && !selection.IsPrimarySelectionLocked())
	{
		auto* entity = scene->FindEntity(action.Clicked);
		if (entity)
		{
			bool appliedRange = false;
			auto* anchor = m_RangeAnchorId != 0 ? scene->FindEntityById(m_RangeAnchorId) : nullptr;

			if (action.Range && anchor && selection.IsEntitySelected(anchor))
			{
				const auto& rows = action.VisibleEntities;
				const auto first = std::find(rows.begin(), rows.end(), anchor->GetEntityHandle());
				const auto last = std::find(rows.begin(), rows.end(), action.Clicked);

				if (first != rows.end() && last != rows.end())
				{
					if (!action.Toggle) selection.ClearSelection();

					const int step = first <= last ? 1 : -1;
					for (auto it = first;; it += step)
					{
						selection.AddToEntitySelection(scene->FindEntity(*it));
						if (it == last) break;
					}
					appliedRange = true;
				}
			}

			if (!appliedRange)
			{
				if (action.Toggle) selection.ToggleEntitySelection(entity);
				else selection.SetSelectedEntity(entity);
				const auto* id = scene->GetRegistry().try_get<Engine::IDComponent>(action.Clicked);
				m_RangeAnchorId = id && selection.IsEntitySelected(entity) ? id->id : 0;
			}
			// 계층 패널에서 엔티티를 클릭하면 인스펙터 창/탭을 맨 위(전면)로 올리고 포커스
			if (auto* inspector = dynamic_cast<InspectorPanel*>(ImGuiManager::Get().GetImGuiPanel(L"Inspector")))
			{
				inspector->Open();
			}
		}
	}
	if (!action.DeleteEntities.empty())
	{
		// 상위 부모가 이미 삭제 목록에 있으면 자식은 별도 Destroy에서 제외하여 충돌 방지
		std::unordered_set<entt::entity> deleteSet(action.DeleteEntities.begin(), action.DeleteEntities.end());
		std::vector<Engine::Entity*> rootsToDelete;
		auto& registry = scene->GetRegistry();
		auto& prefabMode = PrefabModeManager::Get();
		if (prefabMode.IsInPrefabMode() && prefabMode.GetPrefabScene() == scene)
		{
			if (const auto* root = prefabMode.GetRootEntity())
			{
				auto current = root->GetEntityHandle();
				std::unordered_set<entt::entity> visited;

				while (current != entt::null)
				{
					if (!visited.insert(current).second || deleteSet.contains(current))
					{
						m_HierarchyMessage = "Hierarchy.PrefabRootDeleteBlocked";
						action.DeleteEntities.clear();
						break;
					}

					const auto* hierarchy = registry.try_get<Engine::HierarchyComponent>(current);
					current = hierarchy ? hierarchy->parent : entt::null;
				}
			}
		}
		for (auto handle : action.DeleteEntities)
		{
			entt::entity current = handle;
			bool hasSelectedAncestor = false;
			while (true)
			{
				const auto* hier = registry.try_get<Engine::HierarchyComponent>(current);
				if (!hier || hier->parent == entt::null) break;
				if (deleteSet.contains(hier->parent))
				{
					hasSelectedAncestor = true;
					break;
				}
				current = hier->parent;
			}
			if (!hasSelectedAncestor)
			{
				if (auto* ent = scene->FindEntity(handle))
					rootsToDelete.push_back(ent);
			}
		}
		if (!rootsToDelete.empty())
		{
			auto& history = Application::Get().GetEditHistory();
			auto before = history.Enabled() ? SceneEditSnapshot::Capture(*scene) : SceneEditSnapshot{};

			if (before)
			{
				bool deleted = false;
				selection.ClearSelection();

				for (auto* ent : rootsToDelete)
				{
					if (scene->DestroyEntity(*ent))
						deleted = true;
				}

				if (deleted)
				{
					history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "History.Command.DeleteEntity");

					auto& prefabMode = PrefabModeManager::Get();
					if (prefabMode.IsInPrefabMode() && prefabMode.GetPrefabScene() == scene)
						prefabMode.MarkDirty();
				}
			}
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
			if (before) history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "History.Command.ReorderEntities");
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
			if (before) history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "History.Command.EditProperty");
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
			if (before) history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "History.Command.EditProperty");
		}
	}
	if (action.SaveAsPrefab != entt::null)
	{
		auto* entity = scene->FindEntity(action.SaveAsPrefab);
		if (entity)
		{
			const auto* nameComp = scene->GetRegistry().try_get<Engine::NameComponent>(action.SaveAsPrefab);
			std::wstring defaultName = (nameComp && !nameComp->name.empty()) ? nameComp->name : L"NewPrefab";
			std::wstring outPath;
			std::vector<FileDialogFilter> filters = {
				{ StrToWStr(LOCAL("FileDialog.PrefabFiles")), L"*.bamprefab.json" },
				{ StrToWStr(LOCAL("FileDialog.JsonFiles")), L"*.json" }
			};
			if (FileDialogs::SaveFileDialog(outPath, filters, defaultName + L".bamprefab.json", L"bamprefab.json", EFileDialogContext::PrefabSave))
			{
				archive::JsonArchive ar;
				const Engine::EResult res = SceneManager::Get().SavePrefab(ar, outPath, *entity);
				if (res == Engine::EResult::Success)
				{
					NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.PrefabSaved", WStrToStr(std::filesystem::path(outPath).filename().wstring())), ELogLevel::Info, 3.0f);
				}
				else
				{
					NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.PrefabSaveFailed", WStrToStr(std::filesystem::path(outPath).filename().wstring())), ELogLevel::Error, 3.0f);
				}
			}
		}
	}
	if (action.UnpackPrefab != entt::null)
	{
		auto& history = Application::Get().GetEditHistory();
		auto before = history.Enabled() ? SceneEditSnapshot::Capture(*scene) : SceneEditSnapshot{};
		scene->GetRegistry().remove<Engine::PrefabInstanceComponent>(action.UnpackPrefab);
		if (before)
			history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "History.Command.UnpackPrefab");
		NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL("Notification.PrefabUnpacked"), ELogLevel::Info, 3.0f);
	}
	if (action.FocusEntity != entt::null)
	{
		if (auto* ent = scene->FindEntity(action.FocusEntity))
			selection.SetSelectedEntity(ent);
		SceneViewportPanel* vp = SceneViewportPanel::GetActiveSceneViewport();
		if (!vp)
			vp = dynamic_cast<SceneViewportPanel*>(ImGuiManager::Get().GetImGuiPanel(L"Scene View"));
		if (vp)
			vp->FocusSelectedEntity();
	}
	if (action.OpenUIEditor != entt::null)
	{
		if (auto* ent = scene->FindEntity(action.OpenUIEditor))
			selection.SetSelectedEntity(ent);
		if (auto* ui = dynamic_cast<UIViewportPanel*>(ImGuiManager::Get().GetImGuiPanel(L"UI View")))
		{
			ui->Focus();
			ui->SetTarget(scene, action.OpenUIEditor);
			ui->FocusCanvas();
		}
	}
	if (action.CreateChild != entt::null)
	{
		auto* parentEntity = scene->FindEntity(action.CreateChild);
		if (parentEntity)
		{
			try
			{
				auto& child = EntityFactory::CreateEmptyEntity(scene, L"ChildEntity");
				(void)scene->ReparentEntity(child, parentEntity);
				selection.SetSelectedEntity(&child);
				m_RevealEntity = child.GetEntityHandle();
			}
			catch (const std::exception& error)
			{
				EDITOR_LOG_ERROR("Child entity creation failed: {}", error.what());
			}
		}
	}
	if (action.Duplicate)
	{
		auto* primary = selection.GetPrimarySelectedEntity();
		Entity* parent = nullptr;
		if (primary)
		{
			const auto* hier = scene->GetRegistry().try_get<Engine::HierarchyComponent>(primary->GetEntityHandle());
			if (hier && hier->parent != entt::null)
				parent = scene->FindEntity(hier->parent);
		}
		if (EntityClipboard::CopySelected(scene))
			EntityClipboard::Paste(scene, parent);
	}
	if (action.Unparent != entt::null)
	{
		auto* child = scene->FindEntity(action.Unparent);
		if (child)
		{
			auto& history = Application::Get().GetEditHistory();
			auto before = history.Enabled() ? SceneEditSnapshot::Capture(*scene) : SceneEditSnapshot{};
			const auto result = scene->ReparentEntity(*child, nullptr);
			if (result == EntityReparentResult::Success)
			{
				if (before) history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "History.Command.Reparent");
				m_RevealEntity = action.Unparent;
			}
		}
	}
	if (!action.DropPrefabFile.empty())
	{
		auto* parentEntity = action.DropPrefabTarget != entt::null ? scene->FindEntity(action.DropPrefabTarget) : nullptr;
		archive::JsonArchive ar;
		Engine::Entity* spawned = nullptr;
		auto& history = Application::Get().GetEditHistory();
		if (!history.Enabled()) return;

		if (action.DropPrefabTarget != entt::null && !parentEntity)
		{
			m_HierarchyMessage = "Hierarchy.StaleEntityDrop";
			return;
		}

		auto before = SceneEditSnapshot::Capture(*scene);
		if (!before)
		{
			m_HierarchyMessage = "History.CaptureFailed";
			return;
		}

		if (SceneManager::Get().InstantiatePrefab(ar, action.DropPrefabFile, spawned, parentEntity) == Engine::EResult::Success && spawned)
		{
			if (before) history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "History.Command.InstantiatePrefab");
			SelectionManager::Get().SetSelectedEntity(spawned);
			NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.PrefabInstantiated", WStrToStr(std::filesystem::path(action.DropPrefabFile).stem().wstring())), ELogLevel::Info, 3.0f);
			m_RevealEntity = spawned->GetEntityHandle();
			m_HierarchyMessage.clear();
		}
		else
		{
			NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"),
				LOCAL_FMT("Notification.PrefabInstantiateFailed",
					WStrToStr(std::filesystem::path(action.DropPrefabFile).filename().wstring())),
				ELogLevel::Error, 3.0f);
		}
	}
    if (action.RejectedDrop)
        m_HierarchyMessage = "Hierarchy.StaleEntityDrop";
    if (action.Move)
    {
        auto& history = Application::Get().GetEditHistory();
        auto before = history.Enabled() ? SceneEditSnapshot::Capture(*scene) : SceneEditSnapshot{};
        auto* child = scene->FindEntity(action.Move->Child);
        auto* parent = scene->FindEntity(action.Move->Parent);
        const bool editLocked = child && TransformPropertyEditor::HasAnyLock(
            scene->GetRegistry(), child->GetEntityHandle());
		const bool canCapture = !history.Enabled() || before;
		const auto result = !editLocked && canCapture && child && (parent || action.Move->Parent == entt::null) ?
            scene->ReparentEntity(*child, parent) : EntityReparentResult::InvalidEntity;
        switch (result)
        {
        case EntityReparentResult::Success:
            history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "History.Command.Reparent");
            m_RevealEntity = action.Move->Child;
            [[fallthrough]];
        case EntityReparentResult::Unchanged: m_HierarchyMessage.clear(); break;
        case EntityReparentResult::Cycle:
            m_HierarchyMessage = "Hierarchy.CycleRejected";
            break;
        case EntityReparentResult::InvalidTransform:
            m_HierarchyMessage = "Hierarchy.PreserveTransformFailed";
            break;
        case EntityReparentResult::DepthLimit:
            m_HierarchyMessage = "Hierarchy.DepthLimitReached"; break;
        default:
            m_HierarchyMessage = "Hierarchy.EditFailed"; break;
        }
        if (editLocked) m_HierarchyMessage = "Inspector.TransformUnlockBeforeReparent";
    }
	if (action.Copy)
	{
		EntityClipboard::CopySelected(scene);
	}
	if (action.Paste)
	{
		EntityClipboard::Paste(scene, nullptr);
	}
	if (action.PasteAsChild != entt::null)
	{
		auto* parentEntity = scene->FindEntity(action.PasteAsChild);
		EntityClipboard::Paste(scene, parentEntity);
	}
    if (!m_HierarchyMessage.empty()) ImGui::TextDisabled("%s", LOCAL_CSTR(m_HierarchyMessage));
	if (action.StartRename != entt::null)
	{
		const auto* nameComp = scene->GetRegistry().try_get<Engine::NameComponent>(action.StartRename);
		std::string currentName = nameComp ? Engine::WStrToStr(nameComp->name) : "Entity";
		m_EntityTree.StartRename(action.StartRename, currentName);
	}
	if (action.RenameEntity != entt::null && !action.NewName.empty())
	{
		auto* nameComp = scene->GetRegistry().try_get<Engine::NameComponent>(action.RenameEntity);
		if (nameComp && nameComp->name != action.NewName)
		{
			auto& history = Application::Get().GetEditHistory();
			auto before = history.Enabled() ? SceneEditSnapshot::Capture(*scene) : SceneEditSnapshot{};
			nameComp->name = action.NewName;
			if (before)
				history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "History.Command.EditProperty");
			auto& prefabMode = PrefabModeManager::Get();
			if (prefabMode.IsInPrefabMode() && prefabMode.GetPrefabScene() == scene)
				prefabMode.MarkDirty();
		}
	}
}

void HierarchyPanel::DrawEntityName(const wstring& name, bool isSelected)
{
}

bool HierarchyPanel::DeleteSelectedEntities(Engine::Scene* scene)
{
	if (!scene) return false;
	auto& selection = SelectionManager::Get();
	if (selection.IsPrimarySelectionLocked()) return false;
	const auto& selectedEntities = selection.GetSelectedEntities();
	if (selectedEntities.empty()) return false;

	std::vector<entt::entity> deleteList;
	for (auto* entity : selectedEntities)
	{
		if (entity && entity->GetScene() == scene)
			deleteList.push_back(entity->GetEntityHandle());
	}
	if (deleteList.empty()) return false;

	std::unordered_set<entt::entity> deleteSet(deleteList.begin(), deleteList.end());
	auto& registry = scene->GetRegistry();
	auto& prefabMode = PrefabModeManager::Get();
	if (prefabMode.IsInPrefabMode() && prefabMode.GetPrefabScene() == scene)
	{
		if (const auto* root = prefabMode.GetRootEntity())
		{
			auto current = root->GetEntityHandle();
			std::unordered_set<entt::entity> visited;
			while (current != entt::null)
			{
				if (!visited.insert(current).second || deleteSet.contains(current))
					return false;
				const auto* hierarchy = registry.try_get<Engine::HierarchyComponent>(current);
				current = hierarchy ? hierarchy->parent : entt::null;
			}
		}
	}

	std::vector<Engine::Entity*> rootsToDelete;
	for (auto handle : deleteList)
	{
		entt::entity current = handle;
		bool hasSelectedAncestor = false;
		while (true)
		{
			const auto* hier = registry.try_get<Engine::HierarchyComponent>(current);
			if (!hier || hier->parent == entt::null) break;
			if (deleteSet.contains(hier->parent)) { hasSelectedAncestor = true; break; }
			current = hier->parent;
		}
		if (!hasSelectedAncestor)
		{
			if (auto* ent = scene->FindEntity(handle))
				rootsToDelete.push_back(ent);
		}
	}
	if (rootsToDelete.empty()) return false;

	auto& history = Application::Get().GetEditHistory();
	auto before = history.Enabled() ? SceneEditSnapshot::Capture(*scene) : SceneEditSnapshot{};
	if (!before) return false;

	bool deleted = false;
	selection.ClearSelection();
	for (auto* ent : rootsToDelete)
	{
		if (scene->DestroyEntity(*ent))
			deleted = true;
	}
	if (deleted)
	{
		history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "History.Command.DeleteEntity");
		if (prefabMode.IsInPrefabMode() && prefabMode.GetPrefabScene() == scene)
			prefabMode.MarkDirty();
	}
	return deleted;
}

void HierarchyPanel::TriggerSelectedEntityRename()
{
	auto& selection = SelectionManager::Get();
	auto* primary = selection.GetPrimarySelectedEntity();
	auto* scene = SceneManager::IsCreated() ? SceneManager::Get().GetCurrentScene() : nullptr;
	if (primary && scene && primary->GetScene() == scene)
	{
		Focus(); // 계층 패널을 전면화하고 포커스를 획득하여 인라인 InputText 활성화 보장
		const auto handle = primary->GetEntityHandle();
		const auto* nameComp = scene->GetRegistry().try_get<Engine::NameComponent>(handle);
		std::string currentName = nameComp ? Engine::WStrToStr(nameComp->name) : "Entity";
		m_EntityTree.StartRename(handle, currentName);
	}
}