#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)

enum class UIElementPreset
{
	Panel, Row, Column, Grid, ProgressBar, Toggle, Slider, ClipPanel, EllipseMask, ScrollView, ImageMask,
	TextInput, Dropdown
};

class EntityFactory
{
public:
	static Entity& CreateCanvasEntity(class Scene* scene, const wstring& name = L"Canvas");
	static Entity& CreateUIImageEntity(class Scene* scene, Entity& parent, const wstring& name = L"Image");
	static Entity& CreateUIButtonEntity(class Scene* scene, Entity& parent, const wstring& name = L"Button");
	static Entity& CreateUITextEntity(class Scene* scene, Entity& parent, const wstring& name, const std::string& text);
	static Entity& CreateUITextInputEntity(class Scene* scene, Entity& parent, const wstring& name = L"TextInput", const std::string& placeholder = "Enter text...");
	static Entity& CreateUIDropdownEntity(class Scene* scene, Entity& parent, const wstring& name = L"Dropdown");
	static Entity& CreateUIPreset(class Scene* scene, Entity& parent, UIElementPreset preset, const wstring& name);
	static Entity& CreateEmptyEntity(class Scene* scene, const wstring& name = L"Entity");
	static Entity& CreatePrimitiveEntity(class Scene* scene, const wstring& name = L"Primitive", const wstring& meshKey = L"", const wstring& materialKey = L"");
	static Entity& CreateCubeEntity(class Scene* scene, const wstring& name = L"Cube");
	static Entity& CreateSphereEntity(class Scene* scene, const wstring& name = L"Sphere");
	static Entity& CreatePlaneEntity(class Scene* scene, const wstring& name = L"Plane");
	static Entity& CreateCylinderEntity(class Scene* scene, const wstring& name = L"Cylinder");
	static Entity& CreateCapsuleEntity(class Scene* scene, const wstring& name = L"Capsule");
	static Entity& CreateSpriteEntity(class Scene* scene, const wstring& name = L"Sprite", const wstring& materialKey = L"");
	static Entity& CreateLightEntity(class Scene* scene, const wstring& name = L"Light", const wstring& lightType = L"Point");
	static Entity& CreateDirectionalLightEntity(class Scene* scene, const wstring& name = L"Directional Light");
	static Entity& CreatePointLightEntity(class Scene* scene, const wstring& name = L"Point Light");
	static Entity& CreateSpotLightEntity(class Scene* scene, const wstring& name = L"Spot Light");
	static Entity& CreateSkyLightEntity(class Scene* scene, const wstring& name = L"Sky Light");
	static Entity& CreateCameraEntity(class Scene* scene, const wstring& name = L"Camera", bool isMainCamera = false);
	static Entity& CreateTerrainEntity(class Scene* scene, const wstring& name = L"Terrain");
};
END
