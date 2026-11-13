#include "ViewportDeferredPipeline.h"
#include "RenderTargetManager.h"
#include "RenderPassManager.h"
#include <algorithm>

ViewportDeferredPipeline::~ViewportDeferredPipeline()
{
	Free();
}

bool ViewportDeferredPipeline::Initialize(const std::wstring& viewportPrefix, uint32 width, uint32 height,
	bool acceptSceneMeshes)
{
	if (width == 0 || height == 0 || !m_OwnedRTNames.empty())
		return false;

	m_Prefix = viewportPrefix.empty() ? L"VP_" : viewportPrefix + L"_";
	m_Width = width;
	m_Height = height;
	m_AcceptSceneMeshes = acceptSceneMeshes;

	CreateRenderTargets(width, height);
	bool ready = m_SceneRenderer.Initialize();
	auto& targets = Engine::RenderTargetManager::Get();

	for (const auto& name : m_OwnedRTNames)
	{
		auto* target = targets.GetRenderTarget(name);
		if (target == nullptr || target->GetTexture() == nullptr ||
			target->GetTexture()->GetNativeHandle() == nullptr)
			ready = false;
	}

	if (!ready)
	{
		Free();
		return false;
	}

	RegisterRenderPasses();
	return true;
}

void ViewportDeferredPipeline::Free()
{
	if (m_ProfilePasses.GetTargets().empty())
	{
		if (Engine::RenderPassManager::IsCreated())
		{
			auto& passes = Engine::RenderPassManager::Get();
			for (auto id : { m_ForwardTransparentPassID, m_ForwardPassID, m_LightingPassID,
				m_GeometryPassID, m_PointShadowPassID, m_ShadowPassID })
				passes.RemoveRenderPass(id);
		}
		if (Engine::RenderTargetManager::IsCreated())
		{
			auto& targets = Engine::RenderTargetManager::Get();
			for (const auto& name : m_OwnedRTNames)
				targets.RemoveRenderTarget(name);
		}
	}
	m_ProfilePasses.Free();
	m_OwnedRTNames.clear();
	m_LightingInputs = {};
	m_ShadowDepthName.clear();
	m_PointShadowDepthName.clear();
	m_DepthName.clear();
	m_LightingColorName.clear();
	m_Width = m_Height = 0;

	m_SceneRenderer.Free();

	m_ShadowPassID = Engine::INVALID_PASS_ID;
	m_PointShadowPassID = Engine::INVALID_PASS_ID;
	m_GeometryPassID = Engine::INVALID_PASS_ID;
	m_LightingPassID = Engine::INVALID_PASS_ID;
	m_ForwardPassID = Engine::INVALID_PASS_ID;
	m_ForwardTransparentPassID = Engine::INVALID_PASS_ID;
}

Engine::RenderPipelineProfileResult ViewportDeferredPipeline::Initialize(const Engine::RenderPipelineProfile& profile,
	const std::wstring& viewportPrefix, uint32 width, uint32 height)
{
	if (!m_OwnedRTNames.empty())
		return { {}, {}, "The viewport pipeline is already initialized." };

	auto result = m_ProfilePasses.Initialize(profile, viewportPrefix, width, height);
	if (!result)
		return result;

	if (!m_SceneRenderer.Initialize())
	{
		Free();
		return { {}, "DeferredLighting", "Could not create the lighting pipeline. Check FullscreenQuadVS and LightingPS." };
	}

	m_Width = width;
	m_Height = height;
	m_AcceptSceneMeshes = true;
	m_OwnedRTNames = m_ProfilePasses.GetTargets();
	m_LightingInputs = m_ProfilePasses.GetLightingInputs();
	m_ShadowDepthName = m_LightingInputs.back();
	m_PointShadowDepthName = m_ProfilePasses.GetPointShadowInput();
	m_LightingColorName = m_ProfilePasses.GetColorTarget();
	m_ShadowPassID = m_ProfilePasses.GetPass("DirectionalShadow");
	m_PointShadowPassID = m_ProfilePasses.GetPass("PointShadow");
	m_GeometryPassID = m_ProfilePasses.GetPass("GBuffer");
	m_LightingPassID = m_ProfilePasses.GetPass("DeferredLighting");
	m_ForwardPassID = m_ProfilePasses.GetPass("ForwardOpaque");
	m_ForwardTransparentPassID = m_ProfilePasses.GetPass("ForwardTransparent");
	m_DepthName = Engine::RenderPassManager::Get().GetRenderPassByID(m_GeometryPassID)->GetDepthStencilName();
	return {};
}

void ViewportDeferredPipeline::CreateRenderTargets(uint32 width, uint32 height)
{
	auto& rtm = Engine::RenderTargetManager::Get();

	Engine::RenderTargetDesc gbufferDescs[] = {
		// 0~4: GBuffer (Diffuse, Normal, PBR, Emission, Position)
		{ { width, height, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::R8G8B8A8_UNORM, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::RenderTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) }, m_Prefix + L"GBuffer_Diffuse" },
		{ { width, height, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::R16G16B16A16_FLOAT, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::RenderTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) }, m_Prefix + L"GBuffer_Normal" },
		{ { width, height, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::R8G8B8A8_UNORM, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::RenderTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) }, m_Prefix + L"GBuffer_PBR" },
		{ { width, height, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::R8G8B8A8_UNORM, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::RenderTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) }, m_Prefix + L"GBuffer_Emission" },
		{ { width, height, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::R32G32B32A32_FLOAT, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::RenderTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) }, m_Prefix + L"GBuffer_Position" },
		// 5: ShadowDepth (4096 Atlas)
		{ { 4096, 4096, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::D32_FLOAT, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::DepthStencilTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) }, m_Prefix + L"ShadowDepth" },
		// 6: Depth
		{ { width, height, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::D24_UNORM_S8_UINT, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::DepthStencilTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) }, m_Prefix + L"Depth" },
		// 7: LightingColor (HDR 선형 부동소수점 버퍼)
		{ { width, height, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::R16G16B16A16_FLOAT, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::RenderTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f, 0.f, 0.f, 1.f) }, m_Prefix + L"LightingColor" }
	};

	m_OwnedRTNames.clear();
	for (auto& desc : gbufferDescs)
	{
		rtm.CreateRenderTarget(&desc);
		m_OwnedRTNames.push_back(desc.name);
	}

	m_ShadowDepthName = m_OwnedRTNames[5];
	std::copy_n(m_OwnedRTNames.begin(), m_LightingInputs.size(), m_LightingInputs.begin());
	m_DepthName = m_OwnedRTNames[6];
	m_LightingColorName = m_OwnedRTNames[7];

	// 8: PointShadowDepth (Point Light 큐브맵 섀도우)
	m_PointShadowDepthName = m_Prefix + L"PointShadowDepth";
	Engine::RenderTargetDesc pointShadowDesc = {
		{ 1024, 1024, 1, 1, 6, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::D32_FLOAT, Engine::ETextureDimension::TextureCube, Engine::ETextureUsage::DepthStencilTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) },
		m_PointShadowDepthName
	};
	rtm.CreateRenderTarget(&pointShadowDesc);
	m_OwnedRTNames.push_back(m_PointShadowDepthName);
}

void ViewportDeferredPipeline::RegisterRenderPasses()
{
	auto& rpm = Engine::RenderPassManager::Get();
	std::vector<std::wstring> gBufferNames = { m_OwnedRTNames.begin(), m_OwnedRTNames.begin() + 5 };

	// 1. ShadowPass (Directional Light 그림자)
	m_ShadowPassID = rpm.RegisterRenderPass(
		m_Prefix + L"ShadowPass", {}, m_ShadowDepthName,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 0, Engine::ERenderSortType::FrontToBack,
		Engine::ERenderPassType::Shadow, Engine::EBlendMode::Opaque | Engine::EBlendMode::Masked);

	// 2. PointShadowPass (Point Light 큐브맵 그림자)
	m_PointShadowPassID = rpm.RegisterRenderPass(
		m_Prefix + L"PointShadowPass", {}, m_PointShadowDepthName,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 0, Engine::ERenderSortType::FrontToBack,
		Engine::ERenderPassType::Shadow, Engine::EBlendMode::Opaque | Engine::EBlendMode::Masked);

	// 3. GeometryPass (프리뷰 파이프라인의 경우 메인 씬 메쉬 유입을 차단하기 위해 acceptedBlendModes를 None으로 설정)
	const Engine::EBlendMode acceptedModes = m_AcceptSceneMeshes ? (Engine::EBlendMode::Opaque | Engine::EBlendMode::Masked) : Engine::EBlendMode::None;
	m_GeometryPassID = rpm.RegisterRenderPass(
		m_Prefix + L"GeometryPass", gBufferNames, m_DepthName,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 0, Engine::ERenderSortType::FrontToBack,
		Engine::ERenderPassType::Geometry, acceptedModes);

	// 4. LightingPass (풀스크린 조명 합성)
	m_LightingPassID = rpm.RegisterRenderPass(
		m_Prefix + L"LightingPass", { m_LightingColorName }, L"",
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 100, Engine::ERenderSortType::None,
		Engine::ERenderPassType::Lighting, Engine::EBlendMode::None);

	// 5. ForwardPass (포워드 불투명 메쉬 렌더링)
	m_ForwardPassID = rpm.RegisterRenderPass(
		m_Prefix + L"ForwardPass", { m_LightingColorName }, m_DepthName,
		Engine::ERenderPassLoadOperation::RPLO_Load, Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Load, Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 110, Engine::ERenderSortType::FrontToBack,
		Engine::ERenderPassType::Forward, Engine::EBlendMode::Forward);

	// 6. ForwardTransparentPass (반투명 메쉬 렌더링)
	m_ForwardTransparentPassID = rpm.RegisterRenderPass(
		m_Prefix + L"ForwardTransparentPass", { m_LightingColorName }, m_DepthName,
		Engine::ERenderPassLoadOperation::RPLO_Load, Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Load, Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 120, Engine::ERenderSortType::BackToFront,
		Engine::ERenderPassType::ForwardTransparent,
		Engine::EBlendMode::AlphaBlend | Engine::EBlendMode::Additive | Engine::EBlendMode::NonPremultiplied | Engine::EBlendMode::Decal);
}

EResult ViewportDeferredPipeline::Resize(uint32 width, uint32 height)
{
	if (width == 0 || height == 0 || m_OwnedRTNames.empty())
		return EResult::InvalidArgument;

	auto& rtMgr = Engine::RenderTargetManager::Get();
	bool succeeded = true;

	if (!m_ProfilePasses.GetTargets().empty())
	{
		succeeded = m_ProfilePasses.Resize(width, height) == EResult::Success;
	}
	else
	{
		for (const auto& name : m_OwnedRTNames)
		{
			if (name == m_ShadowDepthName || name == m_PointShadowDepthName)
				continue;

			auto* target = rtMgr.GetRenderTarget(name);
			if (!target || target->Resize(width, height) != EResult::Success)
				succeeded = false;
		}
	}

	if (!succeeded) return EResult::Fail;

	m_Width = width;
	m_Height = height;
	return EResult::Success;
}

void ViewportDeferredPipeline::Submit(const CameraBuffer& cameraBuffer, Engine::Scene* targetScene)
{
	m_SceneRenderer.Submit(cameraBuffer, targetScene, { m_ShadowPassID, m_PointShadowPassID, m_GeometryPassID,
		m_LightingPassID, m_ForwardPassID, m_ForwardTransparentPassID, m_LightingInputs, m_PointShadowDepthName, m_AcceptSceneMeshes });
}
