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

struct tagRHIShaderDesc
{
	EShaderType ShaderType = EShaderType::Unknown;
	string EntryPoint = { "main" };
	wstring FilePath = { L"" };
	vector<uint8> ShaderBytecode = {};
};

BEGIN(Engine)
class ENGINE_API RHIShader : public RHIResource
{
protected:
	RHIShader() : RHIResource(ERHIResourceType::Shader) {}
	RHIShader(RHI* rhi) : RHIResource(rhi, ERHIResourceType::Shader) {}
	RHIShader(RHI* rhi, EShaderType shaderType) : RHIResource(rhi, ERHIResourceType::Shader), m_ShaderType(shaderType) {}
	virtual ~RHIShader() = default;
public:
	EShaderType GetShaderType() const { return m_ShaderType; }
	const string& GetEntryPoint() const { return m_EntryPoint; }
protected:
	EShaderType m_ShaderType = { EShaderType::Unknown };
	string m_EntryPoint = { "" };
	vector<uint8> m_ShaderBytecode = {};
};
END