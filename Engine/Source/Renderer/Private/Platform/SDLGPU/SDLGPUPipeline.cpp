#pragma once

#include "SDLGPUPipeline.h"
#include "Vertex.h"

#pragma region Helper
static SDL_GPUCullMode ToSDLCullMode(ECullMode mode) {
	switch (mode) {
	case ECullMode::None: return SDL_GPU_CULLMODE_NONE;
	case ECullMode::Front: return SDL_GPU_CULLMODE_FRONT;
	case ECullMode::Back: return SDL_GPU_CULLMODE_BACK;
	default: return SDL_GPU_CULLMODE_NONE;
	}
}

static SDL_GPUFillMode ToSDLFillMode(EFillMode mode) {
	return (mode == EFillMode::Wireframe) ? SDL_GPU_FILLMODE_LINE : SDL_GPU_FILLMODE_FILL;
}

static SDL_GPUFrontFace ToSDLFrontFace(EFrontFace face) {
	return (face == EFrontFace::CounterClockwise) ? SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE : SDL_GPU_FRONTFACE_CLOCKWISE;
}

static SDL_GPUCompareOp ToSDLCompareOp(ECompareOp op) {
	// 순서가 같다면 캐스팅 가능, 여기선 안전하게 매핑
	switch (op) {
	case ECompareOp::Never: return SDL_GPU_COMPAREOP_NEVER;
	case ECompareOp::Less: return SDL_GPU_COMPAREOP_LESS;
	case ECompareOp::Equal: return SDL_GPU_COMPAREOP_EQUAL;
	case ECompareOp::LessOrEqual: return SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
	case ECompareOp::Greater: return SDL_GPU_COMPAREOP_GREATER;
	case ECompareOp::NotEqual: return SDL_GPU_COMPAREOP_NOT_EQUAL;
	case ECompareOp::GreaterOrEqual: return SDL_GPU_COMPAREOP_GREATER_OR_EQUAL;
	case ECompareOp::Always: return SDL_GPU_COMPAREOP_ALWAYS;
	default: return SDL_GPU_COMPAREOP_LESS;
	}
}

static SDL_GPUPrimitiveType ToSDLTopology(ETopology topology) {
	switch (topology) {
	case ETopology::TriangleList: return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	case ETopology::TriangleStrip: return SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
	case ETopology::LineList: return SDL_GPU_PRIMITIVETYPE_LINELIST;
	case ETopology::LineStrip: return SDL_GPU_PRIMITIVETYPE_LINESTRIP;
	case ETopology::PointList: return SDL_GPU_PRIMITIVETYPE_POINTLIST;
	default: return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
	}
}

static SDL_GPUTextureFormat ToSDLFormat(ERenderTargetFormat format) {
    switch (format)
    {
    case Engine::ERenderTargetFormat::RTF_RGBA8: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    case Engine::ERenderTargetFormat::RTF_RGBA16F: return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    case Engine::ERenderTargetFormat::RTF_RGBA32F: return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    case Engine::ERenderTargetFormat::RTF_DEPTH24STENCIL8: return SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    case Engine::ERenderTargetFormat::RTF_UINT32: return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_UINT;
    case Engine::ERenderTargetFormat::RTF_UNKNOWN:
    default: return SDL_GPU_TEXTUREFORMAT_INVALID;
    }
}
#pragma endregion

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
    vertexBufferDesc.pitch = sizeof(Vertex);
    vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute vertexAttributes[3] = {};
    vertexAttributes[0].location = 0;
    vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[0].offset = offsetof(Vertex, position);
    vertexAttributes[1].location = 1;
    vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertexAttributes[1].offset = offsetof(Vertex, texCoord);
    vertexAttributes[2].location = 2;
    vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[2].offset = offsetof(Vertex, normal);

    SDL_GPUVertexInputState vertexInputState = {};
    vertexInputState.vertex_buffer_descriptions = &vertexBufferDesc;
    vertexInputState.num_vertex_buffers = 1;
    vertexInputState.vertex_attributes = vertexAttributes;
    vertexInputState.num_vertex_attributes = 3;
    createInfo.vertex_input_state = vertexInputState;

    // =========================================================
    // [누락된 부분 1] Rasterizer State (Cull, Fill, FrontFace)
    // =========================================================
    createInfo.rasterizer_state.cull_mode = ToSDLCullMode(desc.CullMode);
    createInfo.rasterizer_state.fill_mode = ToSDLFillMode(desc.FillMode);
    createInfo.rasterizer_state.front_face = ToSDLFrontFace(desc.FrontFace);
    // Depth Bias가 필요하다면 여기에 추가 (RHIRasterizerState에서 가져옴)

    // =========================================================
    // [누락된 부분 2] Primitive Topology
    // =========================================================
    createInfo.primitive_type = ToSDLTopology(desc.Topology);

    // =========================================================
    // [누락된 부분 3] Depth Stencil State
    // =========================================================
    createInfo.depth_stencil_state.enable_depth_test = desc.DepthStencilState.DepthTestEnable;
    createInfo.depth_stencil_state.enable_depth_write = desc.DepthStencilState.DepthWriteEnable;
    createInfo.depth_stencil_state.compare_op = ToSDLCompareOp(desc.DepthStencilState.DepthCompareOp);
    createInfo.depth_stencil_state.enable_stencil_test = desc.DepthStencilState.StencilTestEnable;

    // =========================================================
    // [누락된 부분 4] Color Targets & Blend State
    // =========================================================
    SDL_GPUColorTargetDescription colorTargets[8] = {};

    for (uint32 i = 0; i < desc.ColorAttachmentCount; ++i)
    {
        colorTargets[i].format = ToSDLFormat(desc.ColorAttachmentFormats[i]);

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

    // =========================================================
    // [누락된 부분 5] Depth Format (Target Info)
    // =========================================================
    if (desc.DepthStencilAttachmentFormat != ERenderTargetFormat::RTF_UNKNOWN)
    {
        targetInfo.has_depth_stencil_target = true;
        targetInfo.depth_stencil_format = ToSDLFormat(desc.DepthStencilAttachmentFormat);
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