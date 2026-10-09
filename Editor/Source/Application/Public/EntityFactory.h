#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class EntityFactory
{
public:
	static Entity& CreateEmptyEntity(class Scene* scene, const wstring& name = L"Entity");
	static Entity& CreatePrimitiveEntity(class Scene* scene, const wstring& name = L"Primitive", const wstring& meshKey = L"", const wstring& materialKey = L"");
	static Entity& CreateSpriteEntity(class Scene* scene, const wstring& name = L"Sprite", const wstring& materialKey = L"");
};
END