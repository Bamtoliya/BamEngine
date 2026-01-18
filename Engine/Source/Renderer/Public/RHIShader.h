#pragma once

#include "RHIResource.h"

enum class EShaderType
{
	Vertex,
	Pixel,
	Compute,
	Geometry,
	Hull,
	Domain,
	Unknown,
};

BEGIN(Engine)
class ENGINE_API RHIShader : public RHIResource
{
protected:
	RHIShader() : RHIResource(ERHIResourceType::Shader) {}
	virtual ~RHIShader() = default;
public:
	EShaderType GetShaderType() const { return m_ShaderType; }
protected:
	EShaderType m_ShaderType = { EShaderType::Unknown };
};
END