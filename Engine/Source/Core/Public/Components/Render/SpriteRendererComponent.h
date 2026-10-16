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
