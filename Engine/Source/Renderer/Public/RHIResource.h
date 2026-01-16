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
	RHIResource(ERHIResourceType type) : m_ResourceType{ type } {}
	virtual ~RHIResource() = default;
public:
	virtual void* GetNativeHandle() const PURE;
protected:
	ERHIResourceType m_ResourceType = ERHIResourceType::Unknown;
};
END