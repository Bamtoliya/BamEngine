#pragma once

#include "RenderPipelineProfile.h"
#include "ShaderReflection.h"

BEGIN(Engine)

struct DeferredProfileShader
{
	const wchar_t* Key;
	const wchar_t* DxilFile;
	EShaderType Type;
};

[[nodiscard]] ENGINE_API std::vector<DeferredProfileShader> GetDeferredProfileShaders(const RenderPipelineProfile& profile);

END
