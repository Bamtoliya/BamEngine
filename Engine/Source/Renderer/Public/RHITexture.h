#pragma once

#include "RHIResource.h"

BEGIN(Engine)
class ENGINE_API RHITexture : public RHIResource
{
protected:
	RHITexture() : RHIResource(ERHIResourceType::Texture) {}
	virtual ~RHITexture() = default;
public:

};
END