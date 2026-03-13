#pragma once

#include "PropertyDrawer.h"
#include "InspectorHelper.h"
#include "LocalizationManager.h"

static vector<string> axisLabels = { "X", "Y", "Z", "W" };

static map<string, ImVec4> buttonColors = {
	{ "X", ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f } },
	{ "Y", ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f } },
	{ "Z", ImVec4{ 0.1f, 0.3f, 0.8f, 1.0f } },
	{ "W", ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f } }
};

static map<string, ImVec4> buttonHoverColors = {
	{ "X", ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f } },
	{ "Y", ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f } },
	{ "Z", ImVec4{ 0.2f, 0.3f, 0.9f, 1.0f } },
	{ "W", ImVec4{ 0.6f, 0.6f, 0.6f, 1.0f } }
};

#pragma region VarName & Label Sanitization
string PropertyDrawer::SanitizeVarName(const string& varName)
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
	return cleanName;
}

string PropertyDrawer::SanitizeDisplayLabel(const TypeInfo& typeInfo, const PropertyInfo& property)
{
	string lookupKey = GetMetadataString(property.Metadata, MetaNameHash);
	if (lookupKey.empty())
	{
		lookupKey = typeInfo.Name.data() + string(".") + property.Name.data();
	}

	string displayLabel = Engine::LocalizationManager::Get().GetText(lookupKey);
	if (displayLabel == lookupKey)
	{
		displayLabel = SanitizeVarName(property.Name.data());
	}
	return displayLabel;
}
#pragma endregion

#pragma region Property Header Node
bool PropertyDrawer::DrawHeaderNode(void* instance, const TypeInfo& typeInfo)
{
	bool opened = ImGui::CollapsingHeader(typeInfo.Name.data(), ImGuiTreeNodeFlags_DefaultOpen);
	ImGui::SameLine();
	DrawCheckbox(dynamic_cast<ActiveInterface*>(reinterpret_cast<Base*>(instance)), typeInfo);
	return opened;
}
void PropertyDrawer::DrawCheckbox(ActiveInterface* instance, const TypeInfo& typeInfo)
{
	if (!instance) return;

	float checkboxSize = ImGui::GetFrameHeight();
	float windowWidth = ImGui::GetWindowContentRegionMax().x;
	float stylePadding = ImGui::GetStyle().FramePadding.x;

	ImGui::SetCursorPosX(windowWidth - checkboxSize - stylePadding);
	bool isActive = instance->IsActive();

	ImGui::PushID("IsActiveCheckbox");

	if (ImGui::Checkbox("##IsActive", &isActive))
	{
		instance->SetActive(isActive);
	}
	ImGui::PopID();
}
#pragma endregion

bool PropertyDrawer::DrawPropertyTable(void* instance, const TypeInfo& typeInfo, const vector<const Engine::PropertyInfo*>& props)
{
	bool changed = false;
	if (props.empty()) return false;

	ImGui::BeginTable("PropertyTable", 2, ImGuiTableFlags_BordersInner | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp);

	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.35f);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);

	for (const auto* propPtr : props)
	{
		const auto& prop = *propPtr;
		MetaEditCondition* editCondition = GetMetadataEditCondition(prop.Metadata, MetaEditConditionHash);
		if (!editCondition) continue;

		string displayLabel = SanitizeDisplayLabel(typeInfo, prop);


		ImGui::PushID(displayLabel.c_str());
		ImGui::TableNextRow();


		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%s", displayLabel.c_str());

		string toolTip = GetMetadataTooltip(prop.Metadata);
		if (!toolTip.empty())
		{
			ImGui::SetTooltip("%s", toolTip.c_str());
		}

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);

		bool isReadOnly = GetMetadataReadOnly(prop.Metadata);
		if (isReadOnly) ImGui::BeginDisabled();

		if (isReadOnly) ImGui::EndDisabled();

		ImGui::PopID();
	}

	ImGui::EndTable();

	return changed;
}

bool PropertyDrawer::DrawProperty(void* instance, void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	switch (property.TypeInfo.Type)
	{
	case EPropertyType::Int32:
	case EPropertyType::Int64:
	case EPropertyType::UInt32:
	case EPropertyType::UInt64:		return DrawIntegerProperty(data, property);

	case EPropertyType::F64:
	case EPropertyType::F32:
	case EPropertyType::Double:
	case EPropertyType::Float:		return DrawFloatProperty(data, typeinfo, property);

	case EPropertyType::Bool:		return DrawBooleanProperty(data, typeinfo, property);

	case EPropertyType::Wstring:
	case EPropertyType::String:		return DrawStringProperty(data, typeinfo, property);

	//case EPropertyType::Map: return DrawMapProperty(data, typeinfo, property);
	case EPropertyType::List: return DrawListProperty(data, typeinfo, property);

	case EPropertyType::Vector2: 
	case EPropertyType::Vector3: 
	case EPropertyType::Vector4: 
	case EPropertyType::Quaternion: return DrawVectorProperty(data, typeinfo, property);

	case EPropertyType::Color:		return DrawColorProperty(data, typeinfo, property);

	case EPropertyType::Matrix3:
	case EPropertyType::Matrix4:	return DrawMatrixProperty(data, typeinfo, property);

	//case EPropertyType::Struct: return PropertyDrawer::DrawStructProperty(data, typeinfo, property);
	//case EPropertyType::Set: return DrawSetProperty(data, typeinfo, property);

	case EPropertyType::Enum:		return DrawEnumProperty(data, typeinfo, property);

	case EPropertyType::BitFlag:	return DrawBitFlagProperty(data, typeinfo, property);
	default:
		ImGui::Text("Unsupported Type");
		return false;
	}
}

#pragma region Property Variables

//Integer 타입(드래그로 값 조절)
bool PropertyDrawer::DrawIntegerProperty(void* data, const PropertyInfo& property)
{
	ImGuiDataType dataType;
	switch (property.TypeInfo.Type)
	{
	case EPropertyType::Int32: dataType = ImGuiDataType_S32; break;
	case EPropertyType::Int64: dataType = ImGuiDataType_S64; break;
	case EPropertyType::UInt32: dataType = ImGuiDataType_U32; break;
	case EPropertyType::UInt64: dataType = ImGuiDataType_U64; break;
	default:
		ImGui::Text("Unsupported Integer Type");
		return false;
	}
	if (ImGui::DragScalar("##value", dataType, data)) return true;
	return false;
}

//Float 타입(드래그로 값 조절)
bool PropertyDrawer::DrawFloatProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	MetaRange* range = GetMetadataRange(property.Metadata, MetaRangeHash);
	if (range)
	{
		return ImGui::SliderFloat("##value", reinterpret_cast<f32*>(data), range->Min, range->Max, "%.3f", ImGuiSliderFlags_None);
	}
	else
	{
		return ImGui::DragFloat("##value", reinterpret_cast<f32*>(data));
	}
	return false;
}

//Boolean 타입(체크박스)
bool PropertyDrawer::DrawBooleanProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	bool changed = false;
	if (ImGui::Checkbox("##value", reinterpret_cast<bool*>(data))) changed = true;
	return changed;
}

//Wstring과 String 타입(텍스트 입력)
bool PropertyDrawer::DrawStringProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	bool changed = false;

	if (property.TypeInfo.Type == EPropertyType::String)
	{
		string* strData = reinterpret_cast<string*>(data);
		char buffer[1024];
		strcpy_s(buffer, strData->c_str());
		if (ImGui::InputText("##value", buffer, sizeof(buffer)))
		{
			*strData = string(buffer);
			changed = true;
		}
	}
	else if(property.TypeInfo.Type == EPropertyType::Wstring)
	{
		wstring* wstrData = reinterpret_cast<wstring*>(data);
		string utf8Str = Engine::WStrToStr(*wstrData);
		char buffer[1024];
		strcpy_s(buffer, utf8Str.c_str());
		if (ImGui::InputText("##value", buffer, sizeof(buffer)))
		{
			*wstrData = Engine::StrToWStr(buffer);
			changed = true;
		}
	}
	else
	{
		ImGui::Text("Unsupported String Type");
	}

	return changed;
}

bool PropertyDrawer::DrawListProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	bool changed = false;
	//auto* accessor = property.TypeInfo.Accessor;
	//if (!accessor) return false;

	//uint32 size = accessor->GetSize(data);
	//string headerName = SanitizeDisplayLabel(typeinfo, property) + " [" + to_string(size) + "]###" + property.Name;
	//string innerType = property.InnerTypeName;
	//

	//if (ImGui::CollapsingHeader(headerName.c_str()))
	//{
	//	ImGui::Indent();

	//	// [1. 리스트에 새 요소 추가]
	//	if (ImGui::Button(ICON_FA_PLUS " Add Element"))
	//	{
	//		accessor->Add(data, nullptr);
	//		changed = true;
	//	}

	//	ImGui::Separator();

	//	vector<void*> elements = accessor->GetElements(data);
	//	void* toRemove = nullptr;

	//	// [2. 테이블 형태로 리스트 요소들 그리기]
	//	if (!elements.empty())
	//	{
	//		// 3개의 컬럼: Index(고정 너비), Value(확장 너비), Action(고정 너비)
	//		if (ImGui::BeginTable("##ListTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
	//		{
	//			ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 35.0f);
	//			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
	//			ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 30.0f);

	//			for (int i = 0; i < elements.size(); ++i)
	//			{
	//				ImGui::PushID(i);
	//				ImGui::TableNextRow();
	//				void* elemPtr = elements[i];

	//				// --- [Column 1] Index 표시 ---
	//				ImGui::TableNextColumn();
	//				ImGui::AlignTextToFramePadding();
	//				ImGui::Text("[%d]", i);

	//				// --- [Column 2] 값 인라인 편집기(Editor) 또는 재귀 구조체 렌더링 ---
	//				ImGui::TableNextColumn();
	//				ImGui::SetNextItemWidth(-FLT_MIN);

	//				if (innerType == "int32" || innerType == "int64" || innerType == "uint32" || innerType == "uint64" || innerType == "Int32" || innerType == "int")
	//				{
	//					if (ImGui::DragInt("##val", (int*)elemPtr)) changed = true;
	//				}
	//				else if (innerType == "float" || innerType == "F32")
	//				{
	//					if (ImGui::DragFloat("##val", (float*)elemPtr)) changed = true;
	//				}
	//				else if (innerType == "bool" || innerType == "Bool")
	//				{
	//					if (ImGui::Checkbox("##val", (bool*)elemPtr)) changed = true;
	//				}
	//				else if (innerType == "string" || innerType == "String")
	//				{
	//					string* val = static_cast<string*>(elemPtr);
	//					char buffer[1024];
	//					strcpy_s(buffer, val->c_str());
	//					if (ImGui::InputText("##val", buffer, sizeof(buffer)))
	//					{
	//						*val = string(buffer);
	//						changed = true;
	//					}
	//				}
	//				else if (innerType == "wstring" || innerType == "WString")
	//				{
	//					wstring* val = static_cast<wstring*>(elemPtr);
	//					string utf8Str = Engine::WStrToStr(*val);
	//					char buffer[1024];
	//					strcpy_s(buffer, utf8Str.c_str());
	//					if (ImGui::InputText("##val", buffer, sizeof(buffer)))
	//					{
	//						*val = Engine::StrToWStr(buffer);
	//						changed = true;
	//					}
	//				}
	//				else if (innerType == "vec2" || innerType == "Vector2")
	//				{
	//					vec2* val = static_cast<vec2*>(elemPtr);
	//					if (ImGui::DragFloat2("##val", &val->x)) changed = true;
	//				}
	//				else if (innerType == "vec3" || innerType == "Vector3")
	//				{
	//					vec3* val = static_cast<vec3*>(elemPtr);
	//					if (ImGui::DragFloat3("##val", &val->x)) changed = true;
	//				}
	//				else if (innerType == "vec4" || innerType == "Vector4")
	//				{
	//					vec4* val = static_cast<vec4*>(elemPtr);
	//					if (ImGui::DragFloat4("##val", &val->x)) changed = true;
	//				}
	//				else if (innerType == "quat" || innerType == "Quaternion")
	//				{
	//					quat* q = static_cast<quat*>(elemPtr);
	//					vec3 euler = glm::degrees(glm::eulerAngles(*q));
	//					if (ImGui::DragFloat3("##val", &euler.x))
	//					{
	//						*q = glm::quat(glm::radians(euler));
	//						changed = true;
	//					}
	//				}
	//				else
	//				{
	//					// [핵심] 일반 변수가 아닌 경우: 구조체, 클래스, 혹은 포인터(*) 일 수 있음
	//					bool isPointer = false;
	//					string cleanInnerType = innerType;

	//					// 1. 만약 포인터 타입(예: MaterialInstance*)이라면 '*' 제거
	//					if (!cleanInnerType.empty() && cleanInnerType.back() == '*')
	//					// [개선] const, *, & 제거 로직 강화 (ex: "const MyClass*" -> "MyClass")
	//					// 뒤쪽의 포인터/참조 제거
	//					while (!cleanInnerType.empty() && (cleanInnerType.back() == '*' || cleanInnerType.back() == '&' || cleanInnerType.back() == ' '))
	//					{
	//						isPointer = true;
	//						if (cleanInnerType.back() == '*') isPointer = true;
	//						cleanInnerType.pop_back();
	//					}
	//					
	//					// 앞쪽의 const/volatile 제거
	//					if (cleanInnerType.rfind("const ", 0) == 0) cleanInnerType = cleanInnerType.substr(6);
	//					if (cleanInnerType.rfind("volatile ", 0) == 0) cleanInnerType = cleanInnerType.substr(9);
	//					
	//					// 공백 제거 (Trim)
	//					if (!cleanInnerType.empty() && cleanInnerType.front() == ' ') {
	//						cleanInnerType.erase(0, cleanInnerType.find_first_not_of(' '));
	//					}

	//					// 2. 순수한 타입명으로 리플렉션 레지스트리 검색
	//					TypeInfo* innerTypeInfo = Engine::ReflectionRegistry::Get().GetType(cleanInnerType);

	//					if (innerTypeInfo)
	//					{
	//						// 3. 포인터라면 이중 포인터 해제(Dereference), 일반 구조체면 그대로 사용
	//						void* targetData = isPointer ? *static_cast<void**>(elemPtr) : elemPtr;

	//						if (targetData)
	//						{
	//							// 4. TreeNode를 이용해 접고 펼칠 수 있는 형태의 하위 패널 구성
	//							string nodeLabel = cleanInnerType + "##StructNode";
	//							if (ImGui::TreeNodeEx(nodeLabel.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
	//							{
	//								// 우리가 만들어둔 DrawDetails를 다시 호출하여 완벽한 재귀(Recursive) 렌더링 달성!
	//								changed |= DrawDetails(targetData, *innerTypeInfo);
	//								ImGui::TreePop();
	//							}
	//						}
	//						else
	//						{
	//							ImGui::TextDisabled("Empty (nullptr)");
	//						}
	//					}
	//					else
	//					{
	//						ImGui::TextDisabled("Unsupported List Type: %s", innerType.c_str());
	//					}
	//				}

	//				// --- [Column 3] 요소 삭제 액션 (X 버튼) ---
	//				ImGui::TableNextColumn();
	//				if (ImGui::Button("X", ImVec2(-1, 0)))
	//				{
	//					toRemove = elemPtr;
	//				}

	//				ImGui::PopID();
	//			}
	//			ImGui::EndTable();
	//		}
	//	}

	//	// [3. 삭제 요청 처리]
	//	if (toRemove)
	//	{
	//		accessor->Remove(data, toRemove);
	//		changed = true;
	//	}

	//	ImGui::Unindent();
	//}
	return changed;
}

//Vector 타입(Vector2, Vector3, Vector4, Quaternion) - 각 축마다 드래그로 값 조절 + 리셋 버튼 + 잠금 버튼
#pragma region Vector
bool PropertyDrawer::DrawFloatInVector(string axis, f32& value, bool& lock, ImVec2 buttonSize, ImVec2 lockSize, f32 resetValue, f32 inputWidth)
{
	bool changed = false;
	ImGui::PushID(axis.c_str());
	if (lock) ImGui::BeginDisabled();
	{
		ImGui::PushStyleColor(ImGuiCol_Button, buttonColors[axis]);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonHoverColors[axis]);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonColors[axis]);
		if (ImGui::Button(axis.c_str(), buttonSize)) { value = resetValue; changed = true; }
		ImGui::PopStyleColor(3);

		ImGui::SameLine(0, 0); // 버튼과 입력창 딱 붙이기
		ImGui::SetNextItemWidth(inputWidth);
		if (ImGui::DragFloat("##Val", &value, 0.1f, 0.0f, 0.0f, "%.2f")) changed = true;
	}
	if (lock) ImGui::EndDisabled();

	ImGui::SameLine(0, 0);
	const char* icon = lock ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN;
	if (ImGui::Button(icon, lockSize)) { lock = !lock; changed = true; } // 클릭 시 토글
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(lock ? ("Unlock " + axis).c_str() : ("Lock " + axis).c_str());

	ImGui::PopID();
	return changed;
}

bool PropertyDrawer::DrawVectorProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	bool changed = false;
	//uint32 n = 0;
	//switch (property.Type)
	//{
	//case EPropertyType::Vector2: n = 2; break;
	//case EPropertyType::Vector3: n = 3; break;
	//case EPropertyType::Vector4: n = 4; break;
	//case EPropertyType::Quaternion: n = 4; break;
	//}

	//// 1. 사이즈 및 간격 계산
	//float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
	//ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight }; // 버튼 크기

	//// 테이블 컬럼의 전체 가용 너비 가져오기
	//f32 availableWidth = ImGui::GetContentRegionAvail().x;

	//// 요소 사이의 간격 (X-Y, Y-Z, Z-W => 총 3개)
	//f32 itemSpacing = ImGui::GetStyle().ItemSpacing.x;

	//// [버튼 4개] + [간격 3개]를 전체에서 뺀 후, 4로 나누어 입력창 하나의 너비 계산
	//f32 inputWidth = (availableWidth - (buttonSize.x * n) - (itemSpacing * (n-1))) / static_cast<f32>(n);

	//// 최소 너비 보정
	//if (inputWidth < 1.0f) inputWidth = 1.0f;
	//

	//for (uint32 i = 0; i < n; ++i)
	//{
	//	bool dummyLock = false; // 각 축마다 별도의 잠금 상태를 관리하려면 이 부분을 수정해야 합니다.
	//	changed |= DrawFloatInVector(axisLabels[i], ((f32*)data)[i], dummyLock, buttonSize, buttonSize, 0.0f, inputWidth);
	//	if (i < n - 1) ImGui::SameLine(0, itemSpacing);
	//}

	return changed;
}

#pragma endregion

//Color 타입(컬러 피커)
bool PropertyDrawer::DrawColorProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	return ImGui::ColorEdit4("##Color", reinterpret_cast<f32*>(data));
}

bool PropertyDrawer::DrawMatrixProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	bool changed = false;
	

	uint32 dim = property.TypeInfo.Type == EPropertyType::Matrix3 ? 3 : 4;
	
	bool bReadOnly = GetMetadataReadOnly(property.Metadata);
	if (bReadOnly)
		ImGui::EndDisabled();

	string headerName = SanitizeDisplayLabel(typeinfo, property) + "###" + property.Name.data();
	if (ImGui::CollapsingHeader(headerName.c_str()))
	{
		if (bReadOnly)
			ImGui::BeginDisabled();

		ImGui::Indent();

		if (ImGui::BeginTable("##MatrixTable", dim, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		{
			for (uint32 i = 0; i < dim; i++)
			{
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
			}

			for (uint32 row = 0; row < dim; row++)
			{
				ImGui::TableNextRow();
				for (uint32 col = 0; col < dim; col++)
				{
					ImGui::TableNextColumn();
					uint32 index = row * dim + col;
					ImGui::PushID(index);
					ImGui::SetNextItemWidth(-FLT_MIN);
					changed |= ImGui::DragFloat("##Val", &((f32*)data)[index], 0.1f, 0.0f, 0.0f, "%.2f");
					ImGui::PopID();
				}
			}
			ImGui::EndTable();
		}

		if (!bReadOnly && ImGui::Button("Reset Identity"))
		{
			// 0으로 밀고 대각선만 1로 설정
			memset(data, 0, dim * dim * sizeof(f32));
			for (uint32 i = 0; i < dim; ++i)
			{
				((f32*)data)[i * dim + i] = 1.0f;
			}
			changed = true;
		}

		ImGui::Unindent();

		if (bReadOnly)
			ImGui::EndDisabled();
	}

	if (bReadOnly)
		ImGui::BeginDisabled();

	return changed;
}

//Enum 타입(콤보박스)
bool PropertyDrawer::DrawEnumProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	bool changed = false;

	//int64 enumValue = ReadInteger(data, property.Size);

	//const auto* enumInfo = Engine::ReflectionRegistry::Get().GetEnum(property.TypeName);
	//string previewName = "Unknown";

	//if (enumInfo)
	//{
	//	for (const auto& [name, val] : enumInfo->Entries)
	//	{
	//		if (val == enumValue) { previewName = name; break; }
	//	}
	//}

	//if(ImGui::BeginCombo("##Enum", previewName.c_str()))
	//{
	//	if (enumInfo)
	//	{
	//		for (const auto& [name, val] : enumInfo->Entries)
	//		{
	//			bool isSelected = (val == enumValue);
	//			if (ImGui::Selectable(name.c_str(), isSelected))
	//			{
	//				enumValue = val;
	//				changed = true;
	//			}
	//			if (isSelected)
	//				ImGui::SetItemDefaultFocus();
	//		}
	//	}
	//	ImGui::EndCombo();
	//}

	return changed;
}

//BitFlag 타입(콤보박스 - 다중 선택)
bool PropertyDrawer::DrawBitFlagProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	bool changed = false;

	//int64 enumValue = ReadInteger(data, property.Size);

	//const auto* enumInfo = Engine::ReflectionRegistry::Get().GetEnum(property.TypeName);
	//string previewName = enumInfo ? enumInfo->GetBitFlagsString(enumValue) : "Unknown";

	//if (ImGui::BeginCombo("##BitFlag", previewName.c_str()))
	//{
	//	if (enumInfo)
	//	{
	//		for (const auto& [name, val] : enumInfo->Entries)
	//		{
	//			bool isSelected = (enumValue & val) == val;
	//			if (ImGui::Selectable(name.c_str(), isSelected))
	//			{
	//				if (isSelected)
	//					enumValue &= ~val; // 이미 선택된 경우: 해당 비트 클리어
	//				else
	//					enumValue |= val;  // 선택되지 않은 경우: 해당 비트 세트

	//				WriteInteger(data, property.Size, enumValue);
	//				changed = true;
	//			}
	//			if (isSelected)
	//				ImGui::SetItemDefaultFocus();
	//		}
	//	}
	//	else
	//	{
	//		ImGui::TextDisabled("Enum info not found");
	//	}
	//	ImGui::EndCombo();
	//}

	return changed;
}

#pragma endregion