#pragma once

#include "RHIShader.h"

struct tagSDLShdaerCreateDesc
{
	EShaderType ShaderType = EShaderType::Unknown;
};

BEGIN(Engine)
class SDLShader final : public RHIShader
{
	using DESC = tagSDLShdaerCreateDesc;
#pragma region Constructor&Destructor
private:
	SDLShader() : RHIShader{} {}
	virtual ~SDLShader() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static SDLShader* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region ProcessVertex
public:
	SDL_Vertex* ProcessVertex(const class SDLBuffer* vertexBuffer, const glm::mat4& worldMatrix, glm::vec4& matColor, glm::vec2& screenInfo);
#pragma endregion


#pragma region Getter
public:
	//virtual void* GetNativeHandle() const override { return (void*)(uintptr_t)m_ShaderID; }
	virtual void* GetNativeHandle() const override { return (void*)(this); }
#pragma endregion


#pragma region Variable
private:
	uint32 m_ShaderID = { 0 };
	vector<SDL_Vertex> m_CacheVertices = {};
#pragma endregion
};
END