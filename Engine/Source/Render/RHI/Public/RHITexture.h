#pragma once

#include "RHIResource.h"

struct tagRHITextureDesc
{
	uint32	Width = 0;
	uint32	Height = 0;
	uint32	Depth = 0;
	uint32 	MipLevels = 1;
	uint32 	ArraySize = 1;
	uint32	SampleCount = 1;

	Engine::ETextureFormat Format = Engine::ETextureFormat::UNKNOWN;
	Engine::ETetxtureDimension Dimension = Engine::ETetxtureDimension::Texture2D;

	void*		Data = nullptr;
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
protected:
	uint32 m_Width = { 0 };
	uint32 m_Height = { 0 };
	uint32 m_Depth = { 0 };
	uint32 m_MipLevels = { 1 };
	uint32 m_ArraySize = { 1 };
	Engine::ETextureFormat m_Format = Engine::ETextureFormat::UNKNOWN;
	Engine::ETetxtureDimension m_Dimension = Engine::ETetxtureDimension::Texture2D;
};
END