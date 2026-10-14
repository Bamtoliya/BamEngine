#pragma once
#include "InspectorPanel.h"
#include "SelectionManager.h"
#include "LocalizationManager.h"
#include "Entity.h"
#include "Scene.h"
#include "Reflection/CoreComponentReflection.h"
#include "ReflectionPropertyDrawer.h"
#include "TransformSystem.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "PhysicsComponent.h"
#include "ResourceManager.h"
#include "Texture.h"

void InspectorPanel::Draw()
{
	if (!m_Open) return;

	const string windowTitle = LOCAL("UI_INSPECTOR") + "###InspectorPanel";

	if (ImGui::Begin(windowTitle.c_str(), &m_Open))
	{
		if (Engine::Entity* entity = SelectionManager::Get().GetPrimarySelectedEntity())
		{
			DrawEntityProperties(*entity);
		}
		else
		{
			ImGui::TextDisabled("No Entity selected.");
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
		ImGui::TextDisabled("Entity has no Scene.");
		return;
	}

	auto& world = scene->GetRegistry();
	const entt::entity handle = entity.GetEntityHandle();

	if (!world.valid(handle))
	{
		ImGui::TextDisabled("Entity is no longer available.");
		return;
	}

	auto& entityEditState = world.get_or_emplace<ReflectionEntityEditState>(handle);

	const auto* registry = Engine::GetCoreComponentReflectionRegistry();

	ImGui::TextUnformatted("Components");
	ImGui::Separator();
	const std::string entityId = std::to_string(entt::to_integral(entity.GetEntityHandle()));

	ImGui::PushID(static_cast<const void*>(scene));
	ImGui::PushID(entityId.c_str());

	std::size_t componentCount = 0;
	bool refreshWorldMatrices = false;

	const auto result = scene->ForEachReflectedComponent(entity,
		[&](const reflection::TypeInfo& type, const reflection::ObjectView& object)
		{
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
					ImGui::TextDisabled("Invalid rotation was restored.");
				}

				refreshWorldMatrices |= transform->rotation != previousRotation ||
					transform->eulerRotation != previousEuler;
			}

			auto& componentSettings = entityEditState.Components[type.QualifiedName];
			const bool changed = ReflectionPropertyDrawer::DrawObject(type, object, registry, &componentSettings);

			if (changed && transform != nullptr)
			{
				// Reflection의 Euler 대입을 공통 회전 API로 연결합니다.
				if (!transform->SynchronizeRotation())
				{
					ImGui::TextDisabled("Invalid rotation input was rejected.");
				}

				// 위치와 크기 편집도 월드 행렬에 반영합니다.
				refreshWorldMatrices = true;
			}

			return true;
		});
	DrawAddComponentButton(entity);

	ImGui::PopID();
	ImGui::PopID();

	if (refreshWorldMatrices)
	{
		Engine::TransformSystem::UpdateHierarchy(scene->GetRegistry());
	}

	if (result == Engine::ComponentVisitResult::InvalidEntity)
	{
		ImGui::TextDisabled("Entity is no longer available.");
	}
	else if (result == Engine::ComponentVisitResult::NotInitialized)
	{
		ImGui::TextDisabled("Component reflection is unavailable.");
	}
	else if (componentCount == 0)
	{
		ImGui::TextDisabled("No reflected components.");
	}
}

#pragma region Add Component
namespace
{
	template<typename T>
	void DrawEntityAddComponentMenuItem(Engine::Scene& scene, Engine::Entity& entity,
		const char* label, bool requiresTransform = false)
	{
		const bool exists = scene.HasComponent<T>(entity);

		if (!ImGui::MenuItem(label, nullptr, exists, !exists))
			return;

		scene.AddComponent<T>(entity);

		if (requiresTransform)
		{
			auto& world = scene.GetRegistry();
			const entt::entity handle = entity.GetEntityHandle();

			static_cast<void>(world.get_or_emplace<Engine::TransformComponent>(handle));
			static_cast<void>(world.get_or_emplace<Engine::WorldTransformComponent>(handle));
			static_cast<void>(world.get_or_emplace<Engine::FlagComponent>(handle));

			Engine::TransformSystem::UpdateHierarchy(world);
		}

		ImGui::CloseCurrentPopup();
	}
}

void InspectorPanel::DrawAddComponentButton(Engine::Entity& entity)
{
	Engine::Scene* scene = entity.GetScene();

	if (scene == nullptr || !scene->GetRegistry().valid(entity.GetEntityHandle()))
		return;

	if (ImGui::Button(LOCAL("UI_ADD_COMPONENT").c_str()))
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

	if (ImGui::BeginMenu("Core"))
	{
		DrawEntityAddComponentMenuItem<Engine::NameComponent>(*scene, entity, "Name");
		DrawEntityAddComponentMenuItem<Engine::TagComponent>(*scene, entity, "Tag");
		DrawEntityAddComponentMenuItem<Engine::FlagComponent>(*scene, entity, "Flags");
		DrawEntityAddComponentMenuItem<Engine::TransformComponent>(*scene, entity, "Transform", true);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Rendering"))
	{
		DrawEntityAddComponentMenuItem<Engine::CameraComponent>(*scene, entity, "Camera", true);
		DrawEntityAddComponentMenuItem<Engine::LightComponent>(*scene, entity, "Light", true);
		DrawEntityAddComponentMenuItem<Engine::StaticMeshRendererComponent>(*scene, entity, "Static Mesh Renderer", true);
		DrawEntityAddComponentMenuItem<Engine::SpriteRendererComponent>(*scene, entity, "Sprite Renderer", true);
		DrawEntityAddComponentMenuItem<Engine::SkinnedMeshRendererComponent>(*scene, entity, "Skinned Mesh Renderer", true);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Physics 3D"))
	{
		DrawEntityAddComponentMenuItem<Engine::RigidBodyComponent>(*scene, entity, "Rigid Body", true);
		DrawEntityAddComponentMenuItem<Engine::ColliderComponent>(*scene, entity, "Collider", true);
		DrawEntityAddComponentMenuItem<Engine::JointComponent>(*scene, entity, "Joint", true);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Physics 2D"))
	{
		DrawEntityAddComponentMenuItem<Engine::RigidBody2DComponent>(*scene, entity, "Rigid Body 2D", true);
		DrawEntityAddComponentMenuItem<Engine::Collider2DComponent>(*scene, entity, "Collider 2D", true);
		DrawEntityAddComponentMenuItem<Engine::Joint2DComponent>(*scene, entity, "Joint 2D", true);
		ImGui::EndMenu();
	}

	ImGui::EndPopup();
}
#pragma endregion

#pragma region Component
namespace
{
	// ---------- Sampler 콤보 ----------
	static bool DrawSamplerFilterCombo(const char* id, Engine::ESamplerFilter& value)
	{
		const char* labels[] = { "Point", "Linear", "Anisotropic" };
		int cur = static_cast<int>(value);
		bool changed = false;
		ImGui::PushID(id);
		if (ImGui::BeginCombo("##Filter", labels[cur]))
		{
			for (int i = 0; i < 3; ++i)
			{
				bool sel = (cur == i);
				if (ImGui::Selectable(labels[i], sel))
				{
					value = static_cast<Engine::ESamplerFilter>(i);
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();
		return changed;
	}

	static bool DrawSamplerAddressCombo(const char* id, Engine::ESamplerAddressMode& value)
	{
		const char* labels[] = { "Wrap", "Mirror", "Clamp", "Border", "MirrorOnce" };
		int cur = static_cast<int>(value);
		bool changed = false;
		ImGui::PushID(id);
		if (ImGui::BeginCombo("##Addr", labels[cur]))
		{
			for (int i = 0; i < 5; ++i)
			{
				bool sel = (cur == i);
				if (ImGui::Selectable(labels[i], sel))
				{
					value = static_cast<Engine::ESamplerAddressMode>(i);
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();
		return changed;
	}

	// ---------- Texture 피커 (DnD 지원) ----------
	static bool DrawTextureHandlePicker(const char* id,
		Engine::ResourceHandle<Engine::Texture>& ioHandle)
	{
		using namespace Engine;
		bool changed = false;
		ImGui::PushID(id);

		string currentLabel = "None";
		if (ioHandle && ioHandle.Get())
			currentLabel = WStrToStr(ioHandle.Get()->GetKey());

		const float pickerButtonWidth = 32.0f;
		const float avail = ImGui::GetContentRegionAvail().x;
		ImGui::SetNextItemWidth(avail - pickerButtonWidth - ImGui::GetStyle().ItemSpacing.x);
		ImGui::InputText("##TexName", (char*)currentLabel.c_str(),
			currentLabel.size() + 1, ImGuiInputTextFlags_ReadOnly);

		// 드래그드롭 수락
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload =
				ImGui::AcceptDragDropPayload("CONTENT_ITEM_Texture"))
			{
				const wchar_t* path = static_cast<const wchar_t*>(payload->Data);
				if (path)
				{
					Handle h = ResourceManager::Get().LoadFile(wstring(path));
					if (h.IsValid())
					{
						ioHandle = ResourceHandle<Texture>(h);
						changed = true;
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_BULLSEYE, ImVec2(pickerButtonWidth, 0.0f)))
			ImGui::OpenPopup("TexturePickerPopup");

		if (ImGui::BeginPopup("TexturePickerPopup"))
		{
			if (ImGui::Selectable("None (Clear)", !ioHandle))
			{
				ioHandle = ResourceHandle<Texture>();
				changed = true;
			}

			auto& resourceManager = ResourceManager::Get();

			for (const Handle& h : resourceManager.GetResourceHandles<Texture>())
			{
				Texture* tex = dynamic_cast<Texture*>(resourceManager.GetResource(h));
				if (!tex)
					continue;

				const string key = WStrToStr(tex->GetKey());
				const bool selected = ioHandle && ioHandle.Get() == tex;

				if (ImGui::Selectable(key.c_str(), selected))
				{
					ioHandle = ResourceHandle<Texture>(h);
					changed = true;
				}
			}
			ImGui::EndPopup();
		}

		ImGui::PopID();
		return changed;
	}
} // anonymous namespace
#pragma endregion