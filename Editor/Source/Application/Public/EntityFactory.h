#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class EntityFactory
{
public:
	static Entity& CreateEmptyEntity(class Scene* scene, const wstring& name = L"Entity");
};
END