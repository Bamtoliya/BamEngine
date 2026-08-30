#pragma once

#include "RHIShader.h"

BEGIN(Engine)
class DirectX12Shader final : public RHIShader
{
	using DESC = RHIShaderDesc;
#pragma region Constructor&Destructor
private:
	DirectX12Shader(RHI* rhi) : RHIShader{ rhi } {}
	virtual ~DirectX12Shader() = default;
	EResult Initialize(const DESC& desc);
public:
	static DirectX12Shader* Create(RHI* rhi, const DESC& desc);
	virtual void Free() override;
#pragma endregion

#pragma region ProcessVertex
public:
#pragma endregion


#pragma region Getter
public:
	D3D12_SHADER_BYTECODE GetBytecode() const;
	virtual void* GetNativeHandle() const override { return nullptr; }
	virtual void SetNativeHandle(void* nativeHandle) override { }
#pragma endregion
};
END