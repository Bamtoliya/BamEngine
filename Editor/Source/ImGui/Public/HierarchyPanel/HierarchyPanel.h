#pragma once

#include "Base.h"

BEGIN(Editor)

class HierarchyPanel
{
public:
	void Draw(class GameObject*& selectedObject);

private:
	void DrawGameObjectNode(class GameObject* gameObject, class GameObject*& selectedObject);
};
END