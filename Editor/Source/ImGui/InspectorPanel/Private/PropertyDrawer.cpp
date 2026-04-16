#pragma once

#include "PropertyDrawer.h"
#include "InspectorHelper.h"
#include "LocalizationManager.h"
#include <regex>


#pragma region Vector Axis
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
#pragma endregion

#pragma region Default Value Resolver
inline string_view GetLiteralArgs(string_view literal)
{
	const size_t l = literal.find('(');
	const size_t r = literal.rfind(')');
	if (l != string_view::npos && r != string_view::npos && r > l)
	{
		return literal.substr(l + 1, r - l - 1);
	}
	return literal;
}

inline bool ParseNumberList(string_view literal, vector<f32>& out)
{
	out.clear();
	const string args(GetLiteralArgs(literal));
	static const std::regex numberPattern(R"([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)");

	for (std::sregex_iterator it(args.begin(), args.end(), numberPattern), end; it != end; ++it)
	{
		try
		{
			out.push_back(static_cast<f32>(std::stod(it->str())));
		}
		catch (...)
		{
			return false;
		}
	}

	return !out.empty();
}

template<typename T>
bool TryGetDefaultFromMetadata(const PropertyInfo& property, T& outValue)
{
	const Engine::MetadataEntry* entry = FindMetadata(property.Metadata, MetaDefaultHash);
	if (!entry)
	{
		return false;
	}

	if constexpr (std::is_same_v<T, bool>)
	{
		if (const bool* typed = std::get_if<bool>(&entry->Value))
		{
			outValue = *typed;
			return true;
		}
	}
	else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
	{
		if (const int64* typed = std::get_if<int64>(&entry->Value))
		{
			outValue = static_cast<T>(*typed);
			return true;
		}
	}
	else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>)
	{
		if (const uint64* typed = std::get_if<uint64>(&entry->Value))
		{
			outValue = static_cast<T>(*typed);
			return true;
		}
	}
	else if constexpr (std::is_floating_point_v<T>)
	{
		if (const double* typed = std::get_if<double>(&entry->Value))
		{
			outValue = static_cast<T>(*typed);
			return true;
		}
	}

	const string literal = GetMetadataString(property.Metadata, MetaDefaultHash);
	if (literal.empty())
	{
		return false;
	}

	try
	{
		if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
		{
			outValue = static_cast<T>(std::stoll(literal));
			return true;
		}

		if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>)
		{
			outValue = static_cast<T>(std::stoull(literal));
			return true;
		}

		if constexpr (std::is_same_v<T, f32>)
		{
			outValue = static_cast<f32>(std::stod(literal));
			return true;
		}

		if constexpr (std::is_same_v<T, double>)
		{
			outValue = std::stod(literal);
			return true;
		}

		if constexpr (std::is_same_v<T, vec2>)
		{
			vector<f32> nums;
			if (!ParseNumberList(literal, nums) || nums.size() < 2)
			{
				return false;
			}

			outValue = vec2(nums[0], nums[1]);
			return true;
		}

		if constexpr (std::is_same_v<T, vec3>)
		{
			vector<f32> nums;
			if (!ParseNumberList(literal, nums) || nums.size() < 3)
			{
				return false;
			}

			outValue = vec3(nums[0], nums[1], nums[2]);
			return true;
		}

		if constexpr (std::is_same_v<T, vec4>)
		{
			vector<f32> nums;
			if (!ParseNumberList(literal, nums) || nums.size() < 4)
			{
				return false;
			}

			outValue = vec4(nums[0], nums[1], nums[2], nums[3]);
			return true;
		}

		if constexpr (std::is_same_v<T, quat>)
		{
			vector<f32> nums;
			if (!ParseNumberList(literal, nums) || nums.size() < 4)
			{
				return false;
			}

			outValue = quat(nums[3], nums[0], nums[1], nums[2]);
			return true;
		}
	}
	catch (...)
	{
	}

	return false;
}

template<typename T>
T ResolveDefaultValue(const TypeInfo& typeInfo, const PropertyInfo& property, const T& fallback)
{
	T value = fallback;
	if (TryGetDefaultFromMetadata<T>(property, value))
	{
		return value;
	}

	return fallback;
}

inline bool MatchesNormalizedTypeName(string_view rawTypeName, string_view expected)
{
	return NormalizeReflectedTypeName(rawTypeName) == expected;
}

inline bool IsVector2Property(const PropertyInfo& property)
{
	return property.TypeInfo.Type == EPropertyType::UserDefined &&
		(MatchesNormalizedTypeName(property.TypeInfo.Name, "vec2") || MatchesNormalizedTypeName(property.TypeInfo.Name, "glm::vec2"));
}

inline bool IsVector3Property(const PropertyInfo& property)
{
	return property.TypeInfo.Type == EPropertyType::UserDefined &&
		(MatchesNormalizedTypeName(property.TypeInfo.Name, "vec3") || MatchesNormalizedTypeName(property.TypeInfo.Name, "glm::vec3"));
}

inline bool IsVector4Property(const PropertyInfo& property)
{
	return property.TypeInfo.Type == EPropertyType::UserDefined &&
		(MatchesNormalizedTypeName(property.TypeInfo.Name, "vec4") || MatchesNormalizedTypeName(property.TypeInfo.Name, "glm::vec4"));
}

inline bool IsQuaternionProperty(const PropertyInfo& property)
{
	return property.TypeInfo.Type == EPropertyType::UserDefined &&
		(MatchesNormalizedTypeName(property.TypeInfo.Name, "quat") || MatchesNormalizedTypeName(property.TypeInfo.Name, "glm::quat"));
}

inline bool IsMatrix3Property(const PropertyInfo& property)
{
	return property.TypeInfo.Type == EPropertyType::UserDefined &&
		(MatchesNormalizedTypeName(property.TypeInfo.Name, "mat3") || MatchesNormalizedTypeName(property.TypeInfo.Name, "glm::mat3"));
}

inline bool IsMatrix4Property(const PropertyInfo& property)
{
	return property.TypeInfo.Type == EPropertyType::UserDefined &&
		(MatchesNormalizedTypeName(property.TypeInfo.Name, "mat4") || MatchesNormalizedTypeName(property.TypeInfo.Name, "glm::mat4"));
}

inline bool IsColorProperty(const PropertyInfo& property)
{
	return HasMetadataColor(property.Metadata) ||
		MatchesNormalizedTypeName(property.TypeInfo.Name, "Color") ||
		MatchesNormalizedTypeName(property.TypeInfo.Name, "Engine::Color");
}

inline uint32 GetVectorComponentCount(const PropertyInfo& property)
{
	if (IsVector2Property(property)) return 2;
	if (IsVector3Property(property)) return 3;
	if (IsVector4Property(property)) return 4;
	if (IsQuaternionProperty(property)) return 4;
	return 0;
}

inline uint32 GetMatrixDimension(const PropertyInfo& property)
{
	if (IsMatrix3Property(property)) return 3;
	if (IsMatrix4Property(property)) return 4;
	return 0;
}
#pragma endregion

#pragma region Vector Lock Resolver
enum class ETransformVectorGroup : uint8
{
	None,
	Position,
	Rotation,
	Scale
};

struct TransformLockMaskCache
{
	bool Initialized = false;
	bool Valid = false;
	uint64 StaticMask = 0;
	uint64 PositionMasks[3] = { 0, 0, 0 };
	uint64 RotationMasks[3] = { 0, 0, 0 };
	uint64 ScaleMasks[3] = { 0, 0, 0 };
};

static TransformLockMaskCache g_TransformLockMaskCache;
static std::unordered_map<uint64, bool> g_EditorAxisLockState;

inline uint64 MakeAxisLockKey(const void* instance, const TypeInfo& typeInfo, const PropertyInfo& property, uint32 axis)
{
	uint64 h = 14695981039346656037ull;
	auto mix = [&h](uint64 v)
		{
			h ^= v;
			h *= 1099511628211ull;
		};

	mix(reinterpret_cast<uint64>(instance));
	mix(typeInfo.ID);
	mix(property.ID);
	mix(axis);
	return h;
}

inline ETransformVectorGroup GetTransformVectorGroup(const TypeInfo& typeInfo, const PropertyInfo& property)
{
	if (typeInfo.QualifiedName != "Engine::Transform")
		return ETransformVectorGroup::None;

	if (property.Name == "m_Position")
		return ETransformVectorGroup::Position;
	if (property.Name == "m_Rotation" || property.Name == "m_EulerRotation")
		return ETransformVectorGroup::Rotation;
	if (property.Name == "m_Scale")
		return ETransformVectorGroup::Scale;

	return ETransformVectorGroup::None;
}

inline void InitializeTransformLockMasksIfNeeded()
{
	if (g_TransformLockMaskCache.Initialized)
	{
		return;
	}

	g_TransformLockMaskCache.Initialized = true;
	const EnumInfo* enumInfo = reflection::Registry::Get().GetEnumByQualifiedName("Engine::ETransformFlag");
	if (!enumInfo)
	{
		return;
	}

	for (const auto& entry : enumInfo->Entries)
	{
		const string_view name = entry.Name;
		const uint64 val = entry.Value;

		if (name == "Static")        g_TransformLockMaskCache.StaticMask = val;
		if (name == "LockPositionX") g_TransformLockMaskCache.PositionMasks[0] = val;
		if (name == "LockPositionY") g_TransformLockMaskCache.PositionMasks[1] = val;
		if (name == "LockPositionZ") g_TransformLockMaskCache.PositionMasks[2] = val;
		if (name == "LockRotationX") g_TransformLockMaskCache.RotationMasks[0] = val;
		if (name == "LockRotationY") g_TransformLockMaskCache.RotationMasks[1] = val;
		if (name == "LockRotationZ") g_TransformLockMaskCache.RotationMasks[2] = val;
		if (name == "LockScaleX")    g_TransformLockMaskCache.ScaleMasks[0] = val;
		if (name == "LockScaleY")    g_TransformLockMaskCache.ScaleMasks[1] = val;
		if (name == "LockScaleZ")    g_TransformLockMaskCache.ScaleMasks[2] = val;
	}

	g_TransformLockMaskCache.Valid = true;
}

inline uint64 GetAxisMask(ETransformVectorGroup group, uint32 axis)
{
	if (axis >= 3)
		return 0;

	switch (group)
	{
	case ETransformVectorGroup::Position: return g_TransformLockMaskCache.PositionMasks[axis];
	case ETransformVectorGroup::Rotation: return g_TransformLockMaskCache.RotationMasks[axis];
	case ETransformVectorGroup::Scale:    return g_TransformLockMaskCache.ScaleMasks[axis];
	default: return 0;
	}
}

inline bool GetTransformAxisLock(void* instance, const TypeInfo& typeInfo, ETransformVectorGroup group, uint32 axis)
{
	InitializeTransformLockMasksIfNeeded();
	if (!g_TransformLockMaskCache.Valid)
		return false;

	const PropertyInfo* flagsProp = FindPropertyByName(typeInfo, "m_Flags");
	if (!flagsProp)
		return false;

	void* flagsPtr = reinterpret_cast<uint8_t*>(instance) + flagsProp->Offset;
	const uint64 flags = ReadUnsignedInteger(flagsPtr, flagsProp->Size);

	if (g_TransformLockMaskCache.StaticMask != 0 &&
		(flags & g_TransformLockMaskCache.StaticMask) == g_TransformLockMaskCache.StaticMask)
	{
		return true;
	}

	const uint64 axisMask = GetAxisMask(group, axis);
	return axisMask != 0 && (flags & axisMask) == axisMask;
}

inline void SetTransformAxisLock(void* instance, const TypeInfo& typeInfo, ETransformVectorGroup group, uint32 axis, bool locked)
{
	InitializeTransformLockMasksIfNeeded();
	if (!g_TransformLockMaskCache.Valid)
		return;

	const PropertyInfo* flagsProp = FindPropertyByName(typeInfo, "m_Flags");
	if (!flagsProp)
		return;

	const uint64 axisMask = GetAxisMask(group, axis);
	if (axisMask == 0)
		return;

	void* flagsPtr = reinterpret_cast<uint8_t*>(instance) + flagsProp->Offset;
	uint64 flags = ReadUnsignedInteger(flagsPtr, flagsProp->Size);

	if (locked) flags |= axisMask;
	else flags &= ~axisMask;

	WriteUnsignedInteger(flagsPtr, flagsProp->Size, flags);
}

inline bool GetEditorAxisLock(const void* instance, const TypeInfo& typeInfo, const PropertyInfo& property, uint32 axis)
{
	const uint64 key = MakeAxisLockKey(instance, typeInfo, property, axis);
	auto it = g_EditorAxisLockState.find(key);
	return (it != g_EditorAxisLockState.end()) ? it->second : false;
}

inline void SetEditorAxisLock(const void* instance, const TypeInfo& typeInfo, const PropertyInfo& property, uint32 axis, bool locked)
{
	const uint64 key = MakeAxisLockKey(instance, typeInfo, property, axis);
	g_EditorAxisLockState[key] = locked;
}
#pragma endregion

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
		lookupKey = typeInfo.QualifiedName.data() + string(".") + property.Name.data();
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
	ImGui::SetNextItemAllowOverlap();
	bool opened = ImGui::CollapsingHeader(typeInfo.QualifiedName.data(), ImGuiTreeNodeFlags_DefaultOpen);
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

	ImGui::BeginTable("PropertyTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp);

	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.25f);
	ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.75f);

	for (const auto* propPtr : props)
	{
		const auto& prop = *propPtr;
		if (!CheckEditCondition(instance, typeInfo, prop.Metadata))
		{
			continue;
		}

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
		void* data = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(instance) + prop.Offset);
		bool propertyChanged = DrawProperty(instance, data, typeInfo, prop);
		if (propertyChanged)
		{
			changed = true;
			InvokeOnChanged(instance, typeInfo, prop.Metadata);
		}
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
	case EPropertyType::Int8:
	case EPropertyType::Int16:
	case EPropertyType::Int32:
	case EPropertyType::Int64:
	case EPropertyType::UInt8:
	case EPropertyType::UInt16:
	case EPropertyType::UInt32:
	case EPropertyType::UInt64:
		return DrawIntegerProperty(data, typeinfo, property);

	case EPropertyType::Float32:
	case EPropertyType::Float64:
		return DrawFloatProperty(data, typeinfo, property);

	case EPropertyType::Bool:
		return DrawBooleanProperty(data, typeinfo, property);

	case EPropertyType::String:
	case EPropertyType::WString:
		return DrawStringProperty(data, typeinfo, property);

	case EPropertyType::Array:
	case EPropertyType::List:
		return DrawListProperty(data, typeinfo, property);

	case EPropertyType::Enum:
		return DrawEnumProperty(data, typeinfo, property);

	case EPropertyType::BitFlag:
		return DrawBitFlagProperty(data, typeinfo, property);

	case EPropertyType::ResourceHandle:
		return DrawResourceHandleProperty(data, typeinfo, property);

	case EPropertyType::UserDefined:
		if (IsColorProperty(property))
		{
			return DrawColorProperty(data, typeinfo, property);
		}

		if (GetVectorComponentCount(property) != 0)
		{
			return DrawVectorProperty(instance, data, typeinfo, property);
		}

		if (GetMatrixDimension(property) != 0)
		{
			return DrawMatrixProperty(data, typeinfo, property);
		}

		break;

	default:
		break;
	}

	ImGui::Text("Unsupported Type");
	return false;
}

#pragma region Property Variables

//Integer 타입(드래그로 값 조절)
bool PropertyDrawer::DrawIntegerProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	ImGuiDataType dataType = ImGuiDataType_S32;

	switch (property.TypeInfo.Type)
	{
	case EPropertyType::Int8:   dataType = ImGuiDataType_S8; break;
	case EPropertyType::Int16:  dataType = ImGuiDataType_S16; break;
	case EPropertyType::Int32:  dataType = ImGuiDataType_S32; break;
	case EPropertyType::Int64:  dataType = ImGuiDataType_S64; break;
	case EPropertyType::UInt8:  dataType = ImGuiDataType_U8; break;
	case EPropertyType::UInt16: dataType = ImGuiDataType_U16; break;
	case EPropertyType::UInt32: dataType = ImGuiDataType_U32; break;
	case EPropertyType::UInt64: dataType = ImGuiDataType_U64; break;
	default:
		ImGui::Text("Unsupported Integer Type");
		return false;
	}

	bool changed = ImGui::DragScalar("##value", dataType, data);

	const auto range = GetMetadataRange(property.Metadata);
	const bool hasRange = range.has_value();

	switch (property.TypeInfo.Type)
	{
	case EPropertyType::Int8:
	{
		const int8_t resetValue = ResolveDefaultValue<int8_t>(typeinfo, property, 0);
		changed |= ApplyMouseWheelInput(reinterpret_cast<int8_t*>(data), resetValue, 1.0f, 10.0f, hasRange,
			hasRange ? static_cast<int8_t>(range->Min) : static_cast<int8_t>(0),
			hasRange ? static_cast<int8_t>(range->Max) : static_cast<int8_t>(0));
		break;
	}
	case EPropertyType::Int16:
	{
		const int16_t resetValue = ResolveDefaultValue<int16_t>(typeinfo, property, 0);
		changed |= ApplyMouseWheelInput(reinterpret_cast<int16_t*>(data), resetValue, 1.0f, 10.0f, hasRange,
			hasRange ? static_cast<int16_t>(range->Min) : static_cast<int16_t>(0),
			hasRange ? static_cast<int16_t>(range->Max) : static_cast<int16_t>(0));
		break;
	}
	case EPropertyType::Int32:
	{
		const int32_t resetValue = ResolveDefaultValue<int32_t>(typeinfo, property, 0);
		changed |= ApplyMouseWheelInput(reinterpret_cast<int32_t*>(data), resetValue, 1.0f, 10.0f, hasRange,
			hasRange ? static_cast<int32_t>(range->Min) : static_cast<int32_t>(0),
			hasRange ? static_cast<int32_t>(range->Max) : static_cast<int32_t>(0));
		break;
	}
	case EPropertyType::Int64:
	{
		const int64_t resetValue = ResolveDefaultValue<int64_t>(typeinfo, property, 0);
		changed |= ApplyMouseWheelInput(reinterpret_cast<int64_t*>(data), resetValue, 1.0f, 10.0f, hasRange,
			hasRange ? static_cast<int64_t>(range->Min) : static_cast<int64_t>(0),
			hasRange ? static_cast<int64_t>(range->Max) : static_cast<int64_t>(0));
		break;
	}
	case EPropertyType::UInt8:
	{
		const uint8_t resetValue = ResolveDefaultValue<uint8_t>(typeinfo, property, 0);
		changed |= ApplyMouseWheelInput(reinterpret_cast<uint8_t*>(data), resetValue, 1.0f, 10.0f, hasRange,
			hasRange ? static_cast<uint8_t>(range->Min) : static_cast<uint8_t>(0),
			hasRange ? static_cast<uint8_t>(range->Max) : static_cast<uint8_t>(0));
		break;
	}
	case EPropertyType::UInt16:
	{
		const uint16_t resetValue = ResolveDefaultValue<uint16_t>(typeinfo, property, 0);
		changed |= ApplyMouseWheelInput(reinterpret_cast<uint16_t*>(data), resetValue, 1.0f, 10.0f, hasRange,
			hasRange ? static_cast<uint16_t>(range->Min) : static_cast<uint16_t>(0),
			hasRange ? static_cast<uint16_t>(range->Max) : static_cast<uint16_t>(0));
		break;
	}
	case EPropertyType::UInt32:
	{
		const uint32_t resetValue = ResolveDefaultValue<uint32_t>(typeinfo, property, 0u);
		changed |= ApplyMouseWheelInput(reinterpret_cast<uint32_t*>(data), resetValue, 1.0f, 10.0f, hasRange,
			hasRange ? static_cast<uint32_t>(range->Min) : static_cast<uint32_t>(0),
			hasRange ? static_cast<uint32_t>(range->Max) : static_cast<uint32_t>(0));
		break;
	}
	case EPropertyType::UInt64:
	{
		const uint64_t resetValue = ResolveDefaultValue<uint64_t>(typeinfo, property, 0ull);
		changed |= ApplyMouseWheelInput(reinterpret_cast<uint64_t*>(data), resetValue, 1.0f, 10.0f, hasRange,
			hasRange ? static_cast<uint64_t>(range->Min) : static_cast<uint64_t>(0),
			hasRange ? static_cast<uint64_t>(range->Max) : static_cast<uint64_t>(0));
		break;
	}
	default:
		break;
	}

	return changed;
}

//Float 타입(드래그로 값 조절)
bool PropertyDrawer::DrawFloatProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	const auto range = GetMetadataRange(property.Metadata);

	if (property.TypeInfo.Type == EPropertyType::Float64)
	{
		bool changed = false;
		double* value = reinterpret_cast<double*>(data);
		const double resetValue = ResolveDefaultValue<double>(typeinfo, property, 0.0);

		if (range.has_value())
		{
			const double minValue = static_cast<double>(range->Min);
			const double maxValue = static_cast<double>(range->Max);

			changed |= ImGui::SliderScalar("##value", ImGuiDataType_Double, value, &minValue, &maxValue, "%.3f");
			changed |= ApplyMouseWheelInput(value, resetValue, range->Speed, range->Speed * 10.0f, true, minValue, maxValue);
		}
		else
		{
			changed |= ImGui::DragScalar("##value", ImGuiDataType_Double, value);
			changed |= ApplyMouseWheelInput(value, resetValue, 0.1f, 1.0f);
		}

		return changed;
	}

	bool changed = false;
	f32* value = reinterpret_cast<f32*>(data);
	const f32 resetValue = ResolveDefaultValue<f32>(typeinfo, property, 0.0f);

	if (range.has_value())
	{
		changed |= ImGui::SliderFloat("##value", value, range->Min, range->Max, "%.3f", ImGuiSliderFlags_None);
		changed |= ApplyMouseWheelInput(value, resetValue, range->Speed, range->Speed * 10.0f, true, range->Min, range->Max);
	}
	else
	{
		changed |= ImGui::DragFloat("##value", value);
		changed |= ApplyMouseWheelInput(value, resetValue, 0.1f, 1.0f);
	}

	return changed;
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
	else if (property.TypeInfo.Type == EPropertyType::WString)
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
		changed |= ImGui::DragFloat("##Val", &value, 0.1f, 0.0f, 0.0f, "%.2f");
		changed |= ApplyMouseWheelInput(&value, resetValue, 0.1f, 10.0f);
	}
	if (lock) ImGui::EndDisabled();

	ImGui::SameLine(0, 0);
	const char* icon = lock ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN;
	if (ImGui::Button(icon, lockSize)) { lock = !lock; changed = true; } // 클릭 시 토글
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(lock ? ("Unlock " + axis).c_str() : ("Lock " + axis).c_str());

	ImGui::PopID();
	return changed;
}

bool PropertyDrawer::DrawVectorProperty(void* instance, void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	bool changed = false;
	const uint32 n = GetVectorComponentCount(property);
	if (n == 0)
	{
		ImGui::Text("Unsupported Vector Type");
		return false;
	}

	std::array<f32, 4> defaults = { 0.f, 0.f, 0.f, 0.f };

	if (IsVector2Property(property))
	{
		const vec2 d = ResolveDefaultValue<vec2>(typeinfo, property, vec2(0.f));
		defaults[0] = d.x;
		defaults[1] = d.y;
	}
	else if (IsVector3Property(property))
	{
		const vec3 d = ResolveDefaultValue<vec3>(typeinfo, property, vec3(0.f));
		defaults[0] = d.x;
		defaults[1] = d.y;
		defaults[2] = d.z;
	}
	else if (IsVector4Property(property))
	{
		const vec4 d = ResolveDefaultValue<vec4>(typeinfo, property, vec4(0.f));
		defaults[0] = d.x;
		defaults[1] = d.y;
		defaults[2] = d.z;
		defaults[3] = d.w;
	}
	else if (IsQuaternionProperty(property))
	{
		const quat d = ResolveDefaultValue<quat>(typeinfo, property, glm::identity<quat>());
		defaults[0] = d.x;
		defaults[1] = d.y;
		defaults[2] = d.z;
		defaults[3] = d.w;
	}

	float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
	ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

	f32 availableWidth = ImGui::GetContentRegionAvail().x;
	f32 itemSpacing = ImGui::GetStyle().ItemSpacing.x;
	f32 totalButtonWidth = static_cast<f32>(n) * buttonSize.x * 2.0f;
	f32 totalExternalSpacing = static_cast<f32>(n - 1) * itemSpacing;
	f32 inputWidth = (availableWidth - totalButtonWidth - totalExternalSpacing) / static_cast<f32>(n);

	if (inputWidth < 1.0f)
	{
		inputWidth = 1.0f;
	}

	const ETransformVectorGroup transformGroup = GetTransformVectorGroup(typeinfo, property);
	const bool useTransformLock = (transformGroup != ETransformVectorGroup::None);

	for (uint32 i = 0; i < n; ++i)
	{
		bool axisLock = false;

		if (useTransformLock && i < 3)
		{
			axisLock = GetTransformAxisLock(instance, typeinfo, transformGroup, i);
		}
		else
		{
			axisLock = GetEditorAxisLock(instance, typeinfo, property, i);
		}

		const bool prevLock = axisLock;
		changed |= DrawFloatInVector(axisLabels[i], reinterpret_cast<f32*>(data)[i], axisLock, buttonSize, buttonSize, defaults[i], inputWidth);

		if (axisLock != prevLock)
		{
			if (useTransformLock && i < 3)
			{
				SetTransformAxisLock(instance, typeinfo, transformGroup, i, axisLock);
			}
			else
			{
				SetEditorAxisLock(instance, typeinfo, property, i, axisLock);
			}

			changed = true;
		}

		if (i < n - 1)
		{
			ImGui::SameLine(0, itemSpacing);
		}
	}

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
	const uint32 dim = GetMatrixDimension(property);
	if (dim == 0)
	{
		ImGui::Text("Unsupported Matrix Type");
		return false;
	}

	bool bReadOnly = GetMetadataReadOnly(property.Metadata);
	if (bReadOnly)
	{
		ImGui::EndDisabled();
	}

	string headerName = SanitizeDisplayLabel(typeinfo, property) + "###" + property.Name.data();
	if (ImGui::CollapsingHeader(headerName.c_str()))
	{
		if (bReadOnly)
		{
			ImGui::BeginDisabled();
		}

		ImGui::Indent();

		if (ImGui::BeginTable("##MatrixTable", dim, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		{
			for (uint32 i = 0; i < dim; ++i)
			{
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
			}

			for (uint32 row = 0; row < dim; ++row)
			{
				ImGui::TableNextRow();
				for (uint32 col = 0; col < dim; ++col)
				{
					ImGui::TableNextColumn();
					const uint32 index = row * dim + col;

					ImGui::PushID(index);
					ImGui::SetNextItemWidth(-FLT_MIN);
					changed |= ImGui::DragFloat("##Val", &reinterpret_cast<f32*>(data)[index], 0.1f, 0.0f, 0.0f, "%.2f");
					ImGui::PopID();
				}
			}

			ImGui::EndTable();
		}

		if (!bReadOnly && ImGui::Button("Reset Identity"))
		{
			memset(data, 0, dim * dim * sizeof(f32));
			for (uint32 i = 0; i < dim; ++i)
			{
				reinterpret_cast<f32*>(data)[i * dim + i] = 1.0f;
			}
			changed = true;
		}

		ImGui::Unindent();

		if (bReadOnly)
		{
			ImGui::EndDisabled();
		}
	}

	if (bReadOnly)
	{
		ImGui::BeginDisabled();
	}

	return changed;
}

//Enum 타입(콤보박스)
bool PropertyDrawer::DrawEnumProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	uint64 enumValue = ReadUnsignedInteger(data, property.Size);

	const EnumInfo* enumInfo = ResolveEnumInfo(property.TypeInfo.Name, &typeinfo);
	string previewName = GetEnumPreviewName(enumValue, enumInfo);

	bool changed = false;

	if (ImGui::BeginCombo("##Enum", previewName.c_str()))
	{
		if (enumInfo)
		{
			for (const auto& entry : enumInfo->Entries)
			{
				if (!ShouldDisplayEnumEntry(entry))
				{
					continue;
				}

				const bool isSelected = (entry.Value == enumValue);
				if (ImGui::Selectable(string(entry.Name).c_str(), isSelected))
				{
					WriteUnsignedInteger(data, property.Size, entry.Value);
					changed = true;
				}

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
		}
		else
		{
			ImGui::TextDisabled("Enum info not found");
		}

		ImGui::EndCombo();
	}

	return changed;
}

//BitFlag 타입(콤보박스 - 다중 선택)
bool PropertyDrawer::DrawBitFlagProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	uint64 enumValue = ReadUnsignedInteger(data, property.Size);

	const EnumInfo* enumInfo = ResolveEnumInfo(property.TypeInfo.Name, &typeinfo);
	string previewName = GetBitFlagPreviewName(enumValue, enumInfo);

	bool changed = false;

	if (ImGui::BeginCombo("##BitFlag", previewName.c_str()))
	{
		if (enumInfo)
		{
			for (const auto& entry : enumInfo->Entries)
			{
				if (!ShouldDisplayBitFlagEntry(entry))
				{
					continue;
				}

				const bool isSelected = (entry.Value != 0) && ((enumValue & entry.Value) == entry.Value);
				bool selected = isSelected;

				if (entry.Value == 0)
				{
					selected = (enumValue == 0);
				}

				if (ImGui::Selectable(string(entry.Name).c_str(), selected, ImGuiSelectableFlags_DontClosePopups))
				{
					if (entry.Value == 0)
					{
						enumValue = 0;
					}
					else
					{
						if (selected)
						{
							enumValue &= ~entry.Value;
						}
						else
						{
							enumValue |= entry.Value;
						}
					}

					WriteUnsignedInteger(data, property.Size, enumValue);
					changed = true;
				}
			}
		}
		else
		{
			ImGui::TextDisabled("Enum info not found");
		}

		ImGui::EndCombo();
	}

	return changed;
}

bool PropertyDrawer::DrawResourceHandleProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property)
{
	// 템플릿 T에 관계없이 ResourceHandle<T>는 내부적으로 Handle 클래스와 동일한 레이아웃을 가짐
	Engine::Handle* handleData = reinterpret_cast<Engine::Handle*>(data);

	// 타겟 리소스 타입 추출 (e.g. "ResourceHandle<Texture>" -> "Texture")
	string typeNameStr(property.TypeInfo.Name);
	size_t start = typeNameStr.find('<');
	size_t end = typeNameStr.find('>');
	string targetResourceType = "Unknown";
	if (start != string::npos && end != string::npos)
	{
		targetResourceType = typeNameStr.substr(start + 1, end - start - 1);
	}

	// 현재 할당된 데이터 Key (이름) 가져오기
	string currentAssetKey = "None";
	if (handleData->IsValid())
	{
		if (Engine::Resource* res = Engine::ResourceManager::Get().GetResource(*handleData))
		{
			currentAssetKey = Engine::WStrToStr(res->GetKey());
		}
	}

	bool changed = false;
	ImGui::PushID(data); // 변수 주소 기반 고유 ID

	// -- 1. 읽기 전용 텍스트 필드로 할당된 에셋 이름 시각화 --
	float availWidth = ImGui::GetContentRegionAvail().x;
	float buttonWidth = 30.0f; // [ O ] 버튼 영역

	// 핸들이 유효하면 글씨를 불투명하게, 아니면 반투명(회색)으로 표시
	ImGui::PushStyleColor(ImGuiCol_Text, handleData->IsValid() ? ImVec4(1, 1, 1, 1) : ImVec4(0.5f, 0.5f, 0.5f, 1));
	ImGui::PushItemWidth(availWidth - buttonWidth - ImGui::GetStyle().ItemSpacing.x);
	ImGui::InputText("##ResourceHandleName", (char*)currentAssetKey.c_str(), currentAssetKey.capacity() + 1, ImGuiInputTextFlags_ReadOnly);
	ImGui::PopItemWidth();
	ImGui::PopStyleColor();

	// -- 2. 드래그 앤 드롭 타겟 (Drag & Drop) 처리 --
	if (ImGui::BeginDragDropTarget())
	{
		// 콘텐츠 브라우저에서 사용할 페이로드 정의 (예: "CONTENT_ITEM_Texture")
		string payloadType = "CONTENT_ITEM_" + targetResourceType;
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadType.c_str()))
		{
			// TODO: 넘어온 wchar_t* (또는 ID)를 기반으로 Handle을 찾아 교체
			// wstring* incomingKey = (wstring*)payload->Data;
			// *handleData = ResourceManager::Get().GetResourceHandle(*incomingKey);
			// changed = true;
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::SameLine();

	// -- 3. 선택/필터 팝업 창 --
	if (ImGui::Button(ICON_FA_BULLSEYE, ImVec2(buttonWidth, 0)))
	{
		ImGui::OpenPopup("ResourcePickerPopup");
	}

	// 팝업 내용
	if (ImGui::BeginPopup("ResourcePickerPopup"))
	{
		ImGui::Text("Select %s", targetResourceType.c_str());
		ImGui::Separator();

		// 선택 해제 UI
		if (ImGui::Selectable("None (Clear)"))
		{
			*handleData = Engine::Handle(); // 핸들 무효화
			changed = true;
		}

		uint64 typeHash = 0;

		const string normalizedTargetType = NormalizeReflectedTypeName(targetResourceType);

		const Engine::TypeInfo* targetTypeInfo = reflection::Registry::Get().ResolveTypeName(normalizedTargetType);
		if (!targetTypeInfo && !normalizedTargetType.empty())
		{
			targetTypeInfo = reflection::Registry::Get().GetTypeByQualifiedName("Engine::" + normalizedTargetType);
		}

		vector<Engine::Handle> emptyList;
		const vector<Engine::Handle>* handleList = &emptyList;

		if (targetTypeInfo)
		{
			handleList = &Engine::ResourceManager::Get().GetResourceHandles(targetTypeInfo->ID);
		}

		for (const Engine::Handle& handle : *handleList)
		{
			if (Engine::Resource* res = Engine::ResourceManager::Get().GetResource(handle))
			{
				string keyStr = Engine::WStrToStr(res->GetKey());
				bool isSelected = (*handleData == handle);

				if (ImGui::Selectable(keyStr.c_str(), isSelected))
				{
					*handleData = handle;
					changed = true;
				}

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
		}

		// [TODO] ResourceManager에 등록된 해당 'targetResourceType'을 순회하며 리스트업
		// ImGui::Selectable("Asset_uv1") -> 클릭 시 handleData를 해당 에셋의 핸들로 교체!

		ImGui::EndPopup();
	}

	ImGui::PopID();
	return changed;
}

template <typename T>
bool PropertyDrawer::ApplyMouseWheelInput(T* value, T resetValue, float step, float shiftMultiplier, bool hasRange, T minVal, T maxVal)
{
	bool changed = false;
	
	if (ImGui::IsItemHovered())
	{
		f32 scrollDelta = MOUSE_SCROLL.y;

		if (scrollDelta != 0.0f)
		{
			float currentStep = ImGui::GetIO().KeyShift ? (step * shiftMultiplier) : step;
			*value = static_cast<T>(*value + (ImGui::GetIO().MouseWheel * currentStep));
			ImGui::GetIO().MouseWheel = 0.0f; // 이벤트 소비 (스크롤 방지)
			changed = true;
		}

		
		if (MOUSE_BUTTON_DOWN(EMouseButton::Middle))
		{
			*value = resetValue;
			changed = true;
		}
	}

	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem("Reset to Default"))
		{
			*value = resetValue;
			changed = true;
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Copy Value"))
		{
			ImGui::SetClipboardText(std::to_string(*value).c_str());
		}

		if (ImGui::MenuItem("Paste Value"))
		{
			const char* clipText = ImGui::GetClipboardText();
			if (clipText)
			{
				try
				{
					// 문자열을 타입에 맞게 안전하게 변환
					if constexpr (std::is_floating_point_v<T>)
						*value = static_cast<T>(std::stod(clipText));
					else if constexpr (std::is_unsigned_v<T>)
						*value = static_cast<T>(std::stoull(clipText));
					else
						*value = static_cast<T>(std::stoll(clipText));

					changed = true;
				}
				catch (...) {} // 클립보드에 숫자가 아닌 문자가 있으면 안전하게 무시
			}
		}
		ImGui::EndPopup();
	}

	// 3. 변경 사항이 있을 때 범위(Range) 제한 (Clamp)
	if (changed && hasRange)
	{
		*value = std::clamp(*value, minVal, maxVal);
	}
	return changed;
}
#pragma endregion