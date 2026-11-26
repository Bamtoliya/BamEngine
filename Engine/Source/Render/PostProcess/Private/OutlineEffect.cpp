#include "OutlineEffect.h"
#include "DepthOfField.h"
#include "ResourceManager.h"
#include "PipelineManager.h"
#include "Renderer.h"
#include "RenderTargetManager.h"
#include "SamplerManager.h"
#include "RHI.h"
#include "Shader.h"
#include "PlatformPath.h"

namespace
{
	// outlinePS.hlsl의 OutlineParams (b0, space3)
	struct OutlineShaderParams
	{
		vec4   OutlineColor{ 0.0f, 0.0f, 0.0f, 1.0f };
		vec3   CameraPosition{ 0.0f };
		f32    Thickness = 1.0f;
		vec3   CameraForward{ 0.0f, 0.0f, 1.0f };
		f32    DepthThreshold = 0.1f;
		vec2   InvSourceSize{ 1.0f, 1.0f };
		f32    NormalThreshold = 0.4f;
		f32    Intensity = 1.0f;
		uint32 HasNormalMap = 0u;
		vec3   Padding{ 0.0f };
	};
	static_assert(sizeof(OutlineShaderParams) == 80);
	static_assert(offsetof(OutlineShaderParams, OutlineColor) == 0);
	static_assert(offsetof(OutlineShaderParams, CameraPosition) == 16);
	static_assert(offsetof(OutlineShaderParams, Thickness) == 28);
	static_assert(offsetof(OutlineShaderParams, CameraForward) == 32);
	static_assert(offsetof(OutlineShaderParams, DepthThreshold) == 44);
	static_assert(offsetof(OutlineShaderParams, InvSourceSize) == 48);
	static_assert(offsetof(OutlineShaderParams, NormalThreshold) == 56);
	static_assert(offsetof(OutlineShaderParams, Intensity) == 60);
	static_assert(offsetof(OutlineShaderParams, HasNormalMap) == 64);
	static_assert(offsetof(OutlineShaderParams, Padding) == 68);

	Shader* FindOrLoadOutlineShader(const wchar_t* key, const wchar_t* file)
	{
		auto& resources = ResourceManager::Get();
		auto handle = resources.GetResourceHandle<Shader>(key);
		if (!handle.Get())
		{
			ShaderDesc desc{};
			desc.Key = key;
			desc.dxilPath = (PlatformPath::GetEngineResourceRoot() / "Shader" / "bin" / "dxil" / file).wstring();
			desc.Path = desc.dxilPath;
			desc.shaderType = EShaderType::Fragment;
			handle = resources.LoadResource<Shader>(&desc);
		}
		Shader* shader = handle.Get();
		if (!shader || !shader->GetRHIShader())
		{
			ENGINE_LOG_ERROR("Shader is not available: {}", WStrToStr(file));
			return nullptr;
		}
		return shader;
	}
}

OutlineEffect::OutlineEffect()
	: IPostProcessEffect(L"Outline", "POST_PROCESS_OUTLINE")
{
	AddFloat("Thickness", 1.0f, 0.5f, 5.0f, "OUTLINE_THICKNESS");
	AddFloat("DepthThreshold", 0.1f, 0.001f, 2.0f, "OUTLINE_DEPTH_THRESHOLD");
	AddFloat("NormalThreshold", 0.4f, 0.01f, 2.0f, "OUTLINE_NORMAL_THRESHOLD");
	AddFloat4("OutlineColor", vec4(0.0f, 0.0f, 0.0f, 1.0f), "OUTLINE_COLOR", true);
	AddFloat("Intensity", 1.0f, 0.0f, 1.0f, "OUTLINE_INTENSITY");
	SyncSettings();
}

OutlineEffect::~OutlineEffect()
{
	Free();
}

void OutlineEffect::Initialize(const wstring& prefix)
{
	ResourceManager& rm = ResourceManager::Get();

	Shader* vs = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS").Get();
	Shader* ps = FindOrLoadOutlineShader(L"PostProcess_OutlinePS", L"outlinePS.cso");
	if (!vs || !vs->GetRHIShader() || !ps || !ps->GetRHIShader())
		return;

	RHIPipelineDesc pd = {};
	pd.pipelineType = EPipelineType::Graphics;
	pd.vertexShader = vs->GetRHIShader();
	pd.pixelShader = ps->GetRHIShader();
	pd.colorAttachmentCount = 1;
	pd.colorAttachmentFormats[0] = ETextureFormat::R8G8B8A8_UNORM;
	pd.depthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
	pd.depthStencilState.depthTestEnable = false;
	pd.depthStencilState.depthWriteEnable = false;
	pd.topology = ETopology::TriangleList;
	pd.cullMode = ECullMode::None;
	pd.blendState = BlendState(EBlendMode::None);

	m_Pipeline = PipelineManager::Get().GetOrCreatePipeline(pd);
	SetEnabled(false);
}

void OutlineEffect::Free()
{
	m_Pipeline = nullptr;
	m_HasSceneInputs = false;
}

void OutlineEffect::SetSceneInputs(const PostProcessSceneInputs* inputs)
{
	m_HasSceneInputs = inputs && inputs->IsValid();
	if (m_HasSceneInputs)
		m_SceneInputs = *inputs;
}

void OutlineEffect::AddFloat(const string& name, f32 defaultValue, f32 minValue, f32 maxValue, const string& localKey)
{
	m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float, &defaultValue, sizeof(f32), 0, minValue, maxValue, localKey);
}

void OutlineEffect::AddFloat4(const string& name, const vec4& defaultValue, const string& localKey, bool isColor)
{
	m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float4, &defaultValue, sizeof(vec4), 0, 0.0f, 1.0f, localKey, isColor);
}

void OutlineEffect::SyncSettings()
{
	const auto readFloat = [this](const char* name, f32& out)
		{
			const auto it = m_Parameters.find(name);
			if (it != m_Parameters.end() && it->second.data.size() >= sizeof(f32))
				memcpy(&out, it->second.data.data(), sizeof(f32));
		};

	readFloat("Thickness", m_Thickness);
	readFloat("DepthThreshold", m_DepthThreshold);
	readFloat("NormalThreshold", m_NormalThreshold);
	readFloat("Intensity", m_Intensity);

	if (const auto it = m_Parameters.find("OutlineColor"); it != m_Parameters.end() && it->second.data.size() >= sizeof(vec4))
		memcpy(&m_OutlineColor, it->second.data.data(), sizeof(vec4));
}

void OutlineEffect::SetFloat(const string& name, f32 value)
{
	auto it = m_Parameters.find(name);
	if (it == m_Parameters.end() || it->second.type != EMaterialParameterType::Float)
		return;
	value = std::clamp(value, it->second.minValue, it->second.maxValue);
	memcpy(it->second.data.data(), &value, sizeof(f32));
	SyncSettings();
}

void OutlineEffect::SetFloat4(const string& name, const vec4& value)
{
	auto it = m_Parameters.find(name);
	if (it == m_Parameters.end() || it->second.type != EMaterialParameterType::Float4)
		return;
	memcpy(it->second.data.data(), &value, sizeof(vec4));
	SyncSettings();
}

void OutlineEffect::ResetParameter(const string& name)
{
	auto it = m_Parameters.find(name);
	if (it == m_Parameters.end())
		return;
	it->second.data = it->second.defaultValue;
	SyncSettings();
}

void OutlineEffect::ResetAllParameters()
{
	for (auto& [name, param] : m_Parameters)
		param.data = param.defaultValue;
	SyncSettings();
}

EResult OutlineEffect::SubmitPass(f32 dt, RenderPassID passID, const wstring& sourceRT)
{
	if (!m_HasSceneInputs || !m_Pipeline)
		return EResult::Fail;

	auto* src = RenderTargetManager::Get().GetRenderTarget(sourceRT);
	if (!src || src->GetWidth() == 0 || src->GetHeight() == 0)
		return EResult::Fail;

	auto& rtMgr = RenderTargetManager::Get();
	auto* posRT = rtMgr.GetRenderTarget(m_SceneInputs.PositionRT);
	auto* diffRT = rtMgr.GetRenderTarget(m_SceneInputs.DiffuseRT);
	if (!posRT || !posRT->GetTexture() || !diffRT || !diffRT->GetTexture())
		return EResult::Fail;

	auto* normRT = !m_SceneInputs.NormalRT.empty() ? rtMgr.GetRenderTarget(m_SceneInputs.NormalRT) : nullptr;
	const bool hasNormalMap = (normRT && normRT->GetTexture() != nullptr);

	OutlineShaderParams params;
	params.OutlineColor = m_OutlineColor;
	params.CameraPosition = m_SceneInputs.Camera.cameraPosition;
	params.Thickness = m_Thickness;
	params.CameraForward = GetCameraForward(m_SceneInputs.Camera);
	params.DepthThreshold = m_DepthThreshold;
	params.InvSourceSize = vec2(1.0f / static_cast<f32>(src->GetWidth()), 1.0f / static_cast<f32>(src->GetHeight()));
	params.NormalThreshold = m_NormalThreshold;
	params.Intensity = m_Intensity;
	params.HasNormalMap = hasNormalMap ? 1u : 0u;

	const wstring normalName = hasNormalMap ? m_SceneInputs.NormalRT : m_SceneInputs.PositionRT;

	Renderer::Get().SubmitCustomCommand(
		[pipeline = m_Pipeline, sourceRT, posName = m_SceneInputs.PositionRT, diffName = m_SceneInputs.DiffuseRT, normalName, params]
		(f32, RenderPass*) -> EResult
		{
			auto& rt = RenderTargetManager::Get();
			auto* sceneTarget = rt.GetRenderTarget(sourceRT);
			auto* posTarget = rt.GetRenderTarget(posName);
			auto* diffTarget = rt.GetRenderTarget(diffName);
			auto* normTarget = rt.GetRenderTarget(normalName);
			if (!sceneTarget || !sceneTarget->GetTexture() || !posTarget || !posTarget->GetTexture() ||
				!diffTarget || !diffTarget->GetTexture() || !normTarget || !normTarget->GetTexture())
				return EResult::Fail;

			auto* rhi = Renderer::Get().GetRHI();
			SamplerDesc pointClampDesc{};
			pointClampDesc.MinFilter = ESamplerFilter::Point;
			pointClampDesc.MagFilter = ESamplerFilter::Point;
			pointClampDesc.MipFilter = ESamplerFilter::Point;
			pointClampDesc.AddressU = ESamplerAddressMode::Clamp;
			pointClampDesc.AddressV = ESamplerAddressMode::Clamp;
			pointClampDesc.AddressW = ESamplerAddressMode::Clamp;
			auto* sampler = SamplerManager::Get().GetOrCreateSampler(pointClampDesc);
			if (!sampler)
				return EResult::Fail;

			if (IsFailure(rhi->BindTextureSampler(sceneTarget->GetTexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + 0))) return EResult::Fail;
			if (IsFailure(rhi->BindTextureSampler(posTarget->GetTexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + 1))) return EResult::Fail;
			if (IsFailure(rhi->BindTextureSampler(diffTarget->GetTexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + 2))) return EResult::Fail;
			if (IsFailure(rhi->BindTextureSampler(normTarget->GetTexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + 3))) return EResult::Fail;

			if (IsFailure(rhi->BindConstantBuffer(&params, sizeof(params), 2)))
				return EResult::Fail;
			if (IsFailure(rhi->BindPipeline(pipeline)))
				return EResult::Fail;

			return rhi->Draw(3);
		}, passID);

	return EResult::Success;
}
