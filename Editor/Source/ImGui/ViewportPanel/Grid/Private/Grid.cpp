#pragma once

#include "Grid.h"
#include "Mesh.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "PipelineManager.h"

void Grid::Initialize(const wstring& prefix)
{
    PrepareShaders();
    PrepareRenderPass(prefix + L"_");
}

void Grid::PrepareShaders()
{
    ResourceManager& resourceManager = ResourceManager::Get();
    tagShaderDesc gridVsDesc = {};
    gridVsDesc.Key = L"InfiniteGridVS";
    gridVsDesc.Path = L"Resources/Shader/infinite_grid.vert.spv";
    gridVsDesc.SpirvPath = L"Resources/Shader/infinite_grid.vert.spv";
    gridVsDesc.ShaderType = EShaderType::Vertex;
    gridVsDesc.EntryPoint = "main";
    resourceManager.LoadResource<Shader>(&gridVsDesc);
    resourceManager.SaveToBinaryFile(resourceManager.GetResourceHandle<Shader>(L"InfiniteGridVS").Get(), L"Resources/Shader/infinite_grid.vert.bamshader");

    tagShaderDesc grid2DPsDesc = {};
    grid2DPsDesc.Key = L"InfiniteGrid2DPS";
    grid2DPsDesc.Path = L"Resources/Shader/infinite_grid_2d.frag.spv";
    grid2DPsDesc.SpirvPath = L"Resources/Shader/infinite_grid_2d.frag.spv";
    grid2DPsDesc.ShaderType = EShaderType::Pixel;
    grid2DPsDesc.EntryPoint = "main";
    resourceManager.LoadResource<Shader>(&grid2DPsDesc);
    resourceManager.SaveToBinaryFile(resourceManager.GetResourceHandle<Shader>(L"InfiniteGrid2DPS").Get(), L"Resources/Shader/infinite_grid_2d.frag.bamshader");


    tagShaderDesc grid3DPsDesc = {};
    grid3DPsDesc.Key = L"InfiniteGrid3DPS";
    grid3DPsDesc.Path = L"Resources/Shader/infinite_grid.frag.spv";
    grid3DPsDesc.SpirvPath = L"Resources/Shader/infinite_grid.frag.spv";
    grid3DPsDesc.ShaderType = EShaderType::Pixel;
    grid3DPsDesc.EntryPoint = "main";
    resourceManager.LoadResource<Shader>(&grid3DPsDesc);
    resourceManager.SaveToBinaryFile(resourceManager.GetResourceHandle<Shader>(L"InfiniteGrid3DPS").Get(), L"Resources/Shader/infinite_grid.frag.bamshader");
}

void Grid::PrepareRenderPass(const wstring& prefix)
{
    m_GridPassID = RenderPassManager::Get().RegisterRenderPass(
        prefix + L"GridPass", {}, L"",
        ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
        ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
        vec4(0.f, 0.f, 0.f, -1.f),
        410,
        ERenderSortType::None, ERenderPassType::Custom, EBlendMode::None);
}

void Grid::Free()
{

}

void Grid::SubmitGrid(Camera* camera, bool isOrthographic, const wstring& colorRTName, const wstring& depthStencilName)
{
    if (m_GridPassID == INVALID_PASS_ID)
        return;

    RenderPass* pass = RenderPassManager::Get().GetRenderPassByID(m_GridPassID);
    if (!pass) return;

    if (!colorRTName.empty())
        pass->SetColorAttachments({ colorRTName });
    if (!depthStencilName.empty())
        pass->SetDepthStencilAttachment(depthStencilName);

    Renderer::Get().RegisterViewportCamera(camera, pass);

    // 람다의 매개변수로 RenderPass* 가 넘어오는 것을 적극 활용합니다!
    Renderer::Get().SubmitCustomCommand(
        [this, isOrthographic, colorRTName, depthStencilName](f32 dt, RenderPass* renderPass)->EResult
        {
            if (!m_Visible) return EResult::Success;    
            if (!renderPass) return EResult::Fail;

            RHI* rhi = Renderer::Get().GetRHI();
            Mesh* quadMesh = ResourceManager::Get().GetResourceHandle<Mesh>(L"QuadMesh").Get();
            if (!quadMesh) return EResult::Fail;

            // 매 프레임 RT 이름으로 포맷 조회 → pipelineDesc 구성
            tagRHIPipelineDesc desc = {};
            desc.PipelineType = EPipelineType::Graphics;
            desc.VertexShader = ResourceManager::Get().GetResourceHandle<Shader>(L"InfiniteGridVS")->GetRHIShader();
            desc.PixelShader = ResourceManager::Get().GetResourceHandle<Shader>(
                isOrthographic ? L"InfiniteGrid2DPS" : L"InfiniteGrid3DPS")->GetRHIShader();
            desc.BlendState = Engine::tagBlendState{EBlendMode::AlphaBlend};
            desc.FillMode = EFillMode::Solid;
            desc.CullMode = ECullMode::None;
            desc.Topology = ETopology::TriangleList;
            desc.InputLayouts = quadMesh->GetInputLayoutDescs();

            auto* colorRT = RenderTargetManager::Get().GetRenderTarget(colorRTName);
            auto* depthRT = RenderTargetManager::Get().GetRenderTarget(depthStencilName);

            desc.ColorAttachmentCount = colorRT ? 1 : 0;
            desc.ColorAttachmentFormats[0] = colorRT ? colorRT->GetFormat() : ETextureFormat::R8G8B8A8_UNORM;
            desc.DepthStencilAttachmentFormat = depthRT ? depthRT->GetFormat() : ETextureFormat::UNKNOWN;

            desc.DepthStencilState.DepthTestEnable = true;
            desc.DepthStencilState.DepthWriteEnable = false;
            desc.DepthStencilState.DepthCompareOp = isOrthographic ? ECompareOp::LessOrEqual : ECompareOp::Less;

            // PipelineManager 캐시에서 조회 → 없을 때만 실제 생성
            RHIPipeline* pipeline = PipelineManager::Get().GetOrCreatePipeline(desc);
            if (!pipeline || IsFailure(rhi->BindPipeline(pipeline)))
                return EResult::Fail;

            if (IsFailure(quadMesh->Bind(0)))
                return EResult::Fail;

            return rhi->DrawIndexed(quadMesh->GetIndexCount());
        },
        m_GridPassID);
}

