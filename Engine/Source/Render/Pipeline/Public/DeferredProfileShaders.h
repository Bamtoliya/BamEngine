#pragma once

#include "RenderPipelineProfile.h"
#include "ShaderReflection.h"

BEGIN(Engine)

struct DeferredProfileShader
{
	const wchar_t* Key;
	const wchar_t* DxilFile;
	EShaderType Type;
	bool DirectX12Only = false; // DXIL만 존재하므로 다른 백엔드에서는 등록하지 않는다.
};

[[nodiscard]] ENGINE_API std::vector<DeferredProfileShader> GetDeferredProfileShaders(const RenderPipelineProfile& profile);

END
