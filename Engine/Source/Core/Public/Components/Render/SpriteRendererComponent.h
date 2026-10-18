#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "ResourceHandle.h"

namespace Engine
{
	class Sprite;
	class Mesh;
	class MaterialInterface;

	ENUM()
	enum ESpriteDrawMode
	{
		Simple,
		Sliced,
		Tiled,
		Filled
	};

	ENUM()
	enum class ESpriteBillboardMode : uint8
	{
		None,           // 일반 3D 회전 (바닥, 벽 타일)
		CameraFacing,   // 완전 구면 빌보드 (파티클, 폭발 이펙트)
		AxialY          // Y축 고정 원통형 빌보드 (2.5D 캐릭터, 나무)
	};

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_SPRITE_RENDERER"), CATEGORY("CATEGORY_RENDER"))
	struct SpriteRendererComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPRITE_HANDLE"))
		ResourceHandle<Sprite> spriteHandle;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MESH_HANDLE"))
		ResourceHandle<Mesh> meshHandle;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MATERIAL_HANDLE"))
		ResourceHandle<MaterialInterface> materialHandle;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_DRAW_MODE"))
		ESpriteDrawMode drawMode = ESpriteDrawMode::Simple;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_BILLBOARD_MODE"))
		ESpriteBillboardMode billboardMode = ESpriteBillboardMode::None;

		PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_COLOR"))
		vec4 color = vec4(1.f);
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TILING"))
		vec2 tiling = vec2(1.f);
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_OFFSET"))
		vec2 offset = vec2(0.f);
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FLIPX"))
		bool flipX = false;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FLIPY"))
		bool flipY = false;
	};
}
