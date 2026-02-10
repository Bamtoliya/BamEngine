#pragma once

#include "RHIShader.h"

BEGIN(Engine)
class SDLGPUShader final : public RHIShader
{
	using DESC = tagRHIShaderDesc;
#pragma region Constructor&Destructor
private:
	SDLGPUShader(RHI* rhi) : RHIShader{rhi} {}
	virtual ~SDLGPUShader() = default;
	EResult Initialize(const DESC& desc);
public:
	static SDLGPUShader* Create(RHI* rhi, const DESC& desc);
	virtual void Free() override;
#pragma endregion

#pragma region ProcessVertex
public:
#pragma endregion


#pragma region Getter
public:
	//virtual void* GetNativeHandle() const override { return (void*)(uintptr_t)m_ShaderID; }
	virtual void* GetNativeHandle() const override { return static_cast<void*>(m_Shader); }
	virtual void SetNativeHandle(void* nativeHandle) override { m_Shader = static_cast<SDL_GPUShader*>(nativeHandle); }
#pragma endregion


#pragma region Variable
private:
	SDL_GPUShader* m_Shader = { nullptr };
#pragma endregion
};
END