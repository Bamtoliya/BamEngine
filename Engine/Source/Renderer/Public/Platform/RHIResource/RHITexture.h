#pragma once

#include "RHIResource.h"

struct tagRHITextureDesc
{
	uint32		Width = 0;
	uint32		Height = 0;
	uint32		Channels = 0;
	void*		Data = nullptr;
};

BEGIN(Engine)
class ENGINE_API RHITexture : public RHIResource
{
protected:
	using DESC = tagRHITextureDesc;
	RHITexture() : RHIResource(ERHIResourceType::Texture) {}
	RHITexture(uint32 width, uint32 height) : RHIResource(ERHIResourceType::Texture), m_Width(width), m_Height(height) {}
	virtual ~RHITexture() = default;
public:
	uint32 GetWidth() const { return m_Width; }
	uint32 GetHeight() const { return m_Height; }
protected:
	uint32 m_Width = { 0 };
	uint32 m_Height = { 0 };
};
END