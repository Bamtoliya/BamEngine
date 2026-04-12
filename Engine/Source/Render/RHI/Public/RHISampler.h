#pragma once

#include "RHIResource.h"
#include "SamplerDesc.h"

BEGIN(Engine)
class ENGINE_API RHISampler : public RHIResource
{
protected:
	using DESC = tagSamplerDesc;
	RHISampler(RHI* rhi) : RHIResource(rhi, ERHIResourceType::Sampler) {}
	RHISampler(RHI* rhi, const DESC& desc) : RHIResource(rhi, ERHIResourceType::Sampler), m_Desc(desc) {}
	virtual ~RHISampler() = default;
public:
	virtual void* GetNativeHandle() const = 0;
	virtual const tagSamplerDesc& GetDesc() const { return m_Desc; }
protected:
	DESC m_Desc;
};
END