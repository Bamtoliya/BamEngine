#pragma once

#include <reflection/Annotations.h>
#include "ResourceHandle.h"

namespace Engine
{
	class Mesh;
	class MaterialInterface;

	STRUCT()
	struct StaticMeshRendererComponent
	{
		REFLECT_BODY()

		PROPERTY()
		ResourceHandle<Mesh> meshHandle;
		PROPERTY()
		ResourceHandle<MaterialInterface> materialHandle;
		PROPERTY()
		bool castShadow = true;
	};
}