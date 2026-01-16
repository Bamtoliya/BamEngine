#pragma once

#include "RHIResource.h"
BEGIN(Engine)
class ENGINE_API RHIShader : public RHIResource
{
protected:
	RHIShader() : RHIResource(ERHIResourceType::Shader) {}
	virtual ~RHIShader() = default;
public:

};
END