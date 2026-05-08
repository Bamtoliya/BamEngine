#pragma once
#include "imgui.h"
#include "InspectorPanel.h"
#include "SelectionManager.h"

#include "GameObject.h"
#include "Transform.h"
#include "Component.h"
#include "FileDialogs.h"
#include "RenderComponent.h"
#include "MaterialInterface.h"
#include "MaterialInstance.h"
#include "Texture.h"
#include "ResourceManager.h"
#include "Archives.h"
#include "IconsFontAwesome7.h"
#include "ReflectionTypes.h"
#include "InspectorHelper.h"
#include "PropertyDrawer.h"
#include "LocalizationManager.h"

#include "TextureInspector.h"
#include "SpriteInspector.h"
#include "ModelInspector.h"
#include "ResourceEditors.h"
#include "ImGuiManager.h"

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
	filesystem::path selectedAssetPath = SelectionManager::Get().GetSelectedAssetPath();
	string windowID = "Inspector";
	if (m_SelectedGameObject)
	{
		windowID += " - " + WStrToStr(m_SelectedGameObject->GetName()) + "##";
		selectedObject = m_SelectedGameObject;
	}
	
	if (ImGui::Begin(windowID.c_str(), &m_Open))
	{
		if (selectedObject)
		{
			DrawProperties(selectedObject, selectedObject->GetTypeInfo());
			const vector<Component*>& components = selectedObject->GetAllComponents();
			for (Component* component : components)
			{
				if (DrawProperties(component, component->GetTypeInfo()))
				{
					component->SetDirty();
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

bool InspectorPanel::DrawProperties(void* instance, const TypeInfo& typeInfo)
{
	bool anyChanged = false;
	ImGui::PushID(instance);
	bool opened = PropertyDrawer::DrawHeaderNode(instance, typeInfo);
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
				: reflection::Registry::Get().GetTypeByQualifiedName(currentTypeInfo->ParentQualifiedName);
		}
	}

	if (Component* component = dynamic_cast<Component*>(reinterpret_cast<Base*>(instance)))
	{
		if (RenderComponent* renderComponent = dynamic_cast<RenderComponent*>(component))
		{
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			DrawRenderComponentMaterialEditor(renderComponent);
		}
	}

	ImGui::PopID();
	return anyChanged;
}

#pragma region Add Component
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
				reflection::Registry::Get().GetTypeByQualifiedName("Engine::Texture");
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