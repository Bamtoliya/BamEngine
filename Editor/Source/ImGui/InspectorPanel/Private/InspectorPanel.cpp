#pragma once
#include "InspectorPanel.h"
#include "Application.h"
#include "SelectionManager.h"
#include "LocalizationManager.h"
#include "Entity.h"
#include "Scene.h"
#include "ReflectionPropertyDrawer.h"
#include "ReflectionInspectorSupport.h"
#include "TransformSystem.h"
#include "TransformPropertyEditor.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "PhysicsComponent.h"
#include "AudioComponents.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "UILayout.h"
#include "ReflectionRegistry.h"
#include "Components/Core/PrefabInstanceComponent.h"
#include "PrefabModeManager.h"
#include "AnimationEvaluator.h"
#include "NotificationManager.h"
#include "SceneManager.h"
#include "Archive/JsonArchive.h"
#include <imgui_internal.h>
#include "FileDialogs.h"
#include "Components/Render/ActionTimelineComponent.h"
#include "AnimationTimelinePanel.h"
#include "SplineComponent.h"
#include "SceneViewportPanel.h"
#include "SplineCurve.h"

InspectorPanel::InspectorPanel(uint32 panelId) : m_PanelId(panelId)
{
	m_Name = panelId == 0 ? L"Inspector" : L"Inspector " + std::to_wstring(panelId + 1);
	m_DisplayNameKey = "Inspector.Title";
}

void InspectorPanel::Draw()
{
	if (!m_Open) return;
	if (m_FocusRequested)
		ImGui::SetNextWindowFocus();
	const string windowTitle = m_PanelId == 0 ?
		LOCAL("Inspector.Title") + "###InspectorPanel" :
		fmt::format("{} #{}###InspectorPanel_{}", LOCAL("Inspector.Title"), m_PanelId + 1, m_PanelId);
	if (BeginPanelWindow(windowTitle.c_str(), m_Open))
	{
		if (m_FocusRequested)
		{
			ImGui::SetWindowFocus();
			m_FocusRequested = false;
		}
		Application::Get().GetEditHistory().AllowShortcuts(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));

		// 1. 타깃 엔티티 결정
		Engine::Entity* targetEntity = nullptr;
		Engine::Scene* currentScene = SceneManager::Get().GetCurrentScene();

		if (m_IsLocked && currentScene && m_LockedEntityId != 0)
		{
			targetEntity = currentScene->FindEntityById(m_LockedEntityId);
			if (!targetEntity)
			{
				m_IsLocked = false;
				m_LockedEntityId = 0;
			}
		}
		std::vector<entt::entity> targets;
		if (!m_IsLocked && currentScene)
		{
			const auto& world = currentScene->GetRegistry();
			for (auto* entity : SelectionManager::Get().GetSelectedEntities())
			{
				if (!entity || entity->GetScene() != currentScene) continue;

				const auto handle = entity->GetEntityHandle();
				if (!world.valid(handle) || currentScene->FindEntity(handle) != entity) continue;
				if (std::find(targets.begin(), targets.end(), handle) == targets.end())
					targets.push_back(handle);
			}
		}

		if (!m_IsLocked)
			targetEntity = targets.empty() ? nullptr : currentScene->FindEntity(targets.back());

		DrawHeaderBar(targetEntity, targets.size());

		if (targetEntity != nullptr && targets.size() <= 1)
			DrawPrefabHeaderBar(*targetEntity);

		// 3. 스크롤 가능한 본문 영역 (ImGui::BeginChild)
		if (ImGui::BeginChild("InspectorPropertiesScrollArea", ImVec2(0, 0), false, ImGuiWindowFlags_None))
		{
			if (targets.size() > 1)
			{
				if (m_CurrentDisplayId != 0)
					m_EntityScrollMap[m_CurrentDisplayId] = ImGui::GetScrollY();

				m_CurrentDisplayId = 0;
				DrawCommonComponents(*currentScene, targets);
			}
			else if (targetEntity != nullptr)
			{
				const uint64 entityHandleId = static_cast<uint64>(targetEntity->GetEntityHandle());

				// 엔티티가 바뀔 때 스크롤 복원
				if (m_CurrentDisplayId != entityHandleId)
				{
					if (m_CurrentDisplayId != 0)
						m_EntityScrollMap[m_CurrentDisplayId] = ImGui::GetScrollY();

					if (auto it = m_EntityScrollMap.find(entityHandleId); it != m_EntityScrollMap.end())
						ImGui::SetScrollY(it->second);
					else
						ImGui::SetScrollY(m_LastScrollY);

					m_CurrentDisplayId = entityHandleId;
				}

				DrawEntityProperties(*targetEntity);
				m_LastScrollY = ImGui::GetScrollY();
			}
			else
			{
				m_CurrentDisplayId = 0;
			}
		}
		ImGui::EndChild();
	}

	ImGui::End();
}

void InspectorPanel::DrawHeaderBar(Engine::Entity* currentEntity, std::size_t selectionCount)
{
	const char* lockIcon = m_IsLocked ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN;
	const float btnSize = ImGui::GetFrameHeight();
	const float spacing = ImGui::GetStyle().ItemSpacing.x;

	if (selectionCount > 1)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(LOCAL_FMT("Inspector.MultipleSelection", selectionCount).c_str());
		ImGui::SameLine();
	}
	else if (currentEntity != nullptr && currentEntity->GetScene() != nullptr)
	{
		auto& world = currentEntity->GetScene()->GetRegistry();
		const auto handle = currentEntity->GetEntityHandle();

		// 1. 활성/비활성 체크박스 (FlagComponent)
		if (auto* flag = world.try_get<Engine::FlagComponent>(handle))
		{
			bool active = flag->IsActive();
			if (ImGui::Checkbox("##EntityActive", &active))
				flag->SetActive(active);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Inspector.ActiveStateTooltip"));
			ImGui::SameLine();
		}

		// 2. 우측 자물쇠 및 ID 영역 크기 미리 계산
		uint64 entityIdNum = 0;
		if (const auto* idComp = world.try_get<Engine::IDComponent>(handle))
			entityIdNum = idComp->id;

		const std::string idText = fmt::format("ID: {}", entityIdNum);
		const float idTextWidth = ImGui::CalcTextSize(idText.c_str()).x;
		const float rightReservedWidth = btnSize + spacing + idTextWidth + spacing;
		const float availWidth = ImGui::GetContentRegionAvail().x;
		const float nameInputWidth = std::max(60.0f, availWidth - rightReservedWidth);

		// 3. 엔티티 이름 입력 필드 (NameComponent)
		if (auto* nameComp = world.try_get<Engine::NameComponent>(handle))
		{
			std::string nameStr = Engine::WStrToStr(nameComp->name);
			char nameBuf[128];
			strncpy_s(nameBuf, nameStr.c_str(), sizeof(nameBuf));

			ImGui::SetNextItemWidth(nameInputWidth);
			if (ImGui::InputText("##EntityName", nameBuf, sizeof(nameBuf)))
				nameComp->name = Engine::StrToWStr(nameBuf);
			ImGui::SameLine();
		}

		// 4. ID 표시 (버튼 높이와 세로 정렬 맞춤 + SameLine 추가)
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%s", idText.c_str());
		ImGui::SameLine();
	}
	else
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.NoSelection"));
		ImGui::SameLine();
	}

	// 5. 우측 끝 자물쇠 버튼
	const float finalAvail = ImGui::GetContentRegionAvail().x;
	if (finalAvail > btnSize)
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + finalAvail - btnSize);

	const bool wasLocked = m_IsLocked;
	if (wasLocked) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.1f, 0.8f));

	if (ImGui::Button(lockIcon, ImVec2(btnSize, btnSize)))
	{
		m_IsLocked = !m_IsLocked;
		if (m_IsLocked && currentEntity && currentEntity->GetScene())
		{
			if (const auto* id = currentEntity->GetScene()->GetRegistry().try_get<Engine::IDComponent>(currentEntity->GetEntityHandle()))
				m_LockedEntityId = id->id;
		}
		else
		{
			m_LockedEntityId = 0;
		}
	}

	if (wasLocked) ImGui::PopStyleColor();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", m_IsLocked ? LOCAL_CSTR("Inspector.LockedTooltip") : LOCAL_CSTR("Inspector.UnlockedTooltip"));

	ImGui::Separator();
}

void InspectorPanel::DrawPrefabHeaderBar(Engine::Entity& entity)
{
	auto* scene = entity.GetScene();
	if (!scene) return;

	auto& world = scene->GetRegistry();
	const auto handle = entity.GetEntityHandle();

	// 부모를 거슬러 올라가 PrefabInstanceComponent를 가진 루트 탐색
	entt::entity cur = handle;
	entt::entity rootHandle = entt::null;
	while (cur != entt::null)
	{
		if (world.all_of<Engine::PrefabInstanceComponent>(cur))
		{
			rootHandle = cur;
			break;
		}
		const auto* link = world.try_get<Engine::HierarchyComponent>(cur);
		cur = link ? link->parent : entt::null;
	}

	if (rootHandle == entt::null) return; // 프리팹 소속이 아님

	auto* prefabComp = world.try_get<Engine::PrefabInstanceComponent>(rootHandle);
	if (!prefabComp) return;

	Engine::Entity* rootEntity = scene->FindEntity(rootHandle);
	if (!rootEntity) return;

	const bool isRoot = (rootHandle == handle);
	std::filesystem::path prefabPath(prefabComp->prefabAssetPath);
	std::string prefabFileName = Engine::WStrToStr(prefabPath.filename().wstring());
	if (prefabFileName.empty()) prefabFileName = LOCAL("Prefab.MissingAsset");

	ImGui::Spacing();
	ImGui::AlignTextToFramePadding();

	// 루트 엔티티는 "Prefab: 에셋명", 자식 엔티티는 "(Child of Prefab: 에셋명)"으로 명확히 구분
	if (isRoot)
		ImGui::TextColored(ImVec4(0.35f, 0.78f, 1.0f, 1.0f), ICON_FA_CUBES " %s", prefabFileName.c_str());
	else
		ImGui::TextColored(ImVec4(0.35f, 0.78f, 1.0f, 0.75f), ICON_FA_CUBE " %s",
			LOCAL_FMT("Prefab.ChildOf", prefabFileName).c_str());
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", Engine::WStrToStr(prefabComp->prefabAssetPath).c_str());
	// [반응형 레이아웃]: 버튼 3개의 총 너비 계산
	const float btnWidth = 65.0f;
	const float overridesBtnWidth = 85.0f;
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float totalBtnsWidth = (btnWidth * 2) + overridesBtnWidth + (spacing * 2);
	const float availWidth = ImGui::GetContentRegionAvail().x;
	// 가용 공간이 충분하면 같은 줄 우측에 정렬하고, 부족하면 다음 줄로 자동 개행(Wrap)
	if (availWidth >= totalBtnsWidth + spacing)
	{
		ImGui::SameLine();
		const float rightOffset = availWidth - totalBtnsWidth;
		if (rightOffset > 0.0f)
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + rightOffset);
	}
	else
	{
		ImGui::Spacing(); // 줄바꿈 후 약간의 여백 추가
	}
	// [Open] 버튼
	if (ImGui::Button((std::string(ICON_FA_FOLDER_OPEN " ") + LOCAL("Prefab.Open")).c_str(), ImVec2(btnWidth, 0)))
		PrefabModeManager::Get().EnterPrefabMode(prefabComp->prefabAssetPath);
	ImGui::SameLine();
	// [Select] 버튼
	if (ImGui::Button((std::string(ICON_FA_LOCATION_ARROW " ") + LOCAL("Prefab.Select")).c_str(), ImVec2(btnWidth, 0)))
		SelectionManager::Get().SetSelectedAsset(prefabComp->prefabAssetPath);
	ImGui::SameLine();
	// [Overrides] 버튼
	if (ImGui::Button((std::string(LOCAL("Prefab.Overrides")) + " " ICON_FA_CARET_DOWN).c_str(), ImVec2(overridesBtnWidth, 0)))
		ImGui::OpenPopup("PrefabOverridesPopup");

	if (ImGui::BeginPopup("PrefabOverridesPopup"))
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("Prefab.OverridesHeader"));
		ImGui::Separator();

		// 1. Apply All: 현재 인스턴스의 변경사항을 원본 .bamprefab.json에 덮어쓰기
		if (ImGui::MenuItem((std::string(ICON_FA_CHECK " ") + LOCAL("Prefab.ApplyAll")).c_str()))
		{
			archive::JsonArchive ar;
			const Engine::EResult res = Engine::SceneManager::Get().SavePrefab(ar, prefabComp->prefabAssetPath, *rootEntity);

			if (res == Engine::EResult::Success)
				NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.PrefabApplied", prefabFileName), ELogLevel::Info, 3.0f);
			else
				NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.PrefabSaveFailed", prefabFileName), ELogLevel::Error, 4.0f);
		}

		// 2. Revert All: 원본 파일에서 다시 인스턴스화하여 현재 인스턴스 복원
		if (ImGui::MenuItem((std::string(ICON_FA_ROTATE_LEFT " ") + LOCAL("Prefab.RevertAll")).c_str()))
		{
			entt::entity parentH = entt::null;
			if (const auto* link = world.try_get<Engine::HierarchyComponent>(rootHandle))
				parentH = link->parent;

			Engine::Entity* parentEntity = (parentH != entt::null) ? scene->FindEntity(parentH) : nullptr;
			glm::vec3 oldPos{ 0.0f };
			glm::vec3 oldRot{ 0.0f };
			glm::vec3 oldScale{ 1.0f };

			if (const auto* tr = world.try_get<Engine::TransformComponent>(rootHandle))
			{
				oldPos = tr->position;
				oldRot = tr->eulerRotation;
				oldScale = tr->scale;
			}

			archive::JsonArchive ar;
			Engine::Entity* newRoot = nullptr;
			const Engine::EResult res = Engine::SceneManager::Get().InstantiatePrefab(ar, prefabComp->prefabAssetPath, newRoot, parentEntity);

			if (res == Engine::EResult::Success && newRoot)
			{
				if (auto* newTr = scene->GetRegistry().try_get<Engine::TransformComponent>(newRoot->GetEntityHandle()))
				{
					newTr->position = oldPos;
					newTr->eulerRotation = oldRot;
					newTr->scale = oldScale;
					(void)newTr->SynchronizeRotation();
				}

				(void)scene->DestroyEntity(*rootEntity);
				SelectionManager::Get().SetSelectedEntity(newRoot);
				NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL("Notification.PrefabReverted"), ELogLevel::Info, 3.0f);
			}
		}

		ImGui::Separator();

		// 3. Unpack: 프리팹 연결 해제 (루트의 컴포넌트 1개만 제거)
		if (ImGui::MenuItem((std::string(ICON_FA_LINK_SLASH " ") + LOCAL("Prefab.Unpack")).c_str()))
		{
			world.remove<Engine::PrefabInstanceComponent>(rootHandle);
			NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL("Notification.PrefabUnpacked"), ELogLevel::Info, 3.0f);
		}

		ImGui::EndPopup();
	}

	ImGui::Spacing();
	ImGui::Separator();
}

void InspectorPanel::DrawCommonComponents(Engine::Scene& scene, std::span<const entt::entity> entities)
{
	ImGui::TextUnformatted(LOCAL_CSTR("Inspector.CommonComponents"));
	ImGui::Separator();

	if (!Engine::ReflectionRegistry::IsCreated())
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.ReflectionUnavailable"));
		return;
	}

	const auto& world = scene.GetRegistry();
	const auto entries = Engine::ReflectionRegistry::Get().GetComponentEntries();
	std::size_t commonCount = 0;

	for (const auto& entry : entries)
	{
		if (!entry.ReflectionType || !entry.Has) continue;
		if (!ReflectionPropertyDrawer::IsVisible(*entry.ReflectionType)) continue;

		if (entry.Type == std::type_index(typeid(Engine::NameComponent)) ||
			entry.Type == std::type_index(typeid(Engine::IDComponent))) continue;

		const bool common = std::all_of(entities.begin(), entities.end(), [&](entt::entity entity)
			{
				return entry.Has(world, entity);
			});
		if (!common) continue;

		const auto label = LocalizeInspectorText(entry.Name);
		ImGui::BulletText("%s", label.c_str());

		if (entry.Type == std::type_index(typeid(Engine::FlagComponent)))
		{
			ImGui::PushID(entry.ReflectionType->QualifiedName.c_str());
			DrawCommonFlags(scene, entities);
			ImGui::PopID();
		}
		else if (entry.Type == std::type_index(typeid(Engine::TransformComponent)))
		{
			ImGui::PushID(entry.ReflectionType->QualifiedName.c_str());
			DrawCommonTransformVector(scene, entities, TransformEditMode::Translation);
			DrawCommonTransformVector(scene, entities, TransformEditMode::Rotation);
			DrawCommonTransformVector(scene, entities, TransformEditMode::Scale);
			ImGui::PopID();
		}
		else
		{
			DrawCommonReflectedProperties(scene, entities, entry);
		}

		++commonCount;
	}

	if (commonCount == 0)
		ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.NoCommonComponents"));
}
void InspectorPanel::DrawCommonTransformVector(Engine::Scene& scene, std::span<const entt::entity> entities, TransformEditMode mode)
{
	using Transform = Engine::TransformComponent;
	if (entities.empty()) return;

	glm::vec3 Transform::* member = nullptr;
	const char* labelKey = nullptr;
	const char* hintKey = nullptr;

	switch (mode)
	{
	case TransformEditMode::Translation:
		member = &Transform::position;
		labelKey = "PROP_POSITION";
		hintKey = "Inspector.MultiPositionHint";
		break;
	case TransformEditMode::Rotation:
		member = &Transform::eulerRotation;
		labelKey = "PROP_EULER_ROTATION";
		hintKey = "Inspector.MultiRotationHint";
		break;
	case TransformEditMode::Scale:
		member = &Transform::scale;
		labelKey = "PROP_SCALE";
		hintKey = "Inspector.MultiScaleHint";
		break;
	default:
		return;
	}

	auto& world = scene.GetRegistry();
	auto& history = Application::Get().GetEditHistory();
	std::string selectionId = std::to_string(history.Revision());

	for (const auto entity : entities)
	{
		const auto* identity = world.try_get<Engine::IDComponent>(entity);
		if (!identity || identity->id == 0)
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.MultiEditUnavailable"));
			return;
		}
		selectionId += "/" + std::to_string(identity->id);
	}

	ImGui::PushID(static_cast<const void*>(&scene));
	ImGui::PushID(selectionId.c_str());
	ImGui::PushID(static_cast<int>(mode));
	ImGui::TextUnformatted(LOCAL_CSTR(labelKey));
	ImGui::TextWrapped("%s", LOCAL_CSTR(hintKey));

	constexpr const char* axes[] = { "X", "Y", "Z" };
	for (int axis = 0; axis < 3; ++axis)
	{
		ImGui::PushID(axis);

		const float first = (world.get<Transform>(entities.front()).*member)[axis];
		const bool mixed = std::any_of(entities.begin(), entities.end(), [&](entt::entity entity)
			{
				return (world.get<Transform>(entity).*member)[axis] != first;
			});

		char buffer[64]{};
		if (!mixed)
		{
			const auto text = fmt::format("{:.9g}", first);
			std::copy(text.begin(), text.end(), buffer);
		}

		const auto settings = TransformPropertyEditor::GetCommonAxisSettings(world, entities, mode, axis);
		const bool canEdit = history.Enabled() && settings.has_value();
		auto* storage = ImGui::GetStateStorage();
		const ImGuiID errorId = ImGui::GetID("Error");

		ImGui::TextUnformatted(axes[axis]);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::BeginDisabled(!canEdit);

		const auto flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll |
			ImGuiInputTextFlags_CharsScientific;
		const bool requested = ImGui::InputTextWithHint("##Value", LOCAL_CSTR("Inspector.MixedValues"),
			buffer, sizeof(buffer), flags);

		if (!settings && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("%s", LOCAL_CSTR("Inspector.MultiTransformPolicyHint"));

		ImGui::EndDisabled();

		if (canEdit && requested)
		{
			storage->SetInt(errorId, 0);
			const char* begin = buffer;
			const char* end = buffer + std::strlen(buffer);
			if (begin != end && *begin == '+') ++begin;

			float value = 0.0f;
			const auto parsed = std::from_chars(begin, end, value);

			if (parsed.ec != std::errc{} || parsed.ptr != end)
			{
				storage->SetInt(errorId, 1);
			}
			else
			{
				auto before = SceneEditSnapshot::Capture(scene, entities);
				if (!before)
				{
					storage->SetInt(errorId, 2);
				}
				else
				{
					const auto result = TransformPropertyEditor::SetVectorAxis(world, entities, mode, axis, value);
					if (result == TransformBatchResult::Rejected)
						storage->SetInt(errorId, 1);
					else if (result == TransformBatchResult::Changed)
					{
						history.Record(std::move(before), SceneEditSnapshot::Capture(scene, entities), "History.Command.Transform");

						auto& prefabMode = PrefabModeManager::Get();
						if (prefabMode.IsInPrefabMode() && prefabMode.GetPrefabScene() == &scene)
							prefabMode.MarkDirty();
					}
				}
			}
		}

		const int error = storage->GetInt(errorId, 0);
		if (error == 1) ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.MultiTransformRejected"));
		if (error == 2) ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.MultiEditUnavailable"));

		ImGui::PopID();
	}

	ImGui::PopID();
	ImGui::PopID();
	ImGui::PopID();
}
void InspectorPanel::DrawCommonFlags(Engine::Scene& scene, std::span<const entt::entity> entities)
{
	using Flag = Engine::FlagComponent;
	if (entities.empty()) return;

	auto& world = scene.GetRegistry();
	const auto* type = Engine::ReflectionRegistry::Get().GetRegistry().FindType<Flag>();
	const auto* property = type ? type->FindProperty("flags") : nullptr;
	if (!property) return;

	const auto metadataBool = [&](const char* key, bool fallback)
		{
			const auto* metadata = property->Metadata.Find(key);
			const auto* value = metadata ? std::get_if<bool>(metadata) : nullptr;
			return value ? *value : fallback;
		};

	const bool editable = property->CanWrite() && metadataBool("Editable", false) &&
		!metadataBool("ReadOnly", false);

	const bool locked = std::any_of(entities.begin(), entities.end(), [&](entt::entity entity)
		{
			const auto& settings = GetReflectionEntityEditState(world, entity).Components[type->QualifiedName];
			const auto vector = settings.Vectors.find(property->Name);
			return settings.ReadOnlyProperties.contains(property->Name) ||
				(vector != settings.Vectors.end() && vector->second.Locked);
		});

	auto& history = Application::Get().GetEditHistory();
	auto before = history.Enabled() ? SceneEditSnapshot::Capture(scene, entities) : SceneEditSnapshot{};

	ImGui::TextWrapped("%s", LOCAL_CSTR("Inspector.LocalFlagsHint"));

	const auto drawFlag = [&](const char* labelKey, const char* widgetId, auto getter, auto setter)
		{
			const bool first = (world.get<Flag>(entities.front()).*getter)();
			const bool mixed = std::any_of(entities.begin(), entities.end(), [&](entt::entity entity)
				{
					return (world.get<Flag>(entity).*getter)() != first;
				});

			bool value = mixed ? false : first;
			const bool canEdit = history.Enabled() && before && editable && !locked;

			ImGui::PushID(widgetId);
			ImGui::BeginDisabled(!canEdit);
			ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, mixed);
			const bool requested = ImGui::Checkbox(LOCAL_LABEL(labelKey).c_str(), &value);
			ImGui::PopItemFlag();

			if (mixed && ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", LOCAL_CSTR("Inspector.MixedFlagsHint"));

			ImGui::EndDisabled();
			ImGui::PopID();

			if (!canEdit || !requested) return;

			for (const auto entity : entities)
			{
				auto& flag = world.get<Flag>(entity);
				(flag.*setter)(value);
			}

			scene.UpdateHierarchyFlags();

			auto after = SceneEditSnapshot::Capture(scene, entities);
			history.Record(before, after, "History.Command.EditProperty");
			before = std::move(after);

			auto& prefabMode = PrefabModeManager::Get();
			if (prefabMode.IsInPrefabMode() && prefabMode.GetPrefabScene() == &scene)
				prefabMode.MarkDirty();
		};

	drawFlag("Hierarchy.Active", "Active", &Flag::IsActive, &Flag::SetActive);
	drawFlag("Hierarchy.Visible", "Visible", &Flag::IsVisible, &Flag::SetVisible);

	if (history.Enabled() && !before)
		ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.MultiEditUnavailable"));
}

static bool IsInvalidSplineClose(const Engine::SplineComponent& spline, bool wasClosed)
{
	return !wasClosed && spline.Closed && !spline.IsValid();
}

void InspectorPanel::DrawCommonReflectedProperties(Engine::Scene& scene, std::span<const entt::entity> entities, const Engine::ComponentReflectionEntry& entry)
{
	if (entities.empty() || !entry.ReflectionType || !entry.Edit || !entry.Has) return;
	const auto& type = *entry.ReflectionType;

	const bool allowFloats =
		entry.Type != std::type_index(typeid(Engine::CanvasComponent)) &&
		entry.Type != std::type_index(typeid(Engine::RectTransformComponent)) &&
		entry.Type != std::type_index(typeid(Engine::UIImageComponent)) &&
		entry.Type != std::type_index(typeid(Engine::UIButtonComponent)) &&
		entry.Type != std::type_index(typeid(Engine::ColliderComponent)) &&
		entry.Type != std::type_index(typeid(Engine::Collider2DComponent));

	const bool hasFloats = std::any_of(type.Properties.begin(), type.Properties.end(), [](const auto& property)
		{
			return property.template IsValueType<float>();
		});
	if (!allowFloats && hasFloats)
		ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.MultiFloatValidationRequired"));

	if (std::none_of(type.Properties.begin(), type.Properties.end(), [&](const auto& property)
		{
			return property.template IsValueType<bool>() || (allowFloats && property.template IsValueType<float>());
		})) return;

	auto& world = scene.GetRegistry();
	if (!entry.MatchesStorage(world)) return;

	auto& history = Application::Get().GetEditHistory();
	std::string selectionId = std::to_string(history.Revision());
	std::vector<reflection::ObjectView> objects;
	std::vector<const ReflectionObjectEditSettings*> settings;
	const bool isSpline = entry.Type == std::type_index(typeid(Engine::SplineComponent));
	std::vector<std::pair<Engine::SplineComponent*, bool>> splineStates;

	for (const auto entity : entities)
	{
		if (!world.valid(entity) || !entry.Has(world, entity)) return;
		const auto* identity = world.try_get<Engine::IDComponent>(entity);
		if (!identity || identity->id == 0) return;

		if (isSpline)
		{
			auto& spline = world.get<Engine::SplineComponent>(entity);
			splineStates.emplace_back(&spline, spline.Closed);
		}
		objects.push_back(entry.Edit(world, entity));
		settings.push_back(&GetReflectionEntityEditState(world, entity).Components[type.QualifiedName]);
		selectionId += "/" + std::to_string(identity->id);
	}

	auto before = history.Enabled() ? SceneEditSnapshot::Capture(scene, entities) : SceneEditSnapshot{};

	ImGui::PushID(static_cast<const void*>(&scene));
	ImGui::PushID(selectionId.c_str());
	ImGui::PushID(type.QualifiedName.c_str());
	ImGui::BeginDisabled(!history.Enabled() || !before);

	bool changed = ReflectionPropertyDrawer::DrawCommonBoolProperties(type, objects,
		&Engine::ReflectionRegistry::Get().GetRegistry(), settings);

	if (allowFloats)
	{
		changed |= ReflectionPropertyDrawer::DrawCommonFloatProperties(type, objects,
			&Engine::ReflectionRegistry::Get().GetRegistry(), settings);
	}

	ImGui::EndDisabled();
	ImGui::PopID();
	ImGui::PopID();
	ImGui::PopID();

	if (history.Enabled() && !before)
		ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.MultiEditUnavailable"));

	if (!changed) return;

	const bool rejectedSplineClose = std::any_of(splineStates.begin(), splineStates.end(), [](const auto& state)
		{
			return IsInvalidSplineClose(*state.first, state.second);
		});

	if (rejectedSplineClose)
	{
		for (const auto& [spline, wasClosed] : splineStates)
			spline->Closed = wasClosed;

		NotificationManager::Get().AddNotification(
			LOCAL("STRUCT_SPLINE"), LOCAL("Spline.InvalidData"), ELogLevel::Warn);
		return;
	}

	for (const auto entity : entities)
	{
		if (entry.Type == std::type_index(typeid(Engine::CameraComponent)))
			world.get<Engine::CameraComponent>(entity).InvalidateCache();
		if (entry.Type == std::type_index(typeid(Engine::LightComponent)))
			world.get<Engine::LightComponent>(entity).InvalidateCache();
	}

	history.Record(std::move(before), SceneEditSnapshot::Capture(scene, entities), "History.Command.EditProperty");

	auto& prefabMode = PrefabModeManager::Get();
	if (prefabMode.IsInPrefabMode() && prefabMode.GetPrefabScene() == &scene)
		prefabMode.MarkDirty();
}

EResult InspectorPanel::SetSelectedEntity(Engine::Entity* entity)
{
	SelectionManager::Get().SetSelectedEntity(entity);
	return EResult::Success;
}

static bool TryInitializeAppendedSplinePoint(Engine::SplineComponent& spline)
{
	auto& points = spline.Points;
	if (points.empty() || points.size() > Engine::SplineComponent::MaxControlPoints)
		return false;

	const size_t oldCount = points.size() - 1;
	if (oldCount == 0)
		return true;

	auto& added = points.back();
	const auto& last = points[oldCount - 1];

	if (spline.Closed && oldCount >= 3)
	{
		std::array<glm::vec3, Engine::SplineComponent::MaxControlPoints> positions{};
		for (size_t i = 0; i < oldCount; ++i)
			positions[i] = points[i].Position;

		const double parameter = 1.0 - 0.5 / static_cast<double>(oldCount);
		const auto previousPositions = std::span<const glm::vec3>(positions.data(), oldCount);
		Engine::SplineSample sample;
		if (!Engine::SplineCurve::Evaluate(previousPositions, true, parameter, sample))
			return false;

		added.Position = sample.Position;
		added.RollDegrees = std::lerp(last.RollDegrees, points.front().RollDegrees, 0.5f);
		return std::isfinite(added.RollDegrees);
	}

	glm::dvec3 step(0.0, 0.0, 1.0);
	if (oldCount >= 2 && last.Position != points[oldCount - 2].Position)
		step = glm::dvec3(last.Position) - glm::dvec3(points[oldCount - 2].Position);

	const glm::dvec3 position = glm::dvec3(last.Position) + step;
	const double limit = (std::numeric_limits<float>::max)();
	for (int axis = 0; axis < 3; ++axis)
	{
		if (!std::isfinite(position[axis]) || std::abs(position[axis]) > limit)
			return false;
	}

	added.Position = glm::vec3(position);
	added.RollDegrees = last.RollDegrees;
	return added.Position != last.Position && std::isfinite(added.RollDegrees);
}

void InspectorPanel::DrawEntityProperties(Engine::Entity& entity)
{
	Engine::Scene* scene = entity.GetScene();

	if (scene == nullptr)
	{
		ImGui::TextDisabled("%s", (LOCAL("Inspector.NoScene")).c_str());
		return;
	}

	auto& world = scene->GetRegistry();
	const entt::entity handle = entity.GetEntityHandle();

	if (!world.valid(handle))
	{
		ImGui::TextDisabled("%s", (LOCAL("Inspector.EntityUnavailable")).c_str());
		return;
	}

	auto& entityEditState = GetReflectionEntityEditState(world, handle);

	const auto* registry = Engine::ReflectionRegistry::IsCreated() ?
		&Engine::ReflectionRegistry::Get().GetRegistry() : nullptr;

	ImGui::TextUnformatted(LOCAL_CSTR("Inspector.Components"));
	ImGui::Separator();
	const std::string entityId = std::to_string(entt::to_integral(entity.GetEntityHandle()));

	ImGui::PushID(static_cast<const void*>(scene));
	ImGui::PushID(entityId.c_str());

	std::size_t componentCount = 0;
	bool refreshWorldMatrices = false;
	bool anyChanged = false;
	const std::array targets{ handle };
	auto& history = Application::Get().GetEditHistory();
	auto before = history.Enabled() ? SceneEditSnapshot::Capture(*scene, targets) : SceneEditSnapshot{};
	ImGui::BeginDisabled(!history.Enabled() || !before);

	struct ReflectedItem
	{
		const reflection::TypeInfo* type = nullptr;
		reflection::ObjectView object;
	};
	std::vector<ReflectedItem> items;

	const auto result = scene->ForEachReflectedComponent(entity,
		[&](const reflection::TypeInfo& type, const reflection::ObjectView& object)
		{
			if (!ReflectionPropertyDrawer::IsVisible(type)) return true;
			if (object.Is<Engine::NameComponent>() || object.Is<Engine::IDComponent>()) return true;
			items.push_back({ &type, object });
			return true;
		});

	// 엔티티의 컴포넌트 표시 순서(ComponentOrder) 동기화 및 정렬
	auto& order = entityEditState.ComponentOrder;
	std::erase_if(order, [&](const std::string& name)
		{
			return std::ranges::none_of(items, [&](const auto& item) { return item.type->QualifiedName == name; });
		});
	for (const auto& item : items)
	{
		if (std::ranges::find(order, item.type->QualifiedName) == order.end())
			order.push_back(item.type->QualifiedName);
	}
	std::ranges::sort(items, [&](const auto& a, const auto& b)
		{
			const auto itA = std::ranges::find(order, a.type->QualifiedName);
			const auto itB = std::ranges::find(order, b.type->QualifiedName);
			return itA < itB;
		});

	std::string componentToRemove;
	for (std::size_t i = 0; i < items.size(); ++i)
	{
		const auto& [typePtr, object] = items[i];
		const auto& type = *typePtr;
		++componentCount;

		// 헤더바에서만 띄울 컨텍스트 메뉴 람다 정의
		auto drawHeaderMenu = [&]()
			{
				const bool isRemovable = !object.Is<Engine::TransformComponent>();
				if (ImGui::MenuItem(LOCAL_LABEL("Inspector.MoveUp").c_str(), nullptr, false, i > 0))
				{
					std::swap(order[i], order[i - 1]);
				}
				if (ImGui::MenuItem(LOCAL_LABEL("Inspector.MoveDown").c_str(), nullptr, false, i + 1 < items.size()))
				{
					std::swap(order[i], order[i + 1]);
				}
				ImGui::Separator();
				if (ImGui::MenuItem(LOCAL_LABEL("Inspector.RemoveComponent").c_str(), nullptr, false, isRemovable))
				{
					componentToRemove = type.QualifiedName;
				}
			};

		Engine::TransformComponent* transform = nullptr;
		if (object.Is<Engine::TransformComponent>() && !object.IsReadOnly())
			transform = scene->GetRegistry().try_get<Engine::TransformComponent>(handle);

		if (transform != nullptr)
		{
			const glm::quat previousRotation = transform->rotation;
			const glm::vec3 previousEuler = transform->eulerRotation;
			if (!transform->SynchronizeRotation())
				ImGui::TextDisabled("%s", (LOCAL("Inspector.RotationRestored")).c_str());
			refreshWorldMatrices |= transform->rotation != previousRotation || transform->eulerRotation != previousEuler;
		}

		auto& componentSettings = entityEditState.Components[type.QualifiedName];
		bool changed = false;

		const auto drawCollider = [&]<typename T>()
		{
			auto& current = world.get<T>(handle);
			T candidate = current;
			if (!ReflectionPropertyDrawer::DrawObject(type, reflection::ObjectView::From(candidate),
				registry, &componentSettings, drawHeaderMenu)) return false;
			if (!TryCommitInspectorCollider(current, std::move(candidate)))
			{
				ImGui::TextDisabled("%s", LocalizeInspectorText("Inspector.InvalidValue").c_str());
				return false;
			}
			return true;
		};
		const auto drawUI = [&]<typename T>()
		{
			auto& current = world.get<T>(handle);
			T candidate = current;
			if (!ReflectionPropertyDrawer::DrawObject(type, reflection::ObjectView::From(candidate),
				registry, &componentSettings, drawHeaderMenu)) return false;
			if (!Engine::UILayout::IsValid(candidate))
			{
				ImGui::TextDisabled("%s", (LOCAL("Inspector.LayoutRejected")).c_str());
				return false;
			}
			const T previous = current;
			current = candidate;
			const auto canvas = Engine::UILayout::FindCanvas(world, handle);
			std::vector<Engine::UIElementLayout> layouts;
			if (canvas != entt::null && !Engine::UILayout::Build(world, canvas, layouts))
			{
				current = previous;
				ImGui::TextDisabled("%s", (LOCAL("Inspector.CanvasInvalidated")).c_str());
				return false;
			}
			return true;
		};

		if (transform)
		{
			auto candidate = *transform;
			if (ReflectionPropertyDrawer::DrawObject(type, reflection::ObjectView::From(candidate),
				registry, &componentSettings, drawHeaderMenu))
			{
				changed = TransformPropertyEditor::ApplyCandidate(world, handle, candidate);
				if (!changed) ImGui::TextDisabled("%s", LOCAL("Inspector.TransformRejected").c_str());
			}
		}
		else if (object.Is<Engine::CanvasComponent>()) changed = drawUI.operator() < Engine::CanvasComponent > ();
		else if (object.Is<Engine::RectTransformComponent>()) changed = drawUI.operator() < Engine::RectTransformComponent > ();
		else if (object.Is<Engine::UIImageComponent>()) changed = drawUI.operator() < Engine::UIImageComponent > ();
		else if (object.Is<Engine::UIButtonComponent>()) changed = drawUI.operator() < Engine::UIButtonComponent > ();
		else if (object.Is<Engine::ColliderComponent>()) changed = drawCollider.operator() < Engine::ColliderComponent > ();
		else if (object.Is<Engine::Collider2DComponent>()) changed = drawCollider.operator() < Engine::Collider2DComponent > ();
		else if (object.Is<Engine::AudioSourceComponent>())
		{
			changed = ReflectionPropertyDrawer::DrawObject(type, object, registry, &componentSettings);
			auto& audioSource = world.get<Engine::AudioSourceComponent>(handle);
			ImGui::Spacing();
			if (audioSource.IsPlaying())
			{
				ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.f), "%s", LOCAL_CSTR("AudioPreview.Playing"));
				ImGui::SameLine();
				if (ImGui::Button(LOCAL_LABEL("EditorCommon.Stop").c_str())) audioSource.Stop();
				ImGui::SameLine();
				if (ImGui::Button(LOCAL_LABEL("EditorCommon.Pause").c_str())) audioSource.Pause();
			}
			else
			{
				ImGui::TextDisabled("%s", LOCAL_CSTR("AudioPreview.Stopped"));
				ImGui::SameLine();
				if (ImGui::Button(LOCAL_LABEL("EditorCommon.Play").c_str())) audioSource.Play();
			}
		}
		else if (object.Is<Engine::SpriteAnimationComponent>())
		{
			changed = ReflectionPropertyDrawer::DrawObject(type, object, registry, &componentSettings, drawHeaderMenu);
			auto& anim = world.get<Engine::SpriteAnimationComponent>(handle);
			auto* renderer = world.try_get<Engine::SpriteRendererComponent>(handle);
			ImGui::Spacing();
			ImGui::Separator();
			const int frameCount = static_cast<int>(anim.frames.size());
			if (frameCount <= 0)
			{
				ImGui::TextDisabled("%s", LOCAL_CSTR("SpriteAnimation.NoFrames"));
			}
			else
			{
				// 1. 타임라인 프레임 슬라이더
				int currentFrame = anim.currentFrameIndex;
				ImGui::AlignTextToFramePadding();
				ImGui::Text("%s", LOCAL_CSTR("SpriteAnimation.Timeline"));
				ImGui::SameLine();
				ImGui::SetNextItemWidth(-FLT_MIN); // 남은 가용 폭을 100% 꽉 채움
				const std::string sliderLabel = fmt::format("{} / {}", currentFrame + 1, frameCount);
				if (ImGui::SliderInt("##AnimTimelineSlider", &currentFrame, 0, frameCount - 1, sliderLabel.c_str()))
				{
					anim.SetFrame(currentFrame);
					if (renderer&& currentFrame >= 0 && currentFrame < frameCount)
						renderer->spriteHandle = anim.frames[currentFrame];
					changed = true;
				}
				// 2. 에디터 뷰포트 실시간 미리보기 재생 컨트롤
				static bool s_EditorPreviewActive = false;
				static entt::entity s_PreviewEntity = entt::null;
				const bool isPreviewing = (s_EditorPreviewActive && s_PreviewEntity == handle);
				if (isPreviewing)
				{
					const float dt = ImGui::GetIO().DeltaTime;
					const float fps = anim.framesPerSecond > 0.0f ? anim.framesPerSecond : 12.0f;
					const float frameDuration = std::max(1.0f / fps, 0.001f);
					anim.elapsedTime += dt * std::abs(anim.playRate);
					while (anim.elapsedTime >= frameDuration)
					{
						anim.elapsedTime -= frameDuration;
						anim.currentFrameIndex = (anim.currentFrameIndex + 1) % frameCount;
						if (renderer) renderer->spriteHandle = anim.frames[anim.currentFrameIndex];
					}
					ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "%s", LOCAL_CSTR("SpriteAnimation.Previewing"));
					ImGui::SameLine();
					if (ImGui::Button((ICON_FA_PAUSE " " + LOCAL_LABEL("SpriteAnimation.Pause")).c_str()))
						s_EditorPreviewActive = false;
					ImGui::SameLine();
					if (ImGui::Button((ICON_FA_STOP " " + LOCAL_LABEL("SpriteAnimation.Stop")).c_str()))
					{
						s_EditorPreviewActive = false;
						anim.SetFrame(0);
						if (renderer && !anim.frames.empty()) renderer->spriteHandle = anim.frames[0];
					}
				}
				else
				{
					ImGui::TextDisabled("%s", LOCAL_CSTR("SpriteAnimation.Stopped"));
					ImGui::SameLine();
					if (ImGui::Button((ICON_FA_PLAY " " + LOCAL_LABEL("SpriteAnimation.Preview")).c_str()))
					{
						s_EditorPreviewActive = true;
						s_PreviewEntity = handle;
					}
					ImGui::SameLine();
					if (ImGui::Button((ICON_FA_ROTATE_LEFT " " + LOCAL_LABEL("SpriteAnimation.Reset")).c_str()))
					{
						anim.SetFrame(0);
						if (renderer && !anim.frames.empty()) renderer->spriteHandle = anim.frames[0];
					}
				}
				if (!renderer)
				{
					ImGui::Spacing();
					ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "%s", LOCAL_CSTR("SpriteAnimation.MissingRendererWarning"));
				}
			}
		}
		else if (object.Is<Engine::AnimatorComponent>())
		{
			changed = ReflectionPropertyDrawer::DrawObject(type, object, registry, &componentSettings, drawHeaderMenu);
			auto& animator = world.get<Engine::AnimatorComponent>(handle);
			auto findSkinnedRenderer = [&](auto& self, entt::entity cur) -> Engine::SkinnedMeshRendererComponent*
				{
					if (auto* sr = world.try_get<Engine::SkinnedMeshRendererComponent>(cur)) return sr;
					const auto* hier = world.try_get<Engine::HierarchyComponent>(cur);
					if (!hier) return nullptr;
					for (entt::entity child : hier->children)
					{
						if (!world.valid(child)) continue;
						if (auto* sr = self(self, child)) return sr;
					}
					return nullptr;
				};
			auto* skinnedRenderer = findSkinnedRenderer(findSkinnedRenderer, handle);
			auto* clip = animator.currentClip.Get();
			auto* skeleton = skinnedRenderer ? skinnedRenderer->skeletonHandle.Get() : nullptr;
			ImGui::Spacing();
			ImGui::Separator();

			// ── Animator Controller 에셋 로드 UI ──
			ImGui::Text("%s", LOCAL_CSTR("Animator.ControllerPath"));
			char pathBuf[260] = {};
			std::strncpy(pathBuf, animator.controllerPath.c_str(), sizeof(pathBuf) - 1);
			const float folderBtnSize = ImGui::GetFrameHeight();
			const float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
			ImGui::SetNextItemWidth(std::max(60.0f, ImGui::GetContentRegionAvail().x - folderBtnSize - itemSpacing));
			if (ImGui::InputText("##ControllerPathInput", pathBuf, sizeof(pathBuf)))
			{
				animator.controllerPath = pathBuf;
				changed = true;
			}
			ImGui::SameLine();
			// 파일 탐색기 열기 버튼
			if (ImGui::Button((ICON_FA_FOLDER_OPEN "##BrowseController"), ImVec2(folderBtnSize, folderBtnSize)))
			{
				std::wstring selectedPath;
				const std::vector<FileDialogFilter> filters{
					{ Engine::StrToWStr(LOCAL("FileDialog.AnimatorFiles")), L"*.animator" },
					{ Engine::StrToWStr(LOCAL("FileDialog.JsonFiles")), L"*.json" }
				};
				if (FileDialogs::OpenFileDialog(selectedPath, filters))
				{
					animator.controllerPath = WStrToStr(selectedPath);
					if (animator.LoadController(animator.controllerPath)) changed = true;
				}
			}
			ImGui::Spacing();

			// ▼▼▼ 여기에 삽입 (1004번 라인) ▼▼▼
			if (animator.runtimeState.HasStateMachine())
			{
				ImGui::Spacing();
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("Animator.StateMachine"));
				ImGui::Text("%s: %s", LOCAL_CSTR("Animator.CurrentState"), animator.runtimeState.GetCurrentStateName().c_str());

				if (animator.runtimeState.IsTransitioning())
				{
					const float progress = animator.runtimeState.GetTransitionProgress();
					ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), fmt::format("{:.1f}%", progress * 100.0f).c_str());
				}

				ImGui::Spacing();
				ImGui::TextDisabled("%s", LOCAL_CSTR("Animator.Parameters"));
				auto& params = animator.runtimeState.GetParameters().GetAll();
				for (auto& [paramName, paramData] : params)
				{
					ImGui::PushID(paramName.c_str());
					switch (paramData.type)
					{
					case Engine::EAnimatorParamType::Float:
						if (ImGui::DragFloat(paramName.c_str(), &paramData.floatValue, 0.05f)) changed = true;
						break;
					case Engine::EAnimatorParamType::Int:
						if (ImGui::DragInt(paramName.c_str(), &paramData.intValue)) changed = true;
						break;
					case Engine::EAnimatorParamType::Bool:
						if (ImGui::Checkbox(paramName.c_str(), &paramData.boolValue)) changed = true;
						break;
					case Engine::EAnimatorParamType::Trigger:
						if (ImGui::Button((LOCAL_LABEL("Animator.Trigger") + " (" + paramName + ")").c_str()))
						{
							animator.SetTrigger(paramName);
							changed = true;
						}
						break;
					}
					ImGui::PopID();
				}
				ImGui::Separator();
			}

			if (!clip)
			{
				ImGui::TextDisabled("%s", LOCAL_CSTR("Animator.NoClipAssigned"));
			}
			else
			{
				const float tps = clip->GetTicksPerSecond() > 0.0 ? static_cast<float>(clip->GetTicksPerSecond()) : 25.0f;
				const float durationSec = static_cast<float>(clip->GetDuration() / tps);
				// 1. 타임라인 시간 슬라이더
				float currentTime = animator.currentTime;
				ImGui::AlignTextToFramePadding();
				ImGui::Text("%s", LOCAL_CSTR("Animator.Timeline"));
				ImGui::SameLine();
				ImGui::SetNextItemWidth(-FLT_MIN); // 남은 가용 폭을 100% 꽉 채움
				const std::string sliderLabel = fmt::format("{:.2f}s / {:.2f}s", currentTime, durationSec);
				if (ImGui::SliderFloat("##AnimTimeSlider", &currentTime, 0.0f, durationSec, sliderLabel.c_str()))
				{
					animator.currentTime = currentTime;
					if (skeleton)
					{
						Engine::AnimationEvaluator::EvaluateSkinningMatrices(*clip, *skeleton,
							animator.currentTime, animator.finalBoneMatrices);
					}
					changed = true;
				}
				// 2. 에디터 뷰포트 실시간 미리보기 재생 컨트롤
				static bool s_AnimatorPreviewActive = false;
				static entt::entity s_AnimatorPreviewEntity = entt::null;
				const bool isPreviewing = (s_AnimatorPreviewActive && s_AnimatorPreviewEntity == handle);
				if (isPreviewing)
				{
					const float dt = ImGui::GetIO().DeltaTime;
					animator.currentTime += dt * animator.playRate;
					if (durationSec > 0.0f && animator.isLooping)
						animator.currentTime = std::fmod(animator.currentTime, durationSec);
					if (skeleton)
					{
						Engine::AnimationEvaluator::EvaluateSkinningMatrices(*clip, *skeleton,
							animator.currentTime, animator.finalBoneMatrices);
					}
					ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "%s", LOCAL_CSTR("Animator.Previewing"));
					ImGui::SameLine();
					if (ImGui::Button((ICON_FA_PAUSE " " + LOCAL_LABEL("Animator.Pause")).c_str()))
						s_AnimatorPreviewActive = false;
					ImGui::SameLine();
					if (ImGui::Button((ICON_FA_STOP " " + LOCAL_LABEL("Animator.Stop")).c_str()))
					{
						s_AnimatorPreviewActive = false;
						animator.currentTime = 0.0f;
						if (skeleton)
						{
							Engine::AnimationEvaluator::EvaluateSkinningMatrices(*clip, *skeleton,
								0.0f, animator.finalBoneMatrices);
						}
					}
				}
				else
				{
					ImGui::TextDisabled("%s", LOCAL_CSTR("Animator.Stopped"));
					ImGui::SameLine();
					if (ImGui::Button((ICON_FA_PLAY " " + LOCAL_LABEL("Animator.Preview")).c_str()))
					{
						s_AnimatorPreviewActive = true;
						s_AnimatorPreviewEntity = handle;
					}
					ImGui::SameLine();
					if (ImGui::Button((ICON_FA_ROTATE_LEFT " " + LOCAL_LABEL("Animator.Reset")).c_str()))
					{
						animator.currentTime = 0.0f;
						if (skeleton)
						{
							Engine::AnimationEvaluator::EvaluateSkinningMatrices(*clip, *skeleton,
								0.0f, animator.finalBoneMatrices);
						}
					}
				}
				if (!skinnedRenderer || !skeleton)
				{
					ImGui::Spacing();
					ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "%s", LOCAL_CSTR("Animator.MissingRendererWarning"));
				}
			}
		}
		else if (object.Is<Engine::ParticleEmitterComponent>())
		{
			changed = ReflectionPropertyDrawer::DrawObject(type, object, registry, &componentSettings, drawHeaderMenu);
			auto& emitter = world.get<Engine::ParticleEmitterComponent>(handle);
			ImGui::Spacing();
			ImGui::Separator();

			// 실시간 활성 파티클 수 표시
			ImGui::Text("%s: %zu / %u", LOCAL_CSTR("Particle.ActiveCount"), emitter.particles.size(), emitter.maxParticles);

			// 재생 제어 툴바
			if (ImGui::Button((ICON_FA_PLAY " " + LOCAL_LABEL("Particle.Play")).c_str()))
			{
				emitter.Play();
			}
			ImGui::SameLine();
			if (ImGui::Button((ICON_FA_PAUSE " " + LOCAL_LABEL("Particle.Pause")).c_str()))
			{
				emitter.Pause();
			}
			ImGui::SameLine();
			if (ImGui::Button((ICON_FA_STOP " " + LOCAL_LABEL("Particle.Stop")).c_str()))
			{
				emitter.Stop();
			}
			ImGui::SameLine();
			if (ImGui::Button((ICON_FA_ROTATE_LEFT " " + LOCAL_LABEL("Particle.Reset")).c_str()))
			{
				emitter.Reset();
			}
		}
		else if (object.Is<Engine::ActionTimelineComponent>())
		{
			changed = ReflectionPropertyDrawer::DrawObject(type, object, registry, &componentSettings, drawHeaderMenu);
			auto& timeline = world.get<Engine::ActionTimelineComponent>(handle);
			ImGui::Spacing();
			ImGui::Separator();

			// 1. 에셋 파일 브라우저 UI
			ImGui::Text("%s", LOCAL_CSTR("Timeline.AssetFile"));
			char pathBuf[260] = {};
			std::strncpy(pathBuf, timeline.timelinePath.c_str(), sizeof(pathBuf) - 1);
			const float browseBtnSize = ImGui::GetFrameHeight();
			const float browseSpacing = ImGui::GetStyle().ItemSpacing.x;
			ImGui::SetNextItemWidth(std::max(60.0f, ImGui::GetContentRegionAvail().x - browseBtnSize - browseSpacing));
			if (ImGui::InputText("##TimelinePathInput", pathBuf, sizeof(pathBuf)))
			{
				timeline.timelinePath = pathBuf;
				changed = true;
			}
			ImGui::SameLine();
			if (ImGui::Button((ICON_FA_FOLDER_OPEN "##BrowseTimeline"), ImVec2(browseBtnSize, browseBtnSize)))
			{
				std::wstring selectedPath;
				const std::vector<FileDialogFilter> filters{
					{ Engine::StrToWStr(LOCAL("FileDialog.ActionTimelineFiles")), L"*.actiontimeline.json" },
					{ Engine::StrToWStr(LOCAL("FileDialog.AllFiles")), L"*.*" }
				};
				if (FileDialogs::OpenFileDialog(selectedPath, filters, L"Resources/Animation"))
				{
					timeline.timelinePath = Engine::WStrToStr(selectedPath);
					changed = true;
				}
			}

			// 2. 런타임 제어 툴바 (Play, Pause, Stop)
			ImGui::Spacing();
			if (ImGui::Button((ICON_FA_PLAY " " + LOCAL_LABEL("Particle.Play")).c_str()))
			{
				timeline.Play();
			}
			ImGui::SameLine();
			if (ImGui::Button((ICON_FA_PAUSE " " + LOCAL_LABEL("Particle.Pause")).c_str()))
			{
				timeline.Pause();
			}
			ImGui::SameLine();
			if (ImGui::Button((ICON_FA_STOP " " + LOCAL_LABEL("Particle.Stop")).c_str()))
			{
				timeline.Stop();
			}
			ImGui::SameLine();
			ImGui::TextDisabled(LOCAL_CSTR("Inspector.ElapsedSeconds"), timeline.currentTime);

			// 3. 에디터 타임라인 패널 원클릭 편집 연동 버튼
			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE " ") + LOCAL("Timeline.OpenInEditor")).c_str()))
			{
				if (auto* timelinePanel = AnimationTimelinePanel::GetActivePanel())
				{
					timelinePanel->Open();
					if (!timeline.timelinePath.empty()) timelinePanel->LoadFromFile(timeline.timelinePath);
				}
			}
		}
		else if (object.Is<Engine::SplineComponent>())
		{
			auto& spline = world.get<Engine::SplineComponent>(handle);
			const auto previousPoints = spline.Points;
			const bool previousClosed = spline.Closed;
			const bool wasValid = spline.IsValid();

			changed = ReflectionPropertyDrawer::DrawObject(type, object, registry, &componentSettings, drawHeaderMenu);

			int addedIndex = -1;
			if (spline.Points.size() == previousPoints.size() + 1)
			{
				if (TryInitializeAppendedSplinePoint(spline))
				{
					addedIndex = static_cast<int>(spline.Points.size() - 1);
				}
				else
				{
					spline.Points.pop_back();
					changed = spline.Points != previousPoints || spline.Closed != previousClosed;
					NotificationManager::Get().AddNotification(
						LOCAL("STRUCT_SPLINE"), LOCAL("Spline.PointEditRejected"), ELogLevel::Warn);
				}
			}
			if (IsInvalidSplineClose(spline, previousClosed))
			{
				spline.Closed = previousClosed;
				changed = spline.Points != previousPoints;
				NotificationManager::Get().AddNotification(
					LOCAL("STRUCT_SPLINE"), LOCAL("Spline.InvalidData"), ELogLevel::Warn);
			}
			if (wasValid && spline.Points.size() < previousPoints.size() && !spline.IsValid())
			{
				spline.Points = previousPoints;
				changed = spline.Closed != previousClosed;
				NotificationManager::Get().AddNotification(
					LOCAL("STRUCT_SPLINE"), LOCAL("Spline.InvalidData"), ELogLevel::Warn);
			}
			if (changed)
			{
				int editedIndex = -1;
				bool structureChanged = previousPoints.size() != spline.Points.size();

				if (!structureChanged)
				{
					for (size_t i = 0; i < spline.Points.size(); ++i)
					{
						if (previousPoints[i] == spline.Points[i])
							continue;

						if (editedIndex >= 0)
						{
							structureChanged = true;
							break;
						}

						editedIndex = static_cast<int>(i);
					}
				}

				if (auto* viewport = SceneViewportPanel::GetActiveSceneViewport())
				{
					if (addedIndex >= 0)
						viewport->SelectSplinePoint(scene, handle, addedIndex);
					else if (structureChanged)
						viewport->SelectSplinePoint(scene, handle, -1);
					else if (editedIndex >= 0)
						viewport->SelectSplinePoint(scene, handle, editedIndex);
				}
			}

			if (!spline.IsValid())
				ImGui::TextWrapped("%s", LOCAL_CSTR("Spline.InvalidData"));
		}
		else changed = ReflectionPropertyDrawer::DrawObject(type, object, registry, &componentSettings, drawHeaderMenu);

		if (changed && object.Is<Engine::CameraComponent>()) world.get<Engine::CameraComponent>(handle).InvalidateCache();
		if (changed && object.Is<Engine::LightComponent>()) world.get<Engine::LightComponent>(handle).InvalidateCache();
		anyChanged |= changed;

		if (changed && transform != nullptr)
		{
			if (!transform->SynchronizeRotation()) ImGui::TextDisabled("%s", (LOCAL("Inspector.RotationRejected")).c_str());
			refreshWorldMatrices = true;
		}
	}

	// 컴포넌트 삭제 실행 (Undo/Redo 지원)
	if (!componentToRemove.empty())
	{
		const auto* entry = Engine::ReflectionRegistry::Get().FindComponent(componentToRemove);
		if (entry && entry->Remove)
		{
			entry->Remove(world, handle);
			anyChanged = true;
		}
	}
	DrawAddComponentButton(entity);
	ImGui::EndDisabled();

	ImGui::PopID();
	ImGui::PopID();

	if (refreshWorldMatrices)
	{
		Engine::TransformSystem::UpdateHierarchy(scene->GetRegistry());
	}
	if (anyChanged)
	{
		history.Record(std::move(before), SceneEditSnapshot::Capture(*scene, targets), "History.Command.EditProperties", true);

		auto& prefabMode = PrefabModeManager::Get();
		if (prefabMode.IsInPrefabMode() && prefabMode.GetPrefabScene() == scene)
			prefabMode.MarkDirty();
	}

	if (result == Engine::ComponentVisitResult::InvalidEntity)
	{
		ImGui::TextDisabled("%s", (LOCAL("Inspector.EntityUnavailable")).c_str());
	}
	else if (result == Engine::ComponentVisitResult::NotInitialized)
	{
		ImGui::TextDisabled("%s", (LOCAL("Inspector.ReflectionUnavailable")).c_str());
	}
	else if (componentCount == 0)
	{
		ImGui::TextDisabled("%s", (LOCAL("Inspector.NoReflectedComponents")).c_str());
	}
}

#pragma region Add Component
void InspectorPanel::DrawAddComponentButton(Engine::Entity& entity)
{
	Engine::Scene* scene = entity.GetScene();

	if (scene == nullptr || !scene->GetRegistry().valid(entity.GetEntityHandle()))
		return;

	const ImVec2 buttonSize(ImGui::GetContentRegionAvail().x, 0.0f);
	if (ImGui::Button(LOCAL("Inspector.AddComponent").c_str(), buttonSize))
	{
		ImGui::OpenPopup("EntityAddComponentPopup");
	}

	DrawAddComponentPopup(entity);
}

void InspectorPanel::DrawAddComponentPopup(Engine::Entity& entity)
{
	Engine::Scene* scene = entity.GetScene();

	if (scene == nullptr || !scene->GetRegistry().valid(entity.GetEntityHandle()))
		return;

	if (!ImGui::BeginPopup("EntityAddComponentPopup"))
		return;

    if (!Engine::ReflectionRegistry::IsCreated())
    {
        ImGui::TextDisabled("%s", (LOCAL("Inspector.RegistrationUnavailable")).c_str());
        ImGui::EndPopup();
        return;
    }

    const auto entries = Engine::ReflectionRegistry::Get().GetComponentEntries();
    const auto isVisible = [](const auto& entry)
    {
        return entry.UserAddable && (entry.ReflectionType == nullptr ||
            ReflectionPropertyDrawer::IsVisible(*entry.ReflectionType));
    };
    std::vector<std::string_view> categories;
    for (const auto& entry : entries)
    {
        if (isVisible(entry) && std::ranges::find(categories, entry.Category) == categories.end())
            categories.push_back(entry.Category);
    }

    for (const auto category : categories)
    {
        ImGui::PushID(std::string(category).c_str());
        const std::string label = LocalizeInspectorText(std::string(category));
        if (!ImGui::BeginMenu(label.c_str()))
        {
            ImGui::PopID();
            continue;
        }
        for (const auto& entry : entries)
        {
            if (!isVisible(entry) || entry.Category != category)
                continue;
            auto& world = scene->GetRegistry();
            const auto handle = entity.GetEntityHandle();
            const bool exists = entry.Has(world, handle);
            const std::string entryLabel = LocalizeInspectorText(entry.Name);
            ImGui::PushID(std::string(entry.StorageTypeName).c_str());
            if (ImGui::MenuItem(entryLabel.c_str(), nullptr, exists, !exists))
            {
                auto& history = Application::Get().GetEditHistory();
                const std::array targets{ handle };
                auto before = SceneEditSnapshot::Capture(*scene, targets);
				if (before && entry.TryAdd(world, handle))
				{
					history.Record(std::move(before), SceneEditSnapshot::Capture(*scene, targets), "History.Command.AddComponent");

					auto& prefabMode = PrefabModeManager::Get();
					if (prefabMode.IsInPrefabMode() && prefabMode.GetPrefabScene() == scene)
						prefabMode.MarkDirty();

					ImGui::CloseCurrentPopup();
				}
            }
            ImGui::PopID();
        }
        ImGui::EndMenu();
        ImGui::PopID();
    }

	ImGui::EndPopup();
}
#pragma endregion
