#pragma once

#include "RHIResource.h"
#include "ShaderReflection.h"

struct RHIShaderDesc
{
	EShaderType shaderType = EShaderType::Unknown;
	string entryPoint = { "main" };

	wstring filePath = { L"" };
	vector<uint8> shaderBytecode = {};

	uint32 numSamplers;
	uint32 numStorageTextures;
	uint32 numStorageBuffers;
	uint32 numUniformBuffers;
};

BEGIN(Engine)
class ENGINE_API RHIShader : public RHIResource
{
protected:
	RHIShader() : RHIResource(ERHIResourceType::Shader) {}
	RHIShader(RHI* rhi) : RHIResource(rhi, ERHIResourceType::Shader) {}
	RHIShader(RHI* rhi, EShaderType shaderType) : RHIResource(rhi, ERHIResourceType::Shader), m_ShaderType(shaderType) {}
public:
	virtual ~RHIShader() = default;
	EShaderType GetShaderType() const { return m_ShaderType; }
	const string& GetEntryPoint() const { return m_EntryPoint; }
	const vector<uint8>& GetRawShaderBytecode() const { return m_ShaderBytecode; }
protected:
	PROPERTY()
	EShaderType m_ShaderType = { EShaderType::Unknown };
	PROPERTY()
	string m_EntryPoint = { "" };

	vector<uint8> m_ShaderBytecode = {};
};
END