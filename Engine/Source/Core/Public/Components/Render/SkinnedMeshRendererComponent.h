#pragma once

#include <reflection/Annotations.h>
#include "ResourceHandle.h"

namespace Engine
{
	class Mesh;
	class MaterialInterface;
	class Skeleton;

	STRUCT()
	struct SkinnedMeshRendererComponent
	{
		REFLECT_BODY()

		PROPERTY()
		ResourceHandle<Mesh> meshHandle;
		PROPERTY()
		ResourceHandle<MaterialInterface> materialHandle;
		PROPERTY()
		ResourceHandle<Skeleton> skeletonHandle;
		PROPERTY()
		bool castShadow = true;
	};
}