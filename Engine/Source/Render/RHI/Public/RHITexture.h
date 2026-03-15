#pragma once

#include "RHIResource.h"

struct tagRHITextureDesc
{
	uint32	Width = 1;
	uint32	Height = 1;
	uint32	Depth = 1;
	uint32 	MipLevels = 1;
	uint32 	ArraySize = 1;

	Engine::ETextureSampleCount SampleCount = Engine::ETextureSampleCount::TextureSampleCount1;
	Engine::ETextureFormat Format = Engine::ETextureFormat::R8G8B8A8_UNORM;
	Engine::ETextureDimension Dimension = Engine::ETextureDimension::Texture2D;
	Engine::ETextureUsage Usage = Engine::ETextureUsage::Sampler;

	void*		Data = nullptr;
	uint32		DataSize = 0;
};

BEGIN(Engine)
class ENGINE_API RHITexture : public RHIResource
{
protected:
	using DESC = tagRHITextureDesc;
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
};
END