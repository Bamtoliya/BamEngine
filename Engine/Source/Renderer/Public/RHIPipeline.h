#pragma once

#include "RHIResource.h"

BEGIN(Engine)
class ENGINE_API RHIPipeline : public RHIResource
{
protected:
	RHIPipeline() : RHIResource(ERHIResourceType::PipelineState) {}
	virtual ~RHIPipeline() = default;
public:
};
END