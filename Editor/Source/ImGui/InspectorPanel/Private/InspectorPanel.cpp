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
#include <algorithm>
#include <string_view>
#include <vector>

InspectorPanel::InspectorPanel(uint32 panelId) : m_PanelId(panelId)
{
	m_Name = panelId == 0 ? L"Inspector" : L"Inspector " + std::to_wstring(panelId + 1);
	m_DisplayNameKey = "UI_INSPECTOR";
}

void InspectorPanel::Draw()
{
	if (!m_Open) return;

	const string windowTitle = m_PanelId == 0 ?
		LOCAL("UI_INSPECTOR") + "###InspectorPanel" :
		fmt::format("{} #{}###InspectorPanel_{}", LOCAL("UI_INSPECTOR"), m_PanelId + 1, m_PanelId);

	if (ImGui::Begin(windowTitle.c_str(), &m_Open))
	{
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
		if (!m_IsLocked)
			targetEntity = SelectionManager::Get().GetPrimarySelectedEntity();

		// 2. 상단 고정 헤더 바 (Sticky Header: 스크롤되지 않음)
		DrawHeaderBar(targetEntity);

		// 3. 스크롤 가능한 본문 영역 (ImGui::BeginChild)
		if (ImGui::BeginChild("InspectorPropertiesScrollArea", ImVec2(0, 0), false, ImGuiWindowFlags_None))
		{
			if (targetEntity != nullptr)
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

void InspectorPanel::DrawHeaderBar(Engine::Entity* currentEntity)
{
	const char* lockIcon = m_IsLocked ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN;
	const float btnSize = ImGui::GetFrameHeight();
	const float spacing = ImGui::GetStyle().ItemSpacing.x;

	if (currentEntity != nullptr && currentEntity->GetScene() != nullptr)
	{
		auto& world = currentEntity->GetScene()->GetRegistry();
		const auto handle = currentEntity->GetEntityHandle();

		// 1. 활성/비활성 체크박스 (FlagComponent)
		if (auto* flag = world.try_get<Engine::FlagComponent>(handle))
		{
			bool active = flag->IsActive();
			if (ImGui::Checkbox("##EntityActive", &active))
				flag->SetActive(active);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Active State");
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
		ImGui::SetTooltip("%s", m_IsLocked ? "Locked to current entity" : "Unlocked (follows selection)");

	ImGui::Separator();
}

EResult InspectorPanel::SetSelectedEntity(Engine::Entity* entity)
{
	SelectionManager::Get().SetSelectedEntity(entity);
	return EResult::Success;
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

	const auto result = scene->ForEachReflectedComponent(entity,
		[&](const reflection::TypeInfo& type, const reflection::ObjectView& object)
		{
			if (!ReflectionPropertyDrawer::IsVisible(type)) return true;
			// NameComponent와 IDComponent는 최상단 Sticky Header에 고정되어 있으므로 아래 목록에서는 제외
			if (object.Is<Engine::NameComponent>() || object.Is<Engine::IDComponent>())
				return true;
			++componentCount;

			Engine::TransformComponent* transform = nullptr;

			if (object.Is<Engine::TransformComponent>() && !object.IsReadOnly())
			{
				transform = scene->GetRegistry().try_get<Engine::TransformComponent>(entity.GetEntityHandle());
			}

			if (transform != nullptr)
			{
				const glm::quat previousRotation = transform->rotation;
				const glm::vec3 previousEuler = transform->eulerRotation;

				// 기존 코드의 직접 대입을 표시 전에 반영합니다.
				if (!transform->SynchronizeRotation())
				{
					ImGui::TextDisabled("%s", (LOCAL("Inspector.RotationRestored")).c_str());
				}

				refreshWorldMatrices |= transform->rotation != previousRotation ||
					transform->eulerRotation != previousEuler;
			}

			auto& componentSettings = entityEditState.Components[type.QualifiedName];
			bool changed = false;
			const auto drawCollider = [&]<typename T>()
			{
				auto& current = world.get<T>(handle);
				T candidate = current;
				if (!ReflectionPropertyDrawer::DrawObject(type, reflection::ObjectView::From(candidate),
					registry, &componentSettings)) return false;
				if (!TryCommitInspectorCollider(current, std::move(candidate)))
				{
					ImGui::TextDisabled("%s", LocalizeInspectorText("UI_INVALID_PROPERTY").c_str());
					return false;
				}
				return true;
			};
			const auto drawUI = [&]<typename T>()
			{
				auto& current = world.get<T>(handle);
				T candidate = current;
				if (!ReflectionPropertyDrawer::DrawObject(type, reflection::ObjectView::From(candidate),
					registry, &componentSettings)) return false;
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
					registry, &componentSettings))
				{
					changed = TransformPropertyEditor::ApplyCandidate(world, handle, candidate);
					if (!changed) ImGui::TextDisabled("%s", LOCAL("UI_TRANSFORM_REJECTED").c_str());
				}
			}
			else if (object.Is<Engine::CanvasComponent>()) changed = drawUI.operator()<Engine::CanvasComponent>();
			else if (object.Is<Engine::RectTransformComponent>()) changed = drawUI.operator()<Engine::RectTransformComponent>();
			else if (object.Is<Engine::UIImageComponent>()) changed = drawUI.operator()<Engine::UIImageComponent>();
			else if (object.Is<Engine::UIButtonComponent>()) changed = drawUI.operator()<Engine::UIButtonComponent>();
			else if (object.Is<Engine::ColliderComponent>()) changed = drawCollider.operator()<Engine::ColliderComponent>();
			else if (object.Is<Engine::Collider2DComponent>()) changed = drawCollider.operator()<Engine::Collider2DComponent>();
			else if (object.Is<Engine::AudioSourceComponent>())
			{
				changed = ReflectionPropertyDrawer::DrawObject(type, object, registry, &componentSettings);
				auto& audioSource = world.get<Engine::AudioSourceComponent>(handle);
				ImGui::Spacing();
				if (audioSource.IsPlaying())
				{
					ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.f), "Playing...");
					ImGui::SameLine();
					if (ImGui::Button("Stop##AudioSource")) audioSource.Stop();
					ImGui::SameLine();
					if (ImGui::Button("Pause##AudioSource")) audioSource.Pause();
				}
				else
				{
					ImGui::TextDisabled("Stopped");
					ImGui::SameLine();
					if (ImGui::Button("Play##AudioSource")) audioSource.Play();
				}
			}
			else changed = ReflectionPropertyDrawer::DrawObject(type, object, registry, &componentSettings);
			if (changed && object.Is<Engine::CameraComponent>()) world.get<Engine::CameraComponent>(handle).InvalidateCache();
			if (changed && object.Is<Engine::LightComponent>()) world.get<Engine::LightComponent>(handle).InvalidateCache();
			anyChanged |= changed;

			if (changed && transform != nullptr)
			{
				// Reflection의 Euler 대입을 공통 회전 API로 연결합니다.
				if (!transform->SynchronizeRotation())
				{
					ImGui::TextDisabled("%s", (LOCAL("Inspector.RotationRejected")).c_str());
				}

				// 위치와 크기 편집도 월드 행렬에 반영합니다.
				refreshWorldMatrices = true;
			}

			return true;
		});
	DrawAddComponentButton(entity);
	ImGui::EndDisabled();

	ImGui::PopID();
	ImGui::PopID();

	if (refreshWorldMatrices)
	{
		Engine::TransformSystem::UpdateHierarchy(scene->GetRegistry());
	}
	if (anyChanged)
		history.Record(std::move(before), SceneEditSnapshot::Capture(*scene, targets), "UI_HISTORY_PROPERTIES", true);

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
	if (ImGui::Button(LOCAL("UI_ADD_COMPONENT").c_str(), buttonSize))
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
                    history.Record(std::move(before), SceneEditSnapshot::Capture(*scene, targets), "UI_HISTORY_COMPONENT");
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