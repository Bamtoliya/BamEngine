#pragma once
#include "Base.h"
BEGIN(Engine)

enum class ERHIResourceType
{
	Unknown,
	Buffer,
	Texture,
	Shader,
	PipelineState,
	SwapChain,
	RenderTargetView,
	DepthStencilView,
	ShaderResourceView,
	UnorderedAccessView,
	SamplerState,
	RasterizerState,
	BlendState,
	DepthStencilState,
};

class ENGINE_API RHIResource : public Base
{
protected:
	RHIResource() {}
	virtual ~RHIResource() = default;
public:
	virtual void* GetNativeHandle() const PURE;
protected:
	ERHIResourceType m_ResourceType = ERHIResourceType::Unknown;
};

class ENGINE_API RHIBuffer : public RHIResource
{
};
class ENGINE_API RHITexture : public RHIResource
{
};
class ENGINE_API RHIShader : public RHIResource
{
};
class ENGINE_API RHIPipeline : public RHIResource
{
};
END