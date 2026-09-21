#pragma once

#include "RHIResource.h"

struct RHITextureDesc
{
	uint32	width = 1, height = 1, depth = 1;
	uint32 	mipLevels = 1, arraySize = 1;

	Engine::ETextureSampleCount sampleCount = Engine::ETextureSampleCount::TextureSampleCount1;
	Engine::ETextureFormat format = Engine::ETextureFormat::R8G8B8A8_UNORM;
	Engine::ETextureDimension dimension = Engine::ETextureDimension::Texture2D;
	Engine::ETextureUsage usage = Engine::ETextureUsage::Sampler;

	void*		data = nullptr;
	uint32		dataSize = 0;

	bool sRGB = false;

	vec4 clearColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	f32 clearDepth = 1.0f;
	uint8 clearStencil = 0;
};

BEGIN(Engine)
class ENGINE_API RHITexture : public RHIResource
{
protected:
	using DESC = RHITextureDesc;
	RHITexture() : RHIResource(ERHIResourceType::Texture) {}
	RHITexture(RHI* rhi) : RHIResource(rhi, ERHIResourceType::Texture) {}
	RHITexture(uint32 width, uint32 height) : RHIResource(ERHIResourceType::Texture), m_Width(width), m_Height(height) {}
	RHITexture(RHI* rhi, uint32 width, uint32 height) : RHIResource(rhi, ERHIResourceType::Texture), m_Width(width), m_Height(height) {}
	virtual ~RHITexture() = default;
public:
	uint32 GetWidth() const { return m_Width; }
	uint32 GetHeight() const { return m_Height; }
	uint32 GetDepth() const { return m_Depth; }
	uint32 GetMipLevels() const { return m_MipLevels; }
	uint32 GetArraySize() const { return m_ArraySize; }
	
	ETextureSampleCount GetSampleCount() const { return m_SampleCount; }
	ETextureFormat GetFormat() const { return m_Format; }
	ETextureDimension GetDimension() const { return m_Dimension; }
	ETextureUsage GetUsage() const { return m_Usage; }

	vec4 GetClearColor() const { return m_ClearColor; }
	f32 GetClearDepth() const { return m_ClearDepth; }
	uint8 GetClearStencil() const { return m_ClearStencil; }

	DESC GetDesc() const
	{
		DESC desc = {};
		desc.width = m_Width;
		desc.height = m_Height;
		desc.depth = m_Depth;
		desc.mipLevels = m_MipLevels;
		desc.arraySize = m_ArraySize;
		desc.sampleCount = m_SampleCount;
		desc.format = m_Format;
		desc.dimension = m_Dimension;
		desc.usage = m_Usage;
		desc.sRGB = m_sRGB;
		desc.clearColor = m_ClearColor;
		desc.clearDepth = m_ClearDepth;
		desc.clearStencil = m_ClearStencil;
		return desc;
	}
protected:
	uint32 m_Width			= { 0 };
	uint32 m_Height			= { 0 };
	uint32 m_Depth			= { 0 };
	uint32 m_MipLevels		= { 1 };
	uint32 m_ArraySize		= { 1 };
	
	ETextureSampleCount m_SampleCount = ETextureSampleCount::TextureSampleCount1;
	ETextureFormat		m_Format	= ETextureFormat::UNKNOWN;
	ETextureDimension	m_Dimension = ETextureDimension::Texture2D;
	ETextureUsage		m_Usage		= ETextureUsage::Sampler;

	bool m_sRGB = false;

	vec4	m_ClearColor	= { 0.0f, 0.0f, 0.0f, 1.0f };
	f32		m_ClearDepth	= { 1.0f };
	uint8	m_ClearStencil	= { 0 };
};
END