#pragma once

#include <reflection/Annotations.h>
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

	STRUCT()
	struct SpriteRendererComponent
	{
		REFLECT_BODY()

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
}