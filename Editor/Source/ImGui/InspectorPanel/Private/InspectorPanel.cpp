#pragma once
#include "imgui.h"
#include "InspectorPanel.h"
#include "SelectionManager.h"

#include "GameObject.h"
#include "Components.h"
#include "Resources.h"
#include "FileDialogs.h"
#include "ResourceManager.h"
#include "Archives.h"
#include "IconsFontAwesome7.h"
#include "Reflection/ReflectionTypes.h"
#include "InspectorHelper.h"
#include "PropertyDrawerEnTT.h"
#include "LocalizationManager.h"

#include "Inspectors.h"
#include "ResourceEditors.h"
#include "ImGuiManager.h"

#include "Entity.h"
#include "Scene.h"
#include "Reflection/CoreComponentReflection.h"
#include "ReflectionPropertyDrawer.h"
#include "Components/TransformComponent.h"
#include "TransformSystem.h"

#include "CoreComponents.h"
#include "RenderComponents.h"
#include "PhysicsComponent.h"

#pragma region Constructor&Destructor
InspectorPanel::InspectorPanel()
{
	m_AssetInspectors.push_back(new TextureInspector());
	//m_AssetInspectors.push_back(new SpriteInspector());
	m_AssetInspectors.push_back(new ModelInspector());
}

void InspectorPanel::Free()
{
	__super::Free();
	for (InspectorInterface* inspector : m_AssetInspectors)
	{
		delete inspector;
	}
	m_AssetInspectors.clear();
}
#pragma endregion

void InspectorPanel::Draw()
{
	if (!m_Open) return;
	GameObject* selectedObject = SelectionManager::Get().GetPrimarySelection();
	Entity* selectedEntity = SelectionManager::Get().GetPrimarySelectedEntity();
	filesystem::path selectedAssetPath = SelectionManager::Get().GetSelectedAssetPath();
	string windowTitle = LOCAL("UI_INSPECTOR");
	string windowID = "InspectorPanel";

	if (m_SelectedGameObject)
	{
		windowTitle += " - " + WStrToStr(m_SelectedGameObject->GetName());
		windowID += "_" + to_string(m_SelectedGameObject->GetID());
		selectedObject = m_SelectedGameObject;
	}

	windowTitle += "###" + windowID;
	
	if (ImGui::Begin(windowTitle.c_str(), &m_Open))
	{
		if (selectedEntity != nullptr && m_SelectedGameObject == nullptr)
		{
			DrawEntityProperties(*selectedEntity);
		}
		else if (selectedObject)
		{
			entt::meta_type type = entt::resolve(selectedObject->GetTypeID());
			if (type)
			{
				entt::meta_any anyObj = type.from_void(selectedObject);
				DrawProperties(anyObj, type);
			}
			const vector<Component*>& components = selectedObject->GetAllComponents();
			for (Component* component : components)
			{
				entt::meta_type compType = entt::resolve(component->GetTypeID());
				if (compType)
				{
					entt::meta_any anyComp = compType.from_void(component);
					if (DrawProperties(anyComp, compType))
					{
						component->SetDirty();
					}
				}
			}

			ImGui::Spacing();

			DrawAddComponentButton();
		}
		else if (!selectedAssetPath.empty())
		{

			bool bIsSupported = false;
			string assetName = selectedAssetPath.stem().string();
			ImGui::Text(("Selected Asset: " + assetName).c_str());
			for (InspectorInterface* inspector : m_AssetInspectors)
			{
				if (bIsSupported = inspector->IsSupported(selectedAssetPath))
				{
					inspector->OnInspectorGUI(selectedAssetPath);
					break;
				}
			}

			if (!bIsSupported)
			{
				ImGui::Text("Unsupported Asset File");
				ImGui::Separator();
				ImGui::Text("File: %s", selectedAssetPath.filename().string().c_str());
			}
		}
		else
		{
			ImGui::Text("No object selected.");
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

bool InspectorPanel::DrawProperties(entt::meta_any& instance, const entt::meta_type& type)
{
	bool anyChanged = false;
	
	void* idPtr = nullptr;
	if (auto* go = instance.try_cast<Engine::GameObject>()) idPtr = go;
	else if (auto* comp = instance.try_cast<Engine::Component>()) idPtr = comp;
	ImGui::PushID(idPtr);

	bool opened = PropertyDrawerEnTT::DrawHeaderNode(instance, type);
	if (opened)
	{
		anyChanged |= PropertyDrawerEnTT::DrawPropertyTable(instance, type);
	}
	
	// Temporarily bypass original complex TypeInfo loop
	/*
	if (opened)
	{
		const TypeInfo* currentTypeInfo = &typeInfo;
		while (currentTypeInfo)
		{
			if (currentTypeInfo != &typeInfo)
			{
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();
			}

			unordered_map<string, std::vector<const Engine::PropertyInfo*>> categoryMap;
			vector<const Engine::PropertyInfo*> defaultProps;

			for (const auto& prop : currentTypeInfo->Properties)
			{
				if (!GetMetadataEditable(prop.Metadata))
				{
					continue;
				}

				string category = GetMetadataString(prop.Metadata, MetaCategoryHash);
				if (!category.empty())
				{
					categoryMap[category].push_back(&prop);
				}
				else
				{
					defaultProps.push_back(&prop);
				}
			}

			anyChanged |= PropertyDrawer::DrawPropertyTable(instance, *currentTypeInfo, defaultProps);

			for (const auto& [category, props] : categoryMap)
			{
				ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.25f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.15f, 0.15f, 0.2f, 1.0f));

				string categoryName = category.empty() ? LOCAL("General") : LOCAL(category);
				bool categoryOpened = ImGui::CollapsingHeader(categoryName.c_str(), ImGuiTreeNodeFlags_None);
				ImGui::PopStyleColor(3);

				if (categoryOpened)
				{
					anyChanged |= PropertyDrawer::DrawPropertyTable(instance, *currentTypeInfo, props);
				}
			}

			currentTypeInfo = currentTypeInfo->ParentQualifiedName.empty()
				? nullptr
				: legacy_reflection::Registry::Get().GetTypeByQualifiedName(currentTypeInfo->ParentQualifiedName);
		}
	}
	*/

	if (Engine::Component* component = instance.try_cast<Engine::Component>())
	{
		if (Engine::RenderComponent* renderComponent = dynamic_cast<Engine::RenderComponent*>(component))
		{
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			this->DrawRenderComponentMaterialEditor(renderComponent);
		}
	}

	ImGui::PopID();
	return anyChanged;
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

			world.get_or_emplace<Engine::TransformComponent>(handle);
			world.get_or_emplace<Engine::WorldTransformComponent>(handle);
			world.get_or_emplace<Engine::FlagComponent>(handle);

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
void InspectorPanel::DrawAddComponentButton()
{
	if (ImGui::Button("Add Component"))
	{
		ImGui::OpenPopup("AddComponentPopup");
	}
	DrawAddComponentPopup();
}
void InspectorPanel::DrawAddComponentPopup()
{
	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		ImGui::Text("Component List Placeholder");
		ImGui::EndPopup();
	}
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

			const TypeInfo* texType =
				legacy_reflection::Registry::Get().GetTypeByQualifiedName("Engine::Texture");
			if (texType)
			{
				for (const Handle& h :
					ResourceManager::Get().GetResourceHandlesIncludingDerived(texType->ID))
				{
					Resource* res = ResourceManager::Get().GetResource(h);
					Texture* tex = dynamic_cast<Texture*>(res);
					if (!tex) continue;

					const wstring keyW = tex->GetKey();
					const string  key = WStrToStr(keyW);
					bool sel = ioHandle && ioHandle.Get() &&
						(ioHandle.Get()->GetKey() == keyW);

					if (ImGui::Selectable(key.c_str(), sel))
					{
						ioHandle = ResourceManager::Get()
							.GetResourceHandle<Texture>(keyW);
						changed = true;
					}
				}
			}
			ImGui::EndPopup();
		}

		ImGui::PopID();
		return changed;
	}
} // anonymous namespace



void InspectorPanel::DrawRenderComponentMaterialEditor(RenderComponent* renderComponent)
{
	if (!renderComponent) return;
	if (!ImGui::CollapsingHeader("Material Editor", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	static unordered_map<RenderComponent*, int> s_SelectedSlot;
	int& selectedSlot = s_SelectedSlot[renderComponent];
	const uint32 slotCount = std::max(renderComponent->GetMaterialSlotCount(), 1u);
	selectedSlot = std::clamp(selectedSlot, 0, static_cast<int>(slotCount) - 1);

	string preview = "Slot " + to_string(selectedSlot);
	if (ImGui::BeginCombo("Material Slot", preview.c_str()))
	{
		for (uint32 i = 0; i < slotCount; ++i)
		{
			bool sel = (selectedSlot == static_cast<int>(i));
			if (ImGui::Selectable(("Slot " + to_string(i)).c_str(), sel))
				selectedSlot = static_cast<int>(i);
			if (sel) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	const uint32 slot = static_cast<uint32>(selectedSlot);
	MaterialInterface* sharedMat = renderComponent->GetSharedMaterial(slot);
	MaterialInterface* runtimeMat = renderComponent->GetMaterial(slot);

	ImGui::Text("Shared: %s", sharedMat ? "Yes" : "No");
	ImGui::SameLine();
	ImGui::Text("Dynamic: %s", renderComponent->GetDynamicMaterialInstance(slot) ? "Yes" : "No");

	MaterialInterface* targetMat = sharedMat ? sharedMat : runtimeMat;
	if (!targetMat)
	{
		ImGui::TextDisabled("No material assigned.");
		return;
	}

	const wstring targetKey = targetMat->GetKey();
	if (targetKey.empty())
	{
		ImGui::TextDisabled("Runtime-only material has no asset key.");
		return;
	}

	if (ImGui::Button(ICON_FA_UP_RIGHT_FROM_SQUARE " Open Resource Material Editor"))
	{
		filesystem::path assetPath(targetKey);
		SelectionManager::Get().SetSelectedResource(assetPath);

		for (ImGuiInterface* panel : ImGuiManager::Get().GetImGuiPanels())
		{
			ResourceEditorInterface* editor = dynamic_cast<ResourceEditorInterface*>(panel);
			if (!editor) continue;

			if (editor->IsSupported(assetPath))
			{
				editor->SetTargetResource(assetPath);

				// MaterialEditor면 런타임 슬롯 컨텍스트 전달
				if (MaterialEditor* matEditor = dynamic_cast<MaterialEditor*>(editor))
				{
					matEditor->SetRenderComponentContext(renderComponent, slot);
				}

				editor->Open();
				break;
			}
		}
	}
}
#pragma endregion