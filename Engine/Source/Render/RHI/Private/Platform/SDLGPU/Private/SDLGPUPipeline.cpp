#pragma once

#include "SDLGPUPipeline.h"

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

static SDL_GPUVertexElementFormat ToSDLVertexFormat(EVertexElementFormat format) {
    switch (format)
    {
    case EVertexElementFormat::Int: return SDL_GPU_VERTEXELEMENTFORMAT_INT;
    case EVertexElementFormat::Int2: return SDL_GPU_VERTEXELEMENTFORMAT_INT2;
    case EVertexElementFormat::Int3: return SDL_GPU_VERTEXELEMENTFORMAT_INT3;
    case EVertexElementFormat::Int4: return SDL_GPU_VERTEXELEMENTFORMAT_INT4;
    case EVertexElementFormat::UInt: return SDL_GPU_VERTEXELEMENTFORMAT_UINT;
    case EVertexElementFormat::UInt2: return SDL_GPU_VERTEXELEMENTFORMAT_UINT2;
    case EVertexElementFormat::UInt3: return SDL_GPU_VERTEXELEMENTFORMAT_UINT3;
    case EVertexElementFormat::UInt4: return SDL_GPU_VERTEXELEMENTFORMAT_UINT4;
	case EVertexElementFormat::Float: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
	case EVertexElementFormat::Float2: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	case EVertexElementFormat::Float3: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	case EVertexElementFormat::Float4: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
	case EVertexElementFormat::Byte2: return SDL_GPU_VERTEXELEMENTFORMAT_BYTE2;
	case EVertexElementFormat::Byte4: return SDL_GPU_VERTEXELEMENTFORMAT_BYTE4;
	case EVertexElementFormat::UByte2: return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2;
	case EVertexElementFormat::UByte4: return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4;
	case EVertexElementFormat::Byte2_Norm: return SDL_GPU_VERTEXELEMENTFORMAT_BYTE2_NORM;
	case EVertexElementFormat::Byte4_Norm: return SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM;
	case EVertexElementFormat::UByte2_Norm: return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2_NORM;
	case EVertexElementFormat::UByte4_Norm: return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
	case EVertexElementFormat::Short2: return SDL_GPU_VERTEXELEMENTFORMAT_SHORT2;
	case EVertexElementFormat::Short4: return SDL_GPU_VERTEXELEMENTFORMAT_SHORT4;
	case EVertexElementFormat::UShort2: return SDL_GPU_VERTEXELEMENTFORMAT_USHORT2;
	case EVertexElementFormat::UShort4: return SDL_GPU_VERTEXELEMENTFORMAT_USHORT4;
	case EVertexElementFormat::Short2_Norm: return SDL_GPU_VERTEXELEMENTFORMAT_SHORT2_NORM;
	case EVertexElementFormat::Short4_Norm: return SDL_GPU_VERTEXELEMENTFORMAT_SHORT4_NORM;
	case EVertexElementFormat::UShort2_Norm: return SDL_GPU_VERTEXELEMENTFORMAT_USHORT2_NORM;
	case EVertexElementFormat::UShort4_Norm: return SDL_GPU_VERTEXELEMENTFORMAT_USHORT4_NORM;
	case EVertexElementFormat::Half2: return SDL_GPU_VERTEXELEMENTFORMAT_HALF2;
	case EVertexElementFormat::Half4: return SDL_GPU_VERTEXELEMENTFORMAT_HALF4;
	default: return SDL_GPU_VERTEXELEMENTFORMAT_INVALID;
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