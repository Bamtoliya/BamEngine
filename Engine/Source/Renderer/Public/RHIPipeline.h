#pragma once

#include "RHIResource.h"
#include "RHIShader.h"

typedef struct tagPipelineState
{
	RHIShader* VertexShader = { nullptr };
	RHIShader* PixelShader = { nullptr };
	RHIShader* ComputeShader = { nullptr };
	RHIShader* GeometryShader = { nullptr };
	RHIShader* HullShader = { nullptr };
	RHIShader* DomainShader = { nullptr };
} PIPELINESTATEDESC;

BEGIN(Engine)
class ENGINE_API RHIPipeline : public RHIResource
{
protected:
	RHIPipeline() : RHIResource(ERHIResourceType::PipelineState) {}
	virtual ~RHIPipeline() = default;
public:
};
END