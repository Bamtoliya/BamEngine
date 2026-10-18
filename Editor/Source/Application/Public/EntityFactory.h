#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class EntityFactory
{
public:
	static Entity& CreateCanvasEntity(class Scene* scene, const wstring& name = L"Canvas");
	static Entity& CreateUIImageEntity(class Scene* scene, Entity& parent, const wstring& name = L"Image");
	static Entity& CreateUIButtonEntity(class Scene* scene, Entity& parent, const wstring& name = L"Button");
	static Entity& CreateEmptyEntity(class Scene* scene, const wstring& name = L"Entity");
	static Entity& CreatePrimitiveEntity(class Scene* scene, const wstring& name = L"Primitive", const wstring& meshKey = L"", const wstring& materialKey = L"");
	static Entity& CreateSpriteEntity(class Scene* scene, const wstring& name = L"Sprite", const wstring& materialKey = L"");
	static Entity& CreateLightEntity(class Scene* scene, const wstring& name = L"Light", const wstring& lightType = L"Point");
};
END
