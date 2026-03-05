#pragma once

#include "Grid.h"
#include "Mesh.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "PipelineManager.h"

void Grid::Initialize()
{
	tagShaderDesc gridVsDesc = { EShaderType::Vertex, L"Resources/Shader/infinite_grid.vert.spv", "main" };
	ResourceManager::Get().LoadShader(L"InfiniteGridVS", &gridVsDesc);

	tagShaderDesc grid2DPsDesc = { EShaderType::Pixel, L"Resources/Shader/infinite_grid_2d.frag.spv", "main" };
	ResourceManager::Get().LoadShader(L"InfiniteGrid2DPS", &grid2DPsDesc);

	tagShaderDesc grid3DPsDesc = { EShaderType::Pixel, L"Resources/Shader/infinite_grid.frag.spv", "main" };
	ResourceManager::Get().LoadShader(L"InfiniteGrid3DPS", &grid3DPsDesc);
}

void Grid::Free()
{

}

void Grid::SubmitGrid(RenderPassID renderPassID, bool isOrthographic)
{
    // 람다의 매개변수로 RenderPass* 가 넘어오는 것을 적극 활용합니다!
    Renderer::Get().SubmitCustomCommand([this, isOrthographic](f32 dt, RenderPass* renderPass)->EResult
        {
            if (!m_Visible) return EResult::Success;

            RHI* rhi = Renderer::Get().GetRHI();

            // 1. 파이프라인 디스크립터 설정 (RenderComponent와 동일한 방식 적용!)
            tagRHIPipelineDesc pipelineDesc = {};
            pipelineDesc.PipelineType = EPipelineType::Graphics;
            pipelineDesc.VertexShader = ResourceManager::Get().GetShader(L"InfiniteGridVS")->GetRHIShader();
            pipelineDesc.PixelShader = ResourceManager::Get().GetShader(isOrthographic ? L"InfiniteGrid2DPS" : L"InfiniteGrid3DPS")->GetRHIShader();

            pipelineDesc.BlendMode = EBlendMode::AlphaBlend;
            pipelineDesc.FillMode = EFillMode::Solid;
            pipelineDesc.CullMode = ECullMode::None;
            pipelineDesc.Topology = ETopology::TriangleList;
            pipelineDesc.InputLayout = Vertex::Layout;

            // [핵심 1] RenderPass에서 Color Attachment 포맷 동적 매칭
            pipelineDesc.ColorAttachmentCount = renderPass->GetRenderTargetCount();
            for (uint32 i = 0; i < pipelineDesc.ColorAttachmentCount; ++i)
            {
                pipelineDesc.ColorAttachmentFormats[i] = RenderTargetManager::Get().GetRenderTarget(renderPass->GetRenderTargetName(i))->GetFormat();
            }

            // [핵심 2] RenderPass에서 Depth Stencil 포맷 동적 매칭
            wstring depthStencilName = renderPass->GetDepthStencilName();
            if (!depthStencilName.empty())
            {
                pipelineDesc.DepthStencilAttachmentFormat = RenderTargetManager::Get().GetRenderTarget(depthStencilName)->GetFormat();
            }

            // [핵심 3] 깊이 비교 설정 (이전과 동일하게 배경으로 깔리도록 설정)
            pipelineDesc.DepthStencilState.DepthTestEnable = true;
            pipelineDesc.DepthStencilState.DepthWriteEnable = false; // 그리드끼리 가리지 않도록 Write는 끔
            pipelineDesc.DepthStencilState.DepthCompareOp = ECompareOp::LessOrEqual;

            // 2. 파이프라인 가져오기 및 바인딩
            // 매 프레임 호출되어도 PipelineManager 내부 Map에서 캐싱된 포인터를 반환하므로 오버헤드가 없습니다.
            RHIPipeline* pipeline = PipelineManager::Get().GetOrCreatePipeline(pipelineDesc);
            if (!pipeline || IsFailure(rhi->BindPipeline(pipeline)))
                return EResult::Fail;

            // 3. 메쉬 바인딩 및 렌더링
            Mesh* quadMesh = ResourceManager::Get().GetMesh(L"QuadMesh");
            if (!quadMesh) return EResult::Fail;

            rhi->BindVertexBuffer(quadMesh->GetVertexBuffer());
            rhi->BindIndexBuffer(quadMesh->GetIndexBuffer());
            rhi->DrawIndexed(quadMesh->GetIndexCount());

            return EResult::Success;
        }
    , renderPassID);
}