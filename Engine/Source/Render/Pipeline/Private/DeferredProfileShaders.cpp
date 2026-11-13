#include "DeferredProfileShaders.h"
#include <algorithm>

std::vector<Engine::DeferredProfileShader> Engine::GetDeferredProfileShaders(const RenderPipelineProfile& profile)
{
	using Stage = EShaderType;
	std::vector<DeferredProfileShader> shaders = {
		{ L"FullscreenQuadVS", L"fullscreenQuadVS.cso", Stage::Vertex },
		{ L"LightingPS", L"lightingPS.cso", Stage::Fragment },
		{ L"Resources/Shader/tiledLightCullingCS", L"tiledLightCullingCS.cso", Stage::Compute },
		{ L"Resources/Shader/GBufferVS", L"gbufferVS.cso", Stage::Vertex },
		{ L"Resources/Shader/GBufferPS", L"gbufferPS.cso", Stage::Fragment },
		{ L"Resources/Shader/GBufferSkinningVS", L"gbufferSkinningVS.cso", Stage::Vertex },
		{ L"Resources/Shader/ShadowDepthVS", L"shadowDepthVS.cso", Stage::Vertex },
		{ L"Resources/Shader/ShadowDepthPS", L"shadowDepthPS.cso", Stage::Fragment },
		{ L"Resources/Shader/ShadowDepthSkinningVS", L"shadowDepthSkinningVS.cso", Stage::Vertex },
		{ L"Resources/Shader/UIVS", L"uiVS.cso", Stage::Vertex },
		{ L"Resources/Shader/UIPS", L"uiPS.cso", Stage::Fragment },
		{ L"ClientPresentPS", L"clientPresentPS.cso", Stage::Fragment }
	};
	if (std::ranges::any_of(profile.Passes, [](const auto& pass) { return pass.Type == "ScenePostProcess"; }))
	{
		shaders.insert(shaders.end(), {
			{ L"PostProcess_ToneMappingPS", L"postprocess_tonemappingPS.cso", Stage::Fragment },
			{ L"PostProcess_ColorAdjustmentsPS", L"colorAdjustmentsPS.cso", Stage::Fragment },
			{ L"PostProcess_VignettePS", L"vignettePS.cso", Stage::Fragment },
			{ L"PostProcess_ChromaticAberrationPS", L"chromaticAberrationPS.cso", Stage::Fragment },
			{ L"PostProcess_FilmGrainPS", L"filmGrainPS.cso", Stage::Fragment },
			{ L"PostProcess_LensDistortionPS", L"lensDistortionPS.cso", Stage::Fragment },
			{ L"PostProcess_SharpenPS", L"sharpenPS.cso", Stage::Fragment },
			{ L"PostProcess_BloomPrefilterPS", L"bloomPrefilterPS.cso", Stage::Fragment },
			{ L"PostProcess_BloomDownsamplePS", L"bloomDownsamplePS.cso", Stage::Fragment },
			{ L"PostProcess_BloomUpsamplePS", L"bloomUpsamplePS.cso", Stage::Fragment },
			{ L"PostProcess_BloomCompositePS", L"bloomCompositePS.cso", Stage::Fragment }
		});
	}
	return shaders;
}
