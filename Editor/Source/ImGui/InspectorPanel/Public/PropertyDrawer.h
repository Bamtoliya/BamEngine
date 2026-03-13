#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class PropertyDrawer
{
#pragma region VarName & Label Sanitization
public:
	static string SanitizeVarName(const string& varName);
	static string SanitizeDisplayLabel(const TypeInfo& typeInfo, const PropertyInfo& property);
#pragma endregion

#pragma region Property Header Node
public:
	static bool DrawHeaderNode(void* instance, const TypeInfo& typeInfo);
private:
	static void DrawCheckbox(ActiveInterface* instance, const TypeInfo& typeInfo);
#pragma endregion

public:
	static bool DrawPropertyTable(void* instance, const TypeInfo& typeInfo, const vector<const Engine::PropertyInfo*>& props);
	static bool DrawProperty(void* instance, void* data, const TypeInfo& typeinfo, const PropertyInfo& property);

#pragma region Property Variables
public:
	static bool DrawIntegerProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	static bool DrawFloatProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	static bool DrawBooleanProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	static bool DrawStringProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	//static bool DrawMapProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	static bool DrawListProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	static bool DrawFloatInVector(string axis, f32& value, bool& lock, ImVec2 buttonSize, ImVec2 lockSize, f32 resetValue, f32 inputWidth);
	static bool DrawVectorProperty(void* instance, void* data, const TypeInfo& typeinfo, const PropertyInfo& property);

	static bool DrawColorProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	static bool DrawMatrixProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	//static bool DrawStructProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	//static bool DrawSetProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	static bool DrawEnumProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	static bool DrawBitFlagProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
#pragma endregion


#pragma region Mouse Wheel Value Change Helper
public:
	template <typename T>
	static bool ApplyMouseWheelInput(T* value, T resetValue = static_cast<T>(0), float step = 0.1f, float shiftMultiplier = 10.0f, bool hasRange = false, T minVal = static_cast<T>(0), T maxVal = static_cast<T>(0));
#pragma endregion



	//static bool DrawDetails(void* instance, const TypeInfo& typeInfo);


};
END