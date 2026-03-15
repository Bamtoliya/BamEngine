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

	bool operator==(const tagRHISamplerDesc& other) const
	{
		return MinFilter == other.MinFilter &&
			MagFilter == other.MagFilter &&
			MipFilter == other.MipFilter &&
			AddressU == other.AddressU &&
			AddressV == other.AddressV &&
			AddressW == other.AddressW &&
			MaxAnisotropy == other.MaxAnisotropy &&
			BorderColor == other.BorderColor;
	}
};



template<>
struct hash<tagRHISamplerDesc>
{
	size_t operator()(const tagRHISamplerDesc& desc) const
	{
		size_t seed = 0;

		HashCombine(seed, hash<int>()((int)desc.MinFilter));
		HashCombine(seed, hash<int>()((int)desc.MagFilter));
		HashCombine(seed, hash<int>()((int)desc.MipFilter));
		HashCombine(seed, hash<int>()((int)desc.AddressU));
		HashCombine(seed, hash<int>()((int)desc.AddressV));
		HashCombine(seed, hash<int>()((int)desc.AddressW));
		HashCombine(seed, hash<uint32>()(desc.MaxAnisotropy));


		HashCombine(seed, hash<float>()(desc.BorderColor.x));
		HashCombine(seed, hash<float>()(desc.BorderColor.y));
		HashCombine(seed, hash<float>()(desc.BorderColor.z));
		HashCombine(seed, hash<float>()(desc.BorderColor.w));

		return seed;
	}
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