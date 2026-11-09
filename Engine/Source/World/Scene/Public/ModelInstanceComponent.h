#pragma once

#include "ComponentAnnotations.h"
#include "ResourceHandle.h"

namespace Engine
{
	class Model;
	class MaterialInterface;

	STRUCT(COMPONENT, HIDDEN)
		struct ModelInstanceComponent
	{
		REFLECT_BODY()

		PROPERTY()
		ResourceHandle<Model> modelHandle;

		PROPERTY()
		ResourceHandle<MaterialInterface> fallbackMaterial;
	};

	STRUCT(COMPONENT, HIDDEN)
		struct ModelSourceComponent
	{
		REFLECT_BODY()

		PROPERTY()
		wstring sourceNodeKey;

		PROPERTY()
		wstring meshAttachmentKey;

		PROPERTY()
		bool isMeshAttachment = false;

		PROPERTY(ALLOW_MISSING)
		bool useModelMaterial = false;

		PROPERTY(ALLOW_MISSING)
		bool useModelMesh = false;
	};
}