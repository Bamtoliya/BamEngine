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
					DrawVector3Property(instance, data, typeInfo, property);
					break;
				}
				case EPropertyType::Quaternion:
				{
					vec3* value = reinterpret_cast<vec3*>(data);
					DrawQuaternionProperty(instance, data, typeInfo, property);
					break;
				}
				case EPropertyType::Vector4:
				{
					vec4* value = reinterpret_cast<vec4*>(data);
					DrawVector4Property(instance, data, typeInfo, property);
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

static bool DrawVec3Control(const std::string& label, glm::vec3& values, bool& lockX, bool& lockY, bool& lockZ, float resetValue = 0.0f)
{
	bool changed = false;
	ImGui::PushID(label.c_str());

	// ---------------------------------------------------------
	// 1. 레이아웃 및 너비 계산
	// ---------------------------------------------------------
	float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
	ImVec2 buttonSize = { lineHeight, lineHeight }; // XYZ 색상 버튼 크기
	ImVec2 lockSize = { lineHeight, lineHeight };        // 자물쇠 버튼 크기 (정사각형)

	float availableWidth = ImGui::GetContentRegionAvail().x;
	float itemSpacing = ImGui::GetStyle().ItemSpacing.x;

	// 전체 너비에서 [색상버튼3개] + [자물쇠3개] + [사이간격들]을 뺌
	// 간격 구성: (Btn-Input)*3 + (Input-Lock)*3 + (Group-Group)*2
	// ImGui::SameLine(0,0)을 쓰는 곳은 간격 0으로 계산

	// 그룹 간 간격(2개) + 입력창과 자물쇠 사이 간격(3개) 고려
	float totalFixed = (buttonSize.x * 3) + (lockSize.x * 3) + (itemSpacing * 2);

	float inputWidth = (availableWidth - totalFixed) / 3.0f;
	if (inputWidth < 1.0f) inputWidth = 1.0f;


	// ---------------------------------------------------------
	// X Axis (Red)
	// ---------------------------------------------------------
	ImGui::PushID("X");

	// [잠금 구역 시작]: 색상 버튼과 입력창만 비활성화
	if (lockX) ImGui::BeginDisabled();
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		if (ImGui::Button("X", buttonSize)) { values.x = resetValue; changed = true; }
		ImGui::PopStyleColor(3);

		ImGui::SameLine(0, 0); // 버튼과 입력창 딱 붙이기
		ImGui::SetNextItemWidth(inputWidth);
		if (ImGui::DragFloat("##Val", &values.x, 0.1f, 0.0f, 0.0f, "%.2f")) changed = true;
	}
	if (lockX) ImGui::EndDisabled();
	// [잠금 구역 끝]

	// 자물쇠 버튼 (항상 활성화)
	ImGui::SameLine(0, 0);
	const char* iconX = lockX ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN;
	if (ImGui::Button(iconX, lockSize)) { lockX = !lockX; changed = true; } // 클릭 시 토글
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(lockX ? "Unlock X" : "Lock X");

	ImGui::PopID();

	ImGui::SameLine(0, itemSpacing); // X그룹과 Y그룹 사이 간격


	// ---------------------------------------------------------
	// Y Axis (Green)
	// ---------------------------------------------------------
	ImGui::PushID("Y");

	if (lockY) ImGui::BeginDisabled();
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		if (ImGui::Button("Y", buttonSize)) { values.y = resetValue; changed = true; }
		ImGui::PopStyleColor(3);

		ImGui::SameLine(0, 0);
		ImGui::SetNextItemWidth(inputWidth);
		if (ImGui::DragFloat("##Val", &values.y, 0.1f, 0.0f, 0.0f, "%.2f")) changed = true;
	}
	if (lockY) ImGui::EndDisabled();

	ImGui::SameLine(0, 0);
	const char* iconY = lockY ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN;
	if (ImGui::Button(iconY, lockSize)) { lockY = !lockY; changed = true; }
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(lockY ? "Unlock Y" : "Lock Y");

	ImGui::PopID();

	ImGui::SameLine(0, itemSpacing);


	// ---------------------------------------------------------
	// Z Axis (Blue)
	// ---------------------------------------------------------
	ImGui::PushID("Z");

	if (lockZ) ImGui::BeginDisabled();
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		if (ImGui::Button("Z", buttonSize)) { values.z = resetValue; changed = true; }
		ImGui::PopStyleColor(3);

		ImGui::SameLine(0, 0);
		ImGui::SetNextItemWidth(inputWidth);
		if (ImGui::DragFloat("##Val", &values.z, 0.1f, 0.0f, 0.0f, "%.2f")) changed = true;
	}
	if (lockZ) ImGui::EndDisabled();

	ImGui::SameLine(0, 0);
	const char* iconZ = lockZ ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN;
	if (ImGui::Button(iconZ, lockSize)) { lockZ = !lockZ; changed = true; }
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(lockZ ? "Unlock Z" : "Lock Z");

	ImGui::PopID();

	ImGui::PopID(); // Main Label ID
	return changed;
}

static void DrawVec4Control(const std::string& label, glm::vec4& values, float resetValue = 0.0f)
{
	ImGui::PushID(label.c_str());

	// 1. 사이즈 및 간격 계산
	float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
	ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight }; // 버튼 크기

	// 테이블 컬럼의 전체 가용 너비 가져오기
	float availableWidth = ImGui::GetContentRegionAvail().x;

	// 요소 사이의 간격 (X-Y, Y-Z, Z-W => 총 3개)
	float itemSpacing = ImGui::GetStyle().ItemSpacing.x;

	// [버튼 4개] + [간격 3개]를 전체에서 뺀 후, 4로 나누어 입력창 하나의 너비 계산
	float inputWidth = (availableWidth - (buttonSize.x * 4) - (itemSpacing * 3)) / 4.0f;

	// 최소 너비 보정
	if (inputWidth < 1.0f) inputWidth = 1.0f;

	// -------------------------------------------------------------------------
	// X Axis (Red)
	// -------------------------------------------------------------------------
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	if (ImGui::Button("X", buttonSize)) values.x = resetValue;
	ImGui::PopStyleColor(3);

	ImGui::SameLine(0, 0);
	ImGui::SetNextItemWidth(inputWidth);
	ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");

	ImGui::SameLine(0, itemSpacing);

	// -------------------------------------------------------------------------
	// Y Axis (Green)
	// -------------------------------------------------------------------------
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	if (ImGui::Button("Y", buttonSize)) values.y = resetValue;
	ImGui::PopStyleColor(3);

	ImGui::SameLine(0, 0);
	ImGui::SetNextItemWidth(inputWidth);
	ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");

	ImGui::SameLine(0, itemSpacing);

	// -------------------------------------------------------------------------
	// Z Axis (Blue)
	// -------------------------------------------------------------------------
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	if (ImGui::Button("Z", buttonSize)) values.z = resetValue;
	ImGui::PopStyleColor(3);

	ImGui::SameLine(0, 0);
	ImGui::SetNextItemWidth(inputWidth);
	ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");

	ImGui::SameLine(0, itemSpacing);

	// -------------------------------------------------------------------------
	// W Axis (Gray / Alpha)
	// -------------------------------------------------------------------------
	// W는 보통 회색이나 흰색 계열을 사용합니다.
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.6f, 0.6f, 0.6f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f });
	if (ImGui::Button("W", buttonSize)) values.w = resetValue;
	ImGui::PopStyleColor(3);

	ImGui::SameLine(0, 0);
	ImGui::SetNextItemWidth(inputWidth);
	ImGui::DragFloat("##W", &values.w, 0.1f, 0.0f, 0.0f, "%.2f");

	ImGui::PopID();
}

void InspectorPanel::DrawVector3Property(void* instance, void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	vec3* value = reinterpret_cast<vec3*>(data);
	bool lockX = false, lockY = false, lockZ = false;
	bool isTransform = typeinfo.GetName() == "Transform";
	Transform* transform = isTransform ? static_cast<Transform*>(instance) : nullptr;
	f32 resetValue = 0.0f;
	
	bool isPosition = false;
	bool isScale = false;

	if (isTransform)
	{
		if (property.Name == "m_Position")
		{
			lockX = transform->GetState(ETransformFlag::LockPositionX);
			lockY = transform->GetState(ETransformFlag::LockPositionY);
			lockZ = transform->GetState(ETransformFlag::LockPositionZ);
			isPosition = true;
		}
		else if (property.Name == "m_Scale")
		{
			lockX = transform->GetState(ETransformFlag::LockScaleX);
			lockY = transform->GetState(ETransformFlag::LockScaleY);
			lockZ = transform->GetState(ETransformFlag::LockScaleZ);
			resetValue = 1.0f;
			isScale = true;
		}		
	}

	bool changed = DrawVec3Control(property.Name, *value, lockX, lockY, lockZ, resetValue);

	if(changed && transform)
	{
		if(isPosition) 
		{
			transform->SetState(ETransformFlag::LockPositionX, lockX);
			transform->SetState(ETransformFlag::LockPositionY, lockY);
			transform->SetState(ETransformFlag::LockPositionZ, lockZ);
			transform->SetPosition(*value);
		}
		else if(isScale)
		{
			transform->SetState(ETransformFlag::LockScaleX, lockX);
			transform->SetState(ETransformFlag::LockScaleY, lockY);
			transform->SetState(ETransformFlag::LockScaleZ, lockZ);
			transform->SetScale(*value);
		}
	}

}
void InspectorPanel::DrawVector4Property(void* instance, void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	vec4* value = reinterpret_cast<vec4*>(data);
	DrawVec4Control(property.Name, *value);
}
void InspectorPanel::DrawQuaternionProperty(void* instance, void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	quat* qValue = reinterpret_cast<quat*>(data);
	bool lockX = false, lockY = false, lockZ = false;
	bool isTransform = typeinfo.GetName() == "Transform";
	bool isRotation = property.Name == "m_Rotation";
	Transform* transform = isTransform ? static_cast<Transform*>(instance) : nullptr;

	if (isTransform && isRotation)
	{
		Transform* transform = static_cast<Transform*>(instance);

		// 1. 읽기: 쿼터니언 변환값 대신, 저장된 'EulerHint(Local)'를 가져옴 (90도 반전 해결)
		// Transform에 GetEulerHint() getter가 필요합니다.
		vec3 displayEuler = transform->GetLocalRotationEuler();
		lockX = transform->GetState(ETransformFlag::LockRotationX);
		lockY = transform->GetState(ETransformFlag::LockRotationY);
		lockZ = transform->GetState(ETransformFlag::LockRotationZ);

		// 2. UI 그리기
		bool changed = DrawVec3Control(property.Name, displayEuler, lockX, lockY, lockZ);

		// 3. 쓰기: 값이 변경되었는지 확인
		// (DrawVec3Control이 변경 여부를 bool로 반환하게 수정하거나, 값 비교)
		vec3 currentHint = transform->GetLocalRotationEuler();
		if (glm::distance(displayEuler, currentHint) > 0.001f || changed)
		{
			// [중요] 단순 대입이 아니라, Dirty Flag 등을 처리하는 함수 호출
			transform->SetState(ETransformFlag::LockRotationX, lockX);
			transform->SetState(ETransformFlag::LockRotationY, lockY);
			transform->SetState(ETransformFlag::LockRotationZ, lockZ);
			transform->SetRotation(displayEuler);
		}
	}
	else
	{
		// [일반적인 쿼터니언] (Transform 아님)
		// 기존처럼 매번 계산해서 보여줌 (반전 현상 있을 수 있음)
		vec3 eulerDegree = glm::degrees(glm::eulerAngles(*qValue));

		DrawVec3Control(property.Name, eulerDegree, lockX, lockY, lockZ);

		// 변경 감지 및 적용
		vec3 currentEuler = glm::degrees(glm::eulerAngles(*qValue));
		if (glm::distance(eulerDegree, currentEuler) > 0.001f)
		{
			*qValue = glm::quat(glm::radians(eulerDegree));
		}
	}
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


