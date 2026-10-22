#include "GraphShaderPreview.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "RenderTargetManager.h"

namespace Editor
{
	GraphShaderPreview::~GraphShaderPreview()
	{
		Engine::Safe_Release(m_Pass);
		Engine::Safe_Release(m_Pipeline);
		Engine::Safe_Release(m_PixelShader);
		m_VertexShader.Reset();

		if (!m_TargetName.empty() && Engine::RenderTargetManager::IsCreated())
			Engine::RenderTargetManager::Get().RemoveRenderTarget(m_TargetName);
	}

	std::shared_ptr<GraphShaderPreview> GraphShaderPreview::Create(
		const Engine::ShaderCompileResult& compiled, Engine::ERHIType target,
		const std::string& source, std::string& outErrorKey)
	{
		outErrorKey.clear();

		if (!Engine::Renderer::IsCreated() || !Engine::ResourceManager::IsCreated() ||
			!Engine::RenderTargetManager::IsCreated())
		{
			outErrorKey = "GraphPanel.ShaderPreviewUnavailable";
			return {};
		}

		auto& renderer = Engine::Renderer::Get();
		auto* rhi = renderer.GetRHI();

		if (!rhi || target != Engine::ERHIType::DirectX12 || renderer.GetRHIType() != target)
		{
			outErrorKey = "GraphPanel.ShaderPreviewUnavailable";
			return {};
		}

		if (!compiled.Success || compiled.Bytecode.empty() || compiled.OutputPath.empty())
		{
			outErrorKey = "GraphPanel.ShaderCompileFailed";
			return {};
		}

		auto preview = std::shared_ptr<GraphShaderPreview>(new GraphShaderPreview());
		preview->m_Source = source;
		preview->m_VertexShader =
			Engine::ResourceManager::Get().GetResourceHandle<Engine::Shader>(L"FullscreenQuadVS");

		if (!preview->m_VertexShader || !preview->m_VertexShader->GetRHIShader())
		{
			outErrorKey = "GraphPanel.ShaderPreviewVertexMissing";
			return {};
		}

		RHIShaderDesc shaderDesc{};
		shaderDesc.shaderType = EShaderType::Fragment;
		shaderDesc.entryPoint = "main";
		shaderDesc.shaderBytecode = compiled.Bytecode;
		preview->m_PixelShader = rhi->CreateShader(shaderDesc);

		if (!preview->m_PixelShader)
		{
			outErrorKey = "GraphPanel.ShaderPreviewShaderFailed";
			return {};
		}

		RHIPipelineDesc pipelineDesc{};
		pipelineDesc.vertexShader = preview->m_VertexShader->GetRHIShader();
		pipelineDesc.pixelShader = preview->m_PixelShader;
		pipelineDesc.colorAttachmentCount = 1;
		pipelineDesc.colorAttachmentFormats[0] = Engine::ETextureFormat::R8G8B8A8_UNORM;
		pipelineDesc.depthStencilAttachmentFormat = Engine::ETextureFormat::UNKNOWN;
		pipelineDesc.depthStencilState.depthTestEnable = false;
		pipelineDesc.depthStencilState.depthWriteEnable = false;
		pipelineDesc.cullMode = Engine::ECullMode::None;
		pipelineDesc.topology = Engine::ETopology::TriangleList;
		pipelineDesc.blendState.enable = false;
		preview->m_Pipeline = rhi->CreatePipeline(pipelineDesc);

		if (!preview->m_Pipeline)
		{
			outErrorKey = "GraphPanel.ShaderPreviewPipelineFailed";
			return {};
		}

		auto& targets = Engine::RenderTargetManager::Get();
		const auto targetName = compiled.OutputPath.wstring() + L".GraphPreview";

		if (targets.GetRenderTarget(targetName))
		{
			outErrorKey = "GraphPanel.ShaderPreviewTargetFailed";
			return {};
		}

		preview->m_TargetName = targetName;

		Engine::RenderTargetDesc targetDesc{};
		targetDesc.name = targetName;
		targetDesc.fixedSize = true;
		targetDesc.textureDesc.width = 256;
		targetDesc.textureDesc.height = 256;
		targetDesc.textureDesc.format = Engine::ETextureFormat::R8G8B8A8_UNORM;
		targetDesc.textureDesc.usage = Engine::ETextureUsage::RenderTarget | Engine::ETextureUsage::Sampler;

		if (!targets.CreateRenderTarget(&targetDesc))
		{
			outErrorKey = "GraphPanel.ShaderPreviewTargetFailed";
			return {};
		}

		Engine::RenderPassDesc passDesc{};
		passDesc.name = targetName + L".Pass";
		passDesc.renderTargetNames = { targetName };
		passDesc.loadOperation = Engine::ERenderPassLoadOperation::RPLO_Clear;
		passDesc.storeOperation = Engine::ERenderPassStoreOperation::RPSO_Store;
		passDesc.passType = Engine::ERenderPassType::Custom;
		passDesc.acceptedBlendModes = Engine::EBlendMode::None;
		preview->m_Pass = Engine::RenderPass::Create(&passDesc);

		if (!preview->m_Pass)
		{
			outErrorKey = "GraphPanel.ShaderPreviewPassFailed";
			return {};
		}

		return preview;
	}

	Engine::RHITexture* GraphShaderPreview::GetTexture() const
	{
		if (!Engine::RenderTargetManager::IsCreated()) return nullptr;

		auto* target = Engine::RenderTargetManager::Get().GetRenderTarget(m_TargetName);
		return target ? target->GetTexture() : nullptr;
	}

	Engine::EResult GraphShaderPreview::Render()
	{
		if (!Engine::Renderer::IsCreated() || !m_Pipeline || !m_Pass)
			return Engine::EResult::Fail;

		auto* rhi = Engine::Renderer::Get().GetRHI();
		auto* texture = GetTexture();

		if (!rhi || !texture || rhi->GetCurrentRenderPass())
			return Engine::EResult::Fail;

		auto result = rhi->BeginRenderPass(m_Pass);
		if (Engine::IsFailure(result)) return result;

		result = rhi->SetViewport(0, 0, texture->GetWidth(), texture->GetHeight());
		if (!Engine::IsFailure(result)) result = rhi->BindPipeline(m_Pipeline);
		if (!Engine::IsFailure(result)) result = rhi->Draw(3);

		const auto endResult = rhi->EndRenderPass();
		return Engine::IsFailure(result) ? result : endResult;
	}
}