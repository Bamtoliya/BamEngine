#pragma once


#include "RHIResource.h"

enum class ESamplerFilter
{
	Point,
	Linear,
	Anisotropic
};

enum class ESamplerAddressMode
{
	Wrap,
	Mirror,
	Clamp,
	Border,
	MirrorOnce
};

struct tagRHISamplerDesc
{
	ESamplerFilter MinFilter = ESamplerFilter::Linear;
	ESamplerFilter MagFilter = ESamplerFilter::Linear;
	ESamplerFilter MipFilter = ESamplerFilter::Linear;
	ESamplerAddressMode AddressU = ESamplerAddressMode::Wrap;
	ESamplerAddressMode AddressV = ESamplerAddressMode::Wrap;
	ESamplerAddressMode AddressW = ESamplerAddressMode::Wrap;
	uint32 MaxAnisotropy = 1;
	vec4 BorderColor = vec4{ 0.0f, 0.0f, 0.0f, 0.0f };
};

BEGIN(Engine)
class ENGINE_API RHISampler : public RHIResource
{
protected:
	using DESC = tagRHISamplerDesc;
	RHISampler(RHI* rhi) : RHIResource(rhi, ERHIResourceType::Sampler) {}
	RHISampler(RHI* rhi, const DESC& desc) : RHIResource(rhi, ERHIResourceType::Sampler), m_Desc(desc) {}
	virtual ~RHISampler() = default;
public:
	virtual void* GetNativeHandle() const = 0;
	virtual const tagRHISamplerDesc& GetDesc() const { return m_Desc; }
protected:
	DESC m_Desc;
};
END