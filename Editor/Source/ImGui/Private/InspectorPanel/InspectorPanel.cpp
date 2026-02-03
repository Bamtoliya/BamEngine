#pragma once
#include "imgui.h"
#include "InspectorPanel.h"
#include "SelectionManager.h"

#include "GameObject.h"
#include "Component.h"
#include "ReflectionTypes.h"

#pragma region Helper
// Enum이나 정수형 데이터를 안전하게 읽어오는 헬퍼
int64_t ReadInteger(void* data, size_t size)
{
	switch (size)
	{
	case 1: return *(int8_t*)data;
	case 2: return *(int16_t*)data;
	case 4: return *(int32_t*)data;
	case 8: return *(int64_t*)data;
	default: return 0;
	}
}

// 데이터를 다시 원본 메모리에 쓰는 헬퍼
void WriteInteger(void* data, size_t size, int64_t value)
{
	switch (size)
	{
	case 1: *(int8_t*)data = (int8_t)value; break;
	case 2: *(int16_t*)data = (int16_t)value; break;
	case 4: *(int32_t*)data = (int32_t)value; break;
	case 8: *(int64_t*)data = (int64_t)value; break;
	}
}
#pragma endregion

void InspectorPanel::Draw()
{
	GameObject* selectedObject = SelectionManager::Get().GetPrimarySelection();
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

	ImGui::PushID(instance);
	if (ImGui::CollapsingHeader(typeInfo.GetName().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{

		map<string, vector<const Engine::PropertyInfo*>> categoryMap;
		vector<const Engine::PropertyInfo*> defaultProps;

		for (const auto& property : typeInfo.GetProperties())
		{
			if (property.Metadata.Category.empty())
				defaultProps.push_back(&property);
			else
				categoryMap[property.Metadata.Category].push_back(&property);
		}

		DrawPropertyTable(instance, typeInfo, defaultProps);

		for (const auto& [catName, props] : categoryMap)
		{
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.2f, 0.25f, 1.0f)); // 평소 색
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.25f, 0.3f, 1.0f)); // 마우스 올렸을 때
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.15f, 0.15f, 0.2f, 1.0f)); // 클릭했을 때
			string categoryName = LocalizationManager::Get().GetText(catName);

			if (ImGui::CollapsingHeader(categoryName.c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::PopStyleColor(3);
				ImGui::Indent();
				DrawPropertyTable(instance, typeInfo, props);
				ImGui::Unindent();
			}
			else
			{
				ImGui::PopStyleColor(3);
			}
		}

	}
	ImGui::PopID();
}


#pragma region VarName & Label Sanitization
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


#pragma region Table
void InspectorPanel::DrawPropertyTable(void* instance, const TypeInfo& typeInfo, const vector<const Engine::PropertyInfo*>& props)
{
	if (props.empty()) return;

	if (ImGui::BeginTable("PropTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.35f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);


		for (const auto* propPtr : props)
		{
			const auto& property = *propPtr;
			void* data = (uint8_t*)instance + property.Offset;
			string displayLabel = SanitizeDisplayLabel(typeInfo, property);

			ImGui::PushID(property.Name.c_str());
			ImGui::TableNextRow();

			// [Column 1] 이름 & 툴팁
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::Text("%s", displayLabel.c_str());

			// 툴팁 표시
			if (!property.Metadata.Tooltip.empty() && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", property.Metadata.Tooltip.c_str());
			}

			// [Column 2] 위젯
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);

			bool bReadOnly = property.Metadata.bIsReadOnly;
			if (bReadOnly)
				ImGui::BeginDisabled();

			if (property.Metadata.bIsFilePath || property.Metadata.bIsDirectory)
			{
				string* strVal = reinterpret_cast<string*>(data);
				char buffer[1024];
				strcpy_s(buffer, strVal->c_str());

				f32 buttonWidth = ImGui::GetFrameHeight();
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x);

				if (ImGui::InputText("##path", buffer, sizeof(buffer)))
				{
					*strVal = buffer;
				}

				if (ImGui::Button(property.Metadata.bIsDirectory ? "D" : "...", ImVec2(buttonWidth, 0)))
				{
					std::string selectedPath;
					if (property.Metadata.bIsDirectory)
					{
						// selectedPath = Platform::OpenFolderDialog();
					}
					else
					{
						// selectedPath = Platform::OpenFileDialog(property.Metadata.FileFilter);
					}

					if (!selectedPath.empty())
					{
						*strVal = selectedPath;
					}
				}
			}
			else
			{
				switch (property.Type)
				{
				case EPropertyType::Int32:
				case EPropertyType::Int64:
				case EPropertyType::UInt32:
				case EPropertyType::UInt64:
				{
					DrawIntegerProperty(data, property);
					break;
				}
				case EPropertyType::Float:
				{
					if (property.Metadata.bHasRange)
					{
						float* value = reinterpret_cast<float*>(data);
						ImGui::SliderFloat("##value", value, property.Metadata.Min, property.Metadata.Max, "%.3f", ImGuiSliderFlags_None);
					}
					else
					{
						float* value = reinterpret_cast<float*>(data);
						ImGui::DragFloat("##value", value);
					}
					break;
				}
				case EPropertyType::Vector3:
				{
					vec3* value = reinterpret_cast<vec3*>(data);
					ImGui::DragFloat3("##value", &(*value)[0]);
					break;
				}
				case EPropertyType::Quaternion:
				{
					vec3* value = reinterpret_cast<vec3*>(data);
					ImGui::DragFloat4("##value", &(*value)[0]);
					break;
				}
				case EPropertyType::Vector4:
				{
					vec4* value = reinterpret_cast<vec4*>(data);
					ImGui::DragFloat4("##value", &(*value)[0]);
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
				case EPropertyType::Enum:
				{
					int64 currentVal = ReadInteger(data, property.Size);
					const auto* enumInfo = Engine::ReflectionRegistry::Get().GetEnum(property.TypeName);
					string previewName = "Unknown";

					if (enumInfo)
					{
						for (const auto& [name, val] : enumInfo->Entries)
						{
							if (val == currentVal) { previewName = name; break; }
						}
					}

					if (ImGui::BeginCombo("##Enum", previewName.c_str()))
					{
						if (enumInfo)
						{
							for (const auto& [name, val] : enumInfo->Entries)
							{
								bool isSelected = (currentVal == val);
								if (ImGui::Selectable(name.c_str(), isSelected))
								{
									WriteInteger(data, property.Size, val);
								}
							}
						}
						ImGui::EndCombo();
					}
					break;
				}
				case EPropertyType::BitFlag:
				{
					int64 currentVal = ReadInteger(data, property.Size);
					const auto* enumInfo = Engine::ReflectionRegistry::Get().GetEnum(property.TypeName);
					string previewStr = "None";
					if (enumInfo)
					{
						previewStr = enumInfo->GetBitFlagsString(currentVal);
					}
					if (ImGui::BeginCombo("##BitFlag", previewStr.c_str()))
					{
						if (enumInfo)
						{
							for (const auto& [name, flagVal] : enumInfo->OrderedEntries)
							{
								if (flagVal == 0)
								{
									if (ImGui::Selectable(name.c_str(), currentVal == 0))
									{
										currentVal = 0; // 모두 끄기
										WriteInteger(data, property.Size, currentVal);
									}
									continue;
								}

								bool bChecked = (currentVal & flagVal) == flagVal;

								if (ImGui::Checkbox(name.c_str(), &bChecked))
								{
									if (bChecked)
										currentVal |= flagVal;  // 비트 켜기 (OR)
									else
										currentVal &= ~flagVal; // 비트 끄기 (AND NOT)

									WriteInteger(data, property.Size, currentVal);
								}
							}
						}
						else
						{
							ImGui::TextDisabled("Enum info not found");
						}
						ImGui::EndCombo();
					}
					break;
				}
				case EPropertyType::Set:
				{
					DrawSetProperty(data, typeInfo, property);
					break;
				}
				case EPropertyType::Matrix3:
				{
					DrawMatrixProperty(data, typeInfo, property, 3);
					break;
				}
				case EPropertyType::Matrix4:
				{
					DrawMatrixProperty(data, typeInfo, property);
					break;
				}
				default:
					ImGui::Text("Unsupported property type.");
					break;
				}
			}
			if (bReadOnly)
				ImGui::EndDisabled();
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
}
#pragma endregion

#pragma region Types

void InspectorPanel::DrawIntegerProperty(void* data, const PropertyInfo& property)
{
	ImGuiDataType dataType = ImGuiDataType_S32;

	switch (property.Type)
	{
		case EPropertyType::Int32: dataType = ImGuiDataType_S32; break;
		case EPropertyType::Int64: dataType = ImGuiDataType_S64; break;
		case EPropertyType::UInt32: dataType = ImGuiDataType_U32; break;
		case EPropertyType::UInt64: dataType = ImGuiDataType_U64; break;
	}

	ImGui::DragScalar("##value", dataType, data);
}
void InspectorPanel::DrawMatrixProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property, uint32 dim)
{
	string headerName = SanitizeVarName(property.Name) + "###" + property.Name;
	float* matData = reinterpret_cast<float*>((uint8_t*)data);
	bool bReadOnly = property.Metadata.bIsReadOnly;

	if (bReadOnly)
		ImGui::EndDisabled();
	if (ImGui::CollapsingHeader(headerName.c_str()))
	{
		if (bReadOnly)
			ImGui::BeginDisabled();
		ImGui::Indent();

		// ------------------------------------------------------------------
		// [테이블 시작] 4개의 컬럼 (X, Y, Z, W)
		// ------------------------------------------------------------------
		// BordersInnerV: 세로선 그리기
		// RowBg: 줄무늬 배경 (가독성 향상)
		if (ImGui::BeginTable("##MatrixTable", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		{
			// 컬럼 비율 설정 (4등분)
			for (uint32 i = 0; i < dim; i++)
			{
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
			}

			// 4행 x 4열 루프
			for (uint32 row = 0; row < dim; row++)
			{
				ImGui::TableNextRow(); // 다음 줄

				for (uint32 col = 0; col < dim; col++)
				{
					ImGui::TableNextColumn(); // 다음 칸

					uint32 index = row * 4 + col; // 0 ~ 15

					ImGui::PushID(index); // 각 셀마다 고유 ID 필요

					// 입력 위젯 너비 꽉 채우기
					ImGui::SetNextItemWidth(-FLT_MIN);

					// 값 그리기 (DragFloat 추천 - 미세 조정 가능)
					// 속도(v_speed)는 0.01f ~ 0.1f 정도로 설정
					ImGui::DragFloat("##cell", &matData[index], 0.05f, 0.0f, 0.0f, "%.2f");

					ImGui::PopID();
				}
			}
			ImGui::EndTable();
		}

		// [편의 기능] Identity(단위 행렬)로 초기화 버튼
		if (!bReadOnly && ImGui::Button("Reset Identity"))
		{
			// 0으로 밀고 대각선만 1로 설정
			memset(matData, 0, 16 * sizeof(f32));
			matData[0] = 1.0f;  // [0][0]
			matData[5] = 1.0f;  // [1][1]
			matData[10] = 1.0f; // [2][2]
			matData[15] = 1.0f; // [3][3]
		}
		

		ImGui::Unindent();
		if (bReadOnly)
			ImGui::EndDisabled();
	}
	if (bReadOnly)
		ImGui::BeginDisabled();
}
void InspectorPanel::DrawSetProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	auto* accessor = property.Accessor;
	if (!accessor) return;

	uint32 size = accessor->GetSize(data);
	string headerName = SanitizeDisplayLabel(typeinfo, property) + " (" + to_string(size) + ")###" + property.Name;
	string innerType = property.InnerTypeName;

	if (ImGui::CollapsingHeader(headerName.c_str()))
	{
		ImGui::Indent();

		ImGui::TextDisabled("Add New Element");

		static int32 tempInt = 0;
		static f32 tempFloat = 0.f;
		static char tempStr[256] = "";
		static string tempObjString;
		static wstring tempObjWString;

		void* valueToAdd = nullptr;

		if (innerType == "int32" || innerType == "int64" || innerType == "uint32" || innerType == "uint64")
		{
			ImGui::InputInt("##newInt", &tempInt);
			valueToAdd = &tempInt;
		}
		else if (innerType == "float")
		{
			ImGui::InputFloat("##newFloat", &tempFloat);
			valueToAdd = &tempFloat;
		}
		else if (innerType == "string" || innerType == "String")
		{
			ImGui::InputText("##newStr", tempStr, sizeof(tempStr));
			tempObjString = string(tempStr);
			valueToAdd = &tempObjString;
		}
		else if (innerType == "wstring")
		{
			ImGui::InputText("##newStr", tempStr, sizeof(tempStr));
			tempObjWString = StrToWStr(tempStr);
			valueToAdd = &tempObjWString;
		}
		else
		{
			ImGui::TextDisabled("Unsupported type for addition");
		}

		ImGui::SameLine();
		if (ImGui::Button("+") && valueToAdd)
		{
			accessor->Add(data, valueToAdd);
		}

		ImGui::Separator();

		vector<void*> elements = accessor->GetElements(data);
		void* toRemove = nullptr;

		if (ImGui::BeginTable("##SetListTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 30.0f);

			for (int i = 0; i < elements.size(); ++i)
			{
				ImGui::PushID(i);
				ImGui::TableNextRow();
				void* elemPtr = elements[i];

				ImGui::TableNextColumn();
				ImGui::AlignTextToFramePadding();
				if (innerType == "Int32")
				{
					int val = *(int*)elemPtr;
					ImGui::Text("%d", val);
				}
				else if (innerType == "F32")
				{
					float val = *(float*)elemPtr;
					ImGui::Text("%.3f", val);
				}
				else if (innerType == "String" || innerType == "string")
				{
					string* val = static_cast<string*>(elemPtr);
					ImGui::TextWrapped("%s", val->c_str());
				}
				else if (innerType == "WString" || innerType == "wstring")
				{
					wstring* val = static_cast<wstring*>(elemPtr);
					string utf8Str = WStrToStr(*val);

					ImGui::TextWrapped("%s", utf8Str.c_str());
				}

				ImGui::TableNextColumn();
				if (ImGui::Button("X", ImVec2(-1, 0)))
				{
					toRemove = elemPtr;
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		if (toRemove)
		{
			accessor->Remove(data, toRemove);
		}


		ImGui::Unindent();
	}

}
#pragma endregion


