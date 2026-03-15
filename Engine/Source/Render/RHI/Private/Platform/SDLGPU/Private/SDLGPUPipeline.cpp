#pragma once

#include "SDLGPUPipeline.h"

#pragma region Constructor&Destructor

EResult SDLGPUPipeline::Initialize(const DESC& desc)
{
    SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI());

    SDL_GPUGraphicsPipelineCreateInfo createInfo = {};

    // 1. Shader 설정 (기존 동일)
    Safe_AddRef(desc.VertexShader);
    Safe_AddRef(desc.PixelShader);
    createInfo.vertex_shader = static_cast<SDL_GPUShader*>(desc.VertexShader->GetNativeHandle());
    createInfo.fragment_shader = static_cast<SDL_GPUShader*>(desc.PixelShader->GetNativeHandle());

    // 2. Vertex Input 설정 (기존 동일 - 추후 Mesh InputLayout 연동 필요)
    SDL_GPUVertexBufferDescription vertexBufferDesc = {};
    vertexBufferDesc.slot = 0;
    vertexBufferDesc.pitch = desc.InputLayout.Stride;
    vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

	vector<SDL_GPUVertexAttribute> vertexAttributes(desc.InputLayout.Elements.size());
    for (size_t i = 0; i < desc.InputLayout.Elements.size(); ++i)
    {
        vertexAttributes[i].location = desc.InputLayout.Elements[i].Location;
        vertexAttributes[i].buffer_slot = 0; // 단일 버퍼 가정
        vertexAttributes[i].format = ToSDLVertexFormat(desc.InputLayout.Elements[i].Format);
        vertexAttributes[i].offset = desc.InputLayout.Elements[i].Offset;
	}

    SDL_GPUVertexInputState vertexInputState = {};
    vertexInputState.vertex_buffer_descriptions = &vertexBufferDesc;
    vertexInputState.num_vertex_buffers = 1;
    vertexInputState.vertex_attributes = vertexAttributes.data();
    vertexInputState.num_vertex_attributes = static_cast<uint32>(vertexAttributes.size());
    createInfo.vertex_input_state = vertexInputState;

    createInfo.rasterizer_state.cull_mode = ToSDLCullMode(desc.CullMode);
    createInfo.rasterizer_state.fill_mode = ToSDLFillMode(desc.FillMode);
    createInfo.rasterizer_state.front_face = ToSDLFrontFace(desc.FrontFace);
    // Depth Bias가 필요하다면 여기에 추가 (RHIRasterizerState에서 가져옴)

    createInfo.primitive_type = ToSDLTopology(desc.Topology);
    createInfo.depth_stencil_state.enable_depth_test = desc.DepthStencilState.DepthTestEnable;
    createInfo.depth_stencil_state.enable_depth_write = desc.DepthStencilState.DepthWriteEnable;
    createInfo.depth_stencil_state.compare_op = ToSDLCompareOp(desc.DepthStencilState.DepthCompareOp);
    createInfo.depth_stencil_state.enable_stencil_test = desc.DepthStencilState.StencilTestEnable;

    SDL_GPUColorTargetDescription colorTargets[8] = {};

    for (uint32 i = 0; i < desc.ColorAttachmentCount; ++i)
    {
        colorTargets[i].format = ToSDLGPUTextureFormat(desc.ColorAttachmentFormats[i]);

        // Blend Mode 설정
        if (desc.BlendMode == EBlendMode::Opaque)
        {
            colorTargets[i].blend_state.enable_blend = false;
        }
        else if (desc.BlendMode == EBlendMode::AlphaBlend)
        {
            colorTargets[i].blend_state.enable_blend = true;
            colorTargets[i].blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            colorTargets[i].blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            colorTargets[i].blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            colorTargets[i].blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            colorTargets[i].blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            colorTargets[i].blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        }
        else if (desc.BlendMode == EBlendMode::Additive)
        {
            colorTargets[i].blend_state.enable_blend = true;
            colorTargets[i].blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            colorTargets[i].blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            // ... Additive 설정
        }

        // Color Write Mask (일단 모두 기록)
        colorTargets[i].blend_state.color_write_mask = 0xF;
    }

    SDL_GPUGraphicsPipelineTargetInfo targetInfo = {};
    targetInfo.color_target_descriptions = colorTargets;
    targetInfo.num_color_targets = desc.ColorAttachmentCount;

    if (desc.DepthStencilAttachmentFormat != ETextureFormat::UNKNOWN)
    {
        targetInfo.has_depth_stencil_target = true;
        targetInfo.depth_stencil_format = ToSDLGPUTextureFormat(desc.DepthStencilAttachmentFormat);
    }
    else
    {
        targetInfo.has_depth_stencil_target = false;
    }

    createInfo.target_info = targetInfo;

    // 생성
    m_Pipeline = SDL_CreateGPUGraphicsPipeline(device, &createInfo);
    return m_Pipeline ? EResult::Success : EResult::Fail;
}
SDLGPUPipeline* SDLGPUPipeline::Create(RHI* rhi, const DESC& desc)
{
	SDLGPUPipeline* instance = new SDLGPUPipeline(rhi, desc);
	if (IsFailure(instance->Initialize(desc)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}
void SDLGPUPipeline::Free()
{
	if(m_Pipeline)
	{
		SDL_ReleaseGPUGraphicsPipeline(static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI()), m_Pipeline);
		m_Pipeline = nullptr;
	}
	Safe_Release(m_Desc.VertexShader);
	Safe_Release(m_Desc.PixelShader);
	__super::Free();
}
#pragma endregion