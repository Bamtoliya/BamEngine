#pragma once
#include "imgui.h"
#include "InspectorPanel.h"

#include "GameObject.h"
#include "Component.h"
#include "ReflectionTypes.h"


void InspectorPanel::Draw(class GameObject* selectedObject)
{
	ImGui::Begin("Inspector");
	if (selectedObject)
	{
		DrawProperties(selectedObject, selectedObject->GetType());
		const vector<Component*>& components = selectedObject->GetAllComponents();
		for (Component* component : components)
		{
			DrawProperties(component, component->GetType());
		}

		ImGui::Spacing();

		if (ImGui::Button("Add Component"))
		{
			ImGui::OpenPopup("AddComponentPopup");
		}

		if (ImGui::BeginPopup("AddComponentPopup"))
		{
			ImGui::Text("Component List Placeholder");
			ImGui::EndPopup();
		}

	}
	else
	{
		ImGui::Text("No object selected.");
	}
	ImGui::End();
}

void InspectorPanel::DrawProperties(void* instance, const TypeInfo& typeInfo)
{
	if (ImGui::CollapsingHeader(typeInfo.GetName().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (const auto& property : typeInfo.GetProperties())
		{
			void* data = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(instance) + property.Offset);
			const char* label = property.Name.c_str();
			string displayLabel = SanitizeDisplayLabel(typeInfo, property);
			ImGui::PushID(label);
			ImGui::Text("%s", displayLabel.c_str());
			ImGui::SameLine();
			switch (property.Type)
			{
			case EPropertyType::Int32:
			case EPropertyType::Int64:
			{
				int* value = reinterpret_cast<int*>(data);
				ImGui::InputInt("##value", value);
				break;
			}
			case EPropertyType::UInt32:
			case EPropertyType::UInt64:
			{
				unsigned int* value = reinterpret_cast<unsigned int*>(data);
				ImGui::InputInt("##value", reinterpret_cast<int*>(value));
				break;
				}
			case EPropertyType::Float:
			{
				float* value = reinterpret_cast<float*>(data);
				ImGui::InputFloat("##value", value);
				break;
			}
			case EPropertyType::Vector3:
			{
				vec3* value = reinterpret_cast<vec3*>(data);
				ImGui::InputFloat3("##value", &(*value)[0]);
				break;
			}
			case EPropertyType::Quaternion:
			{
				vec3* value = reinterpret_cast<vec3*>(data);
				ImGui::InputFloat3("##value", &(*value)[0]);
				break;
			}
			case EPropertyType::Vector4:
			{
				vec4* value = reinterpret_cast<vec4*>(data);
				ImGui::InputFloat4("##value", &(*value)[0]);
				break;
			}
			case EPropertyType::Color:
			{
				vec4* value = reinterpret_cast<vec4*>(data);
				ImGui::ColorEdit4("##value", &(*value)[0]);
				break;
			}
			case EPropertyType::Bool:
			{
				bool* value = reinterpret_cast<bool*>(data);
				ImGui::Checkbox("##value", value);
				break;
			}
			case EPropertyType::String:
			{
				if (property.TypeName == "wstring")
				{
					std::wstring* value = reinterpret_cast<std::wstring*>(data);

					string utf8Str = Engine::WStrToStr(*value);
					char buffer[1024];
					strcpy_s(buffer, utf8Str.c_str());
					if (ImGui::InputText("##value", buffer, sizeof(buffer)))
					{
						*value = Engine::StrToWStr(buffer);
					}
				}
				else
				{
					std::string* value = reinterpret_cast<std::string*>(data);
					char buffer[1024];
					strcpy_s(buffer, value->c_str());
					if (ImGui::InputText("##value", buffer, sizeof(buffer)))
					{
						*value = std::string(buffer);
					}
				}
				break;
			}
			default:
				ImGui::Text("Unsupported property type.");
				break;
			}
			ImGui::PopID();
		}
	}
}


#pragma region Helper
string InspectorPanel::SanitizeVarName(const string& varName)
{
	string cleanName = varName;

	if (cleanName.length() > 2 && cleanName.substr(0, 2) == "m_")
	{
		cleanName = cleanName.substr(2);
	}
	else if (cleanName.length() > 1 && cleanName[0] == 'b' && isupper(cleanName[1]))
	{
		cleanName = cleanName.substr(1);
	}
	return  cleanName;
}

string InspectorPanel::SanitizeDisplayLabel(const TypeInfo& typeInfo, const PropertyInfo& property)
{
	string lookupKey;
	if (!property.Metadata.DisplayName.empty())
	{
		lookupKey = property.Metadata.DisplayName;
	}
	else
	{
		string cleanVarName = SanitizeVarName(property.Name);
		lookupKey = typeInfo.GetName() + "." + cleanVarName;
	}	

	string displayLabel = Engine::LocalizationManager::Get().GetText(lookupKey);
	if (displayLabel == lookupKey)
	{
		displayLabel = SanitizeVarName(property.Name);
	}
	return displayLabel;
}
#pragma endregion