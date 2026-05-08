#include "PostProcess.h"
#include "ResourceManager.h"
#include "PipelineManager.h"
#include "Renderer.h"
#include "RenderTargetManager.h"
#include "SamplerManager.h"
#include "RHI.h"

BEGIN(Editor)

#pragma region ToneMapping
void ToneMapping::Initialize(const wstring& prefix)
{
	Engine::ResourceManager& rm = Engine::ResourceManager::Get();

	tagRHIPipelineDesc pd = {};
	pd.PipelineType = Engine::EPipelineType::Graphics;
	pd.VertexShader = rm.GetResourceHandle<Engine::Shader>(L"FullscreenQuadVS")->GetRHIShader();
	pd.PixelShader = rm.GetResourceHandle<Engine::Shader>(L"PostProcess_ToneMappingPS")->GetRHIShader();
	pd.ColorAttachmentCount = 1;
	pd.ColorAttachmentFormats[0] = Engine::ETextureFormat::R8G8B8A8_UNORM;
	pd.DepthStencilAttachmentFormat = Engine::ETextureFormat::UNKNOWN;
	pd.DepthStencilState.DepthTestEnable = false;
	pd.DepthStencilState.DepthWriteEnable = false;
	pd.Topology = Engine::ETopology::TriangleList;
	pd.CullMode = Engine::ECullMode::None;
	pd.BlendState = Engine::tagBlendState{};

	m_Pipeline = Engine::PipelineManager::Get().GetOrCreatePipeline(pd);

	// 기본값 초기화
	m_Params.exposure = 1.0f;
	m_Params.gamma = 2.2f;
	m_Enabled = true; // 기본적으로 켜둠 (SceneViewportPanel에서 기존 하던 방식)
}

void ToneMapping::Free()
{
	// PipelineManager가 관리하므로 직접 해제하지 않음
	m_Pipeline = nullptr;
}

EResult ToneMapping::SubmitPass(f32 dt, Engine::RenderPassID passID, const wstring& sourceRT)
{
	if (!m_Pipeline) return EResult::Fail;

	Engine::Renderer::Get().SubmitCustomCommand(
		[this, capturedRead = sourceRT](f32 dt, Engine::RenderPass* pass) -> EResult
		{
			auto* src = Engine::RenderTargetManager::Get().GetRenderTarget(capturedRead);
			if (!src || !src->GetTexture()) return EResult::Fail;

			auto* rhi = Engine::Renderer::Get().GetRHI();
			rhi->BindTextureSampler(src->GetTexture(), Engine::SamplerManager::Get().GetDefaultSampler(), 0);
			rhi->BindConstantBuffer(&m_Params, sizeof(m_Params), 0, EShaderType::Pixel);
			rhi->BindPipeline(m_Pipeline);
			return rhi->Draw(3);
		}, passID);

	return EResult::Success;
}

void ToneMapping::DrawImGuiOptions()
{
	ImGui::SetNextItemWidth(160.f);
	ImGui::SliderFloat("Exposure", &m_Params.exposure, 0.1f, 5.0f, "%.2f");
	ImGui::SetNextItemWidth(160.f);
	ImGui::SliderFloat("Gamma", &m_Params.gamma, 1.0f, 3.0f, "%.2f");
	if (ImGui::Button("Reset", ImVec2(-1, 0)))
	{
		m_Params.exposure = 1.0f;
		m_Params.gamma = 2.2f;
	}
}
#pragma endregion

#pragma region PostProcessChain
void PostProcessChain::Initialize(const wstring& prefix)
{
	// 기본 이펙트들 등록
	ToneMapping* toneMapping = new ToneMapping();
	toneMapping->Initialize(prefix);
	AddPostProcess(toneMapping);
}

void PostProcessChain::Free()
{
	for (auto* effect : m_Effects)
	{
		effect->Free();
		delete effect;
	}
	m_Effects.clear();
}

void PostProcessChain::AddPostProcess(PostProcess* effect)
{
	m_Effects.push_back(effect);
}

void PostProcessChain::ExecuteChain(f32 dt, const std::vector<Engine::RenderPassID>& passIDs, wstring& currentRT, const wstring* ppRTNames)
{
	uint32 slot = 0;
	for (auto* effect : m_Effects)
	{
		if (!effect->IsEnabled()) continue;
		if (slot >= passIDs.size()) break;

		const wstring writeRT = ppRTNames[slot % 2];
		
		effect->SubmitPass(dt, passIDs[slot], currentRT);

		currentRT = writeRT; // 다음 패스를 위해 읽을 타겟 갱신
		++slot;
	}
}

void PostProcessChain::DrawImGuiMenu()
{
	if (ImGui::BeginMenu("Post FX"))
	{
		ImGui::SeparatorText("Effects");

		for (int i = 0; i < (int)m_Effects.size(); ++i)
		{
			auto* effect = m_Effects[i];
			string effectName = WStrToStr(effect->GetName());
			string checkboxID = "##PPEnabled_" + std::to_string(i);

			bool enabled = effect->IsEnabled();
			if (ImGui::Checkbox(checkboxID.c_str(), &enabled))
			{
				effect->SetEnabled(enabled);
			}
			ImGui::SameLine();

			if (enabled)
			{
				if (ImGui::BeginMenu(effectName.c_str()))
				{
					effect->DrawImGuiOptions();
					ImGui::EndMenu();
				}
			}
			else
			{
				ImGui::TextDisabled("%s", effectName.c_str());
			}
		}

		ImGui::EndMenu();
	}
}
#pragma endregion

END
