#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "ResourceHandle.h"

namespace Engine
{
	class Mesh;
	class MaterialInterface;
	class Skeleton;

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_SKINNED_MESH_RENDERER"), CATEGORY("CATEGORY_RENDER"))
	struct SkinnedMeshRendererComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MESH_HANDLE"))
		ResourceHandle<Mesh> meshHandle;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MATERIAL_HANDLE"))
		ResourceHandle<MaterialInterface> materialHandle;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SKELETON_HANDLE"))
		ResourceHandle<Skeleton> skeletonHandle;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CAST_SHADOW"))
		bool castShadow = true;
	};
}
