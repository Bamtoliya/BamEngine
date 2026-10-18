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

void InspectorPanel::Draw()
{
	if (!m_Open) return;

	const string windowTitle = LOCAL("UI_INSPECTOR") + "###InspectorPanel";

	if (ImGui::Begin(windowTitle.c_str(), &m_Open))
	{
		Application::Get().GetEditHistory().AllowShortcuts(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
		if (Engine::Entity* entity = SelectionManager::Get().GetPrimarySelectedEntity())
		{
			DrawEntityProperties(*entity);
		}
		else
		{
			ImGui::TextDisabled("%s", (LOCAL("Inspector.NoSelection")).c_str());
		}
	}

	ImGui::End();
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
	const auto* transformType = registry ? registry->FindType<Engine::TransformComponent>() : nullptr;
	if (transformType && world.all_of<Engine::TransformComponent>(handle))
	{
		auto& settings = entityEditState.Components[transformType->QualifiedName];
		auto& positionLock = settings.Vectors["position"].Locked;
		auto& rotationLock = settings.Vectors["eulerRotation"].Locked;
		auto& scaleLock = settings.Vectors["scale"].Locked;
		bool all = positionLock && rotationLock && scaleLock;
		ImGui::BeginDisabled(!history.Enabled());
		if (ImGui::Checkbox(LOCAL("UI_TRANSFORM_LOCK_ALL").c_str(), &all))
			positionLock = rotationLock = scaleLock = all;
		ImGui::Checkbox(LOCAL("UI_TRANSFORM_LOCK_POSITION").c_str(), &positionLock);
		ImGui::SameLine();
		ImGui::Checkbox(LOCAL("UI_TRANSFORM_LOCK_ROTATION").c_str(), &rotationLock);
		ImGui::SameLine();
		ImGui::Checkbox(LOCAL("UI_TRANSFORM_LOCK_SCALE").c_str(), &scaleLock);
		ImGui::EndDisabled();
		if (positionLock || rotationLock || scaleLock) settings.ReadOnlyProperties.insert("absolute");
		else settings.ReadOnlyProperties.erase("absolute");
	}
	ImGui::BeginDisabled(!history.Enabled() || !before);

	const auto result = scene->ForEachReflectedComponent(entity,
		[&](const reflection::TypeInfo& type, const reflection::ObjectView& object)
		{
			if (!ReflectionPropertyDrawer::IsVisible(type)) return true;
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
