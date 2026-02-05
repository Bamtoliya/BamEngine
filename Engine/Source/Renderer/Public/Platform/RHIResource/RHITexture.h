#pragma once

#include "RHIResource.h"

struct tagRHITextureDesc
{
	uint32		Width = 0;
	uint32		Height = 0;
};

BEGIN(Engine)
class ENGINE_API RHITexture : public RHIResource
{
protected:
	using DESC = tagRHITextureDesc;
	RHITexture() : RHIResource(ERHIResourceType::Texture) {}
	virtual ~RHITexture() = default;
public:
	uint32 GetWidth() const { return m_Width; }
	uint32 GetHeight() const { return m_Height; }
protected:
	uint32 m_Width = { 0 };
	uint32 m_Height = { 0 };
};
END