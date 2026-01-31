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
};
END