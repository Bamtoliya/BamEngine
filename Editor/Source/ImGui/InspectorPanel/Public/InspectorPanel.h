#pragma once

#include "ImGuiInterface.h"

BEGIN(Editor)
class InspectorPanel final : public ImGuiInterface
{
public:
	InspectorPanel() { m_Name = L"Inspector Panel"; }
	virtual ~InspectorPanel() = default;
public:
	virtual void Draw() override;
	virtual void Free() override { __super::Free(); }
private:
	bool DrawProperties(void* instance, const TypeInfo& typeInfo);
	string SanitizeVarName(const string& varName);
	string SanitizeDisplayLabel(const TypeInfo& typeInfo, const PropertyInfo& property);
public:
	EResult SetSelectedGameObject(class GameObject* gameObject) { m_SelectedGameObject = gameObject; return EResult::Success; }


private:
	bool DrawPropertyTable(void* instance, const TypeInfo& typeInfo, const vector<const Engine::PropertyInfo*>& props);

private:
	bool DrawIntegerProperty(void* data, const PropertyInfo& property);
	bool DrawFloatProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	bool DrawBooleanProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	bool DrawStringProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	bool DrawVectorProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	bool DrawMapProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	bool DrawListProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	bool DrawVector2Property(void* instance, void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	bool DrawVector3Property(void* instance, void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	bool DrawVector4Property(void* instance, void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	bool DrawQuaternionProperty(void* instance, void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	bool DrawColorProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	bool DrawMatrixProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property, uint32 dim = 4);
	bool DrawStructProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	bool DrawSetProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	bool DrawDetails(void* instance, const TypeInfo& typeInfo);
	bool DrawEnumProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);
	bool DrawBitFlagProperty(void* data, const TypeInfo& typeinfo, const PropertyInfo& property);

private:
	class GameObject* m_SelectedGameObject = nullptr;
};
END