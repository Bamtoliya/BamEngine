#pragma once

#include "Engine_Includes.h"
#include "Reflection/ReflectionMacro.h"
#include "Resources.h"

enum ESpriteDrawMode
{
	Simple,
	Sliced,
	Tiled,
	Filled
};

STRUCT()
struct SpriteRendererComponent
{
	REFLECT_STRUCT()

	PROPERTY(EDITABLE)
	ResourceHandle<Sprite> spriteHandle;
	PROPERTY(EDITABLE)
	ResourceHandle<Mesh> meshHandle;
	PROPERTY(EDITABLE)
	ResourceHandle<MaterialInterface> materialHandle;

	PROPERTY(EDITABLE)
	ESpriteDrawMode drawMode = ESpriteDrawMode::Simple;

	PROPERTY(EDITABLE)
	vec4 color = vec4(1.f);
	PROPERTY(EDITABLE)
	vec2 tiling = vec2(1.f);
	PROPERTY(EDITABLE)
	vec2 offset = vec2(0.f);
	PROPERTY(EDITABLE)
	bool flipX = false;
	PROPERTY(EDITABLE)
	bool flipY = false;
};

struct StaticMeshRendererComponent
{
	ResourceHandle<Mesh> meshHandle;
	ResourceHandle<MaterialInterface> materialHandle;
	bool castShadow = true;
};

struct SkinnedMeshRendererComponent
{
	ResourceHandle<Mesh> meshHandle;
	ResourceHandle<MaterialInterface> materialHandle;
	ResourceHandle<Skeleton> skeletonHandle;
	bool castShadow = true;
};

struct LightComponent
{
	enum class ELightType
	{
		Directional,
		Point,
		Spot
	} LightType = ELightType::Directional;
	vec3 Color = vec3(1.f);
	f32 Intensity = 1.f;
	f32 Range = 10.f; // For Point and Spot lights
	f32 SpotAngle = 30.f; // For Spot lights
};

//struct AnimatorComponent
//{
//	ResourceHandle<class Animator> animator;
//};
//
//struct Animator2DComponent
//{
//	ResourceHandle<class Animator2D> animator2D;
//};

struct CameraComponent
{
	enum class ECameraType
	{
		Perspective,
		Orthographic
	} CameraType = ECameraType::Perspective;
	f32 FOV = 60.f;
	f32 AspectRatio = 16.f / 9.f;
	f32 NearPlane = 0.1f;
	f32 FarPlane = 1000.f;
	f32 OrthographicSize = 10.f;
	bool IsMainCamera = false;
};