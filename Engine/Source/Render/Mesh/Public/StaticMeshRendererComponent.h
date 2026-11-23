#pragma once

#include "ComponentAnnotations.h"
#include "ResourceHandle.h"

namespace Engine
{
	class Mesh;
	class MaterialInterface;

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_STATIC_MESH_RENDERER"), CATEGORY("CATEGORY_RENDER"))
	struct StaticMeshRendererComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MESH_HANDLE"))
		ResourceHandle<Mesh> meshHandle;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MATERIAL_HANDLE"))
		ResourceHandle<MaterialInterface> materialHandle;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CAST_SHADOW"))
		bool castShadow = true;
		// 끄면 DecalComponent와 VFX 데칼이 이 메시에 투영되지 않는다.
		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_RECEIVE_DECALS"))
		bool receiveDecals = true;
	};
}
