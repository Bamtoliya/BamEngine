#pragma once

#include "Base.h"

BEGIN(Editor)
class InspectorPanel final
{
public:
	void Draw(class GameObject* selectedObject);
private:
	void DrawProperties(void* instance, const TypeInfo& typeInfo);
	string SanitizeVarName(const string& varName);
	string SanitizeDisplayLabel(const TypeInfo& typeInfo, const PropertyInfo& property);


private:
	void DrawPropertyTable(void* instance, const TypeInfo& typeInfo, const vector<const Engine::PropertyInfo*>& props);

private:
	void DrawIntegerProperty(void* data, const PropertyInfo& property);
	void DrawFloatProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	void DrawBooleanProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	void DrawStringProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	void DrawVectorProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	void DrawMapProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	void DrawListProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	void DrawVector3Property(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	void DrawVector4Property(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	void DrawQuaternionProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	void DrawColorProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	void DrawMatrixProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property, uint32 dim = 4);
	void DrawStructProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	void DrawSetProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	void DrawEnumProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	void DrawBitFlagProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);

};
END