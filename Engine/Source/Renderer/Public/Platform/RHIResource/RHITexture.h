#pragma once

#include "RHIResource.h"

BEGIN(Engine)
class ENGINE_API RHITexture : public RHIResource
{
protected:
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