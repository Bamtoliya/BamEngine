#pragma once

#include "Engine_Includes.h"
#include "Resource/ResourceHandle.h"

struct SpriteRendererComponent
{
	ResourceHandle<Texture> Texture;
	vec4 Color = vec4(1.f);
	vec2 Tiling = vec2(1.f);
	vec2 Offset = vec2(0.f);
	bool FlipX = false;
	bool FlipY = false;
	//vec2 Size = vec2(1.f);
	//vec2 Pivot = vec2(0.5f);
};

struct MeshRendererComponent
{
	ResourceHandle<Mesh> Mesh;
	ResourceHandle<MaterialInterface> Material;
	bool CastShadow = true;
};

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