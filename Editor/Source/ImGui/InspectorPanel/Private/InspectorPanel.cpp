#pragma once
#include "imgui.h"
#include "InspectorPanel.h"
#include "SelectionManager.h"

#include "GameObject.h"
#include "Transform.h"
#include "Component.h"
#include "ReflectionTypes.h"
#include "InspectorHelper.h"
#include "PropertyDrawer.h"
#include "LocalizationManager.h"

void InspectorPanel::Draw()
{
	if (!m_Open) return;
	GameObject* selectedObject = SelectionManager::Get().GetPrimarySelection();
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
			DrawProperties(selectedObject, selectedObject->GetType());
			const vector<Component*>& components = selectedObject->GetAllComponents();
			for (Component* component : components)
			{
				if (DrawProperties(component, component->GetType()))
				{
					component->SetDirty();
				}
			}

			ImGui::Spacing();

			DrawAddComponentButton();
		}
		else
		{
			ImGui::Text("No object selected.");
		}
		ImGui::End();
	}
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
				string category = GetMetadataString(prop.Metadata, MetaCategoryHash);
				if (!category.empty())
					categoryMap[category].push_back(&prop);
				else
					defaultProps.push_back(&prop);
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

				if(categoryOpened)
					anyChanged |= PropertyDrawer::DrawPropertyTable(instance, *currentTypeInfo, props);
			}	
			currentTypeInfo = currentTypeInfo->ParentName.empty() ? nullptr : Engine::ReflectionRegistry::Get().GetType(currentTypeInfo->ParentName.data());
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