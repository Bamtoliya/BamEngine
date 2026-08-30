#pragma once

#include "SDLGPUPipeline.h"
#include "SDLGPUTypes.h"

#pragma region Constructor&Destructor

EResult SDLGPUPipeline::Initialize(const DESC& desc)
{
    SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI());

    SDL_GPUGraphicsPipelineCreateInfo createInfo = {};

    // 1. Shader 설정 (기존 동일)
    Safe_AddRef(desc.vertexShader);
    Safe_AddRef(desc.pixelShader);
    createInfo.vertex_shader = static_cast<SDL_GPUShader*>(desc.vertexShader->GetNativeHandle());
    createInfo.fragment_shader = static_cast<SDL_GPUShader*>(desc.pixelShader->GetNativeHandle());

    // 2. Vertex Input 설정 (기존 동일 - 추후 Mesh InputLayout 연동 필요)
    uint32 numBuffers = static_cast<uint32>(desc.inputLayouts.size());
    // 슬롯별 Buffer Description 배열 생성
    vector<SDL_GPUVertexBufferDescription> vertexBufferDescs(numBuffers);
    for (uint32 i = 0; i < numBuffers; ++i)
    {
        vertexBufferDescs[i].slot = i;
        vertexBufferDescs[i].pitch = desc.inputLayouts[i].stride;
        vertexBufferDescs[i].input_rate = (desc.inputLayouts[i].inputRate == EVertexInputRate::PerInstance)
            ? SDL_GPU_VERTEXINPUTRATE_INSTANCE
            : SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDescs[i].instance_step_rate =
            (desc.inputLayouts[i].inputRate == EVertexInputRate::PerInstance)
            ? (desc.inputLayouts[i].instanceDataStepRate == 0 ? 1 : desc.inputLayouts[i].instanceDataStepRate)
            : 0;
    }

    vector<SDL_GPUVertexAttribute> vertexAttributes;
    uint32 locationOffset = 0;
    for (uint32 slot = 0; slot < numBuffers; ++slot)
    {
        for (const auto& elem : desc.inputLayouts[slot].elements)
        {
            SDL_GPUVertexAttribute attr = {};
            attr.location = locationOffset + elem.location;  // 오프셋 적용!
            attr.buffer_slot = slot;
            attr.format = ToSDLVertexFormat(elem.format);
            attr.offset = elem.offset;
            vertexAttributes.push_back(attr);
        }
        // 다음 슬롯은 이 슬롯의 attribute 개수만큼 뒤에서 시작
        locationOffset += static_cast<uint32>(desc.inputLayouts[slot].elements.size());
    }

    SDL_GPUVertexInputState vertexInputState = {};
    vertexInputState.vertex_buffer_descriptions = vertexBufferDescs.data();
    vertexInputState.num_vertex_buffers = numBuffers;
    vertexInputState.vertex_attributes = vertexAttributes.data();
    vertexInputState.num_vertex_attributes = static_cast<uint32>(vertexAttributes.size());
    createInfo.vertex_input_state = vertexInputState;

    createInfo.rasterizer_state.cull_mode = ToSDLCullMode(desc.cullMode);
    createInfo.rasterizer_state.fill_mode = ToSDLFillMode(desc.fillMode);
    createInfo.rasterizer_state.front_face = ToSDLFrontFace(desc.frontFace);
    // Depth Bias가 필요하다면 여기에 추가 (RHIRasterizerState에서 가져옴)

    createInfo.primitive_type = ToSDLTopology(desc.topology);
    createInfo.depth_stencil_state.enable_depth_test = desc.depthStencilState.depthTestEnable;
    createInfo.depth_stencil_state.enable_depth_write = desc.depthStencilState.depthWriteEnable;
    createInfo.depth_stencil_state.compare_op = ToSDLCompareOp(desc.depthStencilState.depthCompareOp);
    createInfo.depth_stencil_state.enable_stencil_test = desc.depthStencilState.stencilTestEnable;

    SDL_GPUColorTargetDescription colorTargets[8] = {};

    for (uint32 i = 0; i < desc.colorAttachmentCount; ++i)
    {
        colorTargets[i].format = ToSDLGPUTextureFormat(desc.colorAttachmentFormats[i]);

        colorTargets[i].blend_state.enable_blend = desc.blendState.enable;
        colorTargets[i].blend_state.src_color_blendfactor = ToSDLBlendFactor(desc.blendState.srcColor);
        colorTargets[i].blend_state.dst_color_blendfactor = ToSDLBlendFactor(desc.blendState.dstColor);
        colorTargets[i].blend_state.color_blend_op = ToSDLBlendOp(desc.blendState.colorBlendOp);
        colorTargets[i].blend_state.src_alpha_blendfactor = ToSDLBlendFactor(desc.blendState.srcAlpha);
        colorTargets[i].blend_state.dst_alpha_blendfactor = ToSDLBlendFactor(desc.blendState.dstAlpha);
        colorTargets[i].blend_state.alpha_blend_op = ToSDLBlendOp(desc.blendState.alphaBlendOp);
        colorTargets[i].blend_state.color_write_mask = (int)desc.blendState.colorWriteMask;
		colorTargets[i].blend_state.enable_color_write_mask = desc.blendState.enableColorWriteMask;
    }

    SDL_GPUGraphicsPipelineTargetInfo targetInfo = {};
    targetInfo.color_target_descriptions = colorTargets;
    targetInfo.num_color_targets = desc.colorAttachmentCount;

    if (desc.depthStencilAttachmentFormat != ETextureFormat::UNKNOWN)
    {
        targetInfo.has_depth_stencil_target = true;
        targetInfo.depth_stencil_format = ToSDLGPUTextureFormat(desc.depthStencilAttachmentFormat);
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
	Safe_Release(m_Desc.vertexShader);
	Safe_Release(m_Desc.pixelShader);
	__super::Free();
}
#pragma endregion