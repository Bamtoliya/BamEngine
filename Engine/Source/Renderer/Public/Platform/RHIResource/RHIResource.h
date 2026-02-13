#pragma once

#include "Base.h"
#include "RHI.h"
#include "RHIDefinitions.h"

enum class ERHIResourceType
{
	Unknown,
	Buffer,
	Texture,
	Shader,
	Pipeline,
	SwapChain,
	RenderTargetView,
	DepthStencilView,
	ShaderResourceView,
	UnorderedAccessView,
	Sampler,
	RasterizerState,
	BlendState,
	DepthStencilState,
};


BEGIN(Engine)
class ENGINE_API RHIResource : public Base
{
protected:
	RHIResource(RHI* rhi) : m_RHI{ rhi } {}
	RHIResource(ERHIResourceType type) : m_ResourceType{ type } {}
	RHIResource(RHI* rhi, ERHIResourceType type) : m_RHI{ rhi }, m_ResourceType{ type } { Safe_AddRef(m_RHI); }
	virtual ~RHIResource() = default;
public:
	virtual void Free() override { Safe_Release(m_RHI); }
public:
	virtual EResult Bind(uint32 slot = 0) { return EResult::NotImplemented; }
public:
	virtual void* GetNativeHandle() const PURE;
	virtual void SetNativeHandle(void* nativeHandle) PURE;
protected:
	RHI* m_RHI = nullptr;
	ERHIResourceType m_ResourceType = ERHIResourceType::Unknown;
};
END