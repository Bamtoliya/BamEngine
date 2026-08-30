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
    ShaderDesc gridVsDesc = {};
    gridVsDesc.Key = L"InfiniteGridVS";
    gridVsDesc.Path = L"Resources/Shader/infinite_grid.vert.spv";
    gridVsDesc.spirvPath = L"Resources/Shader/infinite_grid.vert.spv";
    gridVsDesc.shaderType = EShaderType::Vertex;
    gridVsDesc.entryPoint = "main";
    resourceManager.LoadResource<Shader>(&gridVsDesc);
    {
        auto handle = resourceManager.GetResourceHandle<Shader>(L"InfiniteGridVS");
        resourceManager.SaveToBinaryFile(handle.Get(), L"Resources/Shader/infinite_grid.vert.bamshader");
    }

    ShaderDesc grid2DPsDesc = {};
    grid2DPsDesc.Key = L"InfiniteGrid2DPS";
    grid2DPsDesc.Path = L"Resources/Shader/infinite_grid_2d.frag.spv";
    grid2DPsDesc.spirvPath = L"Resources/Shader/infinite_grid_2d.frag.spv";
    grid2DPsDesc.shaderType = EShaderType::Pixel;
    grid2DPsDesc.entryPoint = "main";
    resourceManager.LoadResource<Shader>(&grid2DPsDesc);
    {
        auto handle = resourceManager.GetResourceHandle<Shader>(L"InfiniteGrid2DPS");
        resourceManager.SaveToBinaryFile(handle.Get(), L"Resources/Shader/infinite_grid_2d.frag.bamshader");
    }


    ShaderDesc grid3DPsDesc = {};
    grid3DPsDesc.Key = L"InfiniteGrid3DPS";
    grid3DPsDesc.Path = L"Resources/Shader/infinite_grid.frag.spv";
    grid3DPsDesc.spirvPath = L"Resources/Shader/infinite_grid.frag.spv";
    grid3DPsDesc.shaderType = EShaderType::Pixel;
    grid3DPsDesc.entryPoint = "main";
    resourceManager.LoadResource<Shader>(&grid3DPsDesc);
    {
        auto handle = resourceManager.GetResourceHandle<Shader>(L"InfiniteGrid3DPS");
        resourceManager.SaveToBinaryFile(handle.Get(), L"Resources/Shader/infinite_grid.frag.bamshader");
    }
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
            auto quadMeshHandle = ResourceManager::Get().GetResourceHandle<Mesh>(L"QuadMesh");
            Mesh* quadMesh = quadMeshHandle.Get();
            if (!quadMesh) return EResult::Fail;

            // 매 프레임 RT 이름으로 포맷 조회 → pipelineDesc 구성
            RHIPipelineDesc desc = {};
            desc.pipelineType = EPipelineType::Graphics;
            auto gridVSHandle = ResourceManager::Get().GetResourceHandle<Shader>(L"InfiniteGridVS");
            auto gridPSHandle = ResourceManager::Get().GetResourceHandle<Shader>(
                isOrthographic ? L"InfiniteGrid2DPS" : L"InfiniteGrid3DPS");
            desc.vertexShader = gridVSHandle->GetRHIShader();
            desc.pixelShader = gridPSHandle->GetRHIShader();
            desc.blendState = Engine::BlendState{EBlendMode::AlphaBlend};
            desc.fillMode = EFillMode::Solid;
            desc.cullMode = ECullMode::None;
            desc.topology = ETopology::TriangleList;
            desc.inputLayouts = quadMesh->GetInputLayoutDescs();

            auto* colorRT = RenderTargetManager::Get().GetRenderTarget(colorRTName);
            auto* depthRT = RenderTargetManager::Get().GetRenderTarget(depthStencilName);

            desc.colorAttachmentCount = colorRT ? 1 : 0;
            desc.colorAttachmentFormats[0] = colorRT ? colorRT->GetFormat() : ETextureFormat::R8G8B8A8_UNORM;
            desc.depthStencilAttachmentFormat = depthRT ? depthRT->GetFormat() : ETextureFormat::UNKNOWN;

            desc.depthStencilState.depthTestEnable = true;
            desc.depthStencilState.depthWriteEnable = false;
            desc.depthStencilState.depthCompareOp = isOrthographic ? ECompareOp::LessOrEqual : ECompareOp::Less;

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

