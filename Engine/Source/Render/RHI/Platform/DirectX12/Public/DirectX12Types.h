#pragma once

#include "Engine_Includes.h"
#include "RHITypes.h"
#include "RenderTypes.h"
#include "SamplerDesc.h"
#include "Vertex.h"


constexpr DXGI_FORMAT ToDXGIFormat(Engine::ETextureFormat format)
{
	switch (format)
	{
	case Engine::ETextureFormat::A8_UNORM: return DXGI_FORMAT_A8_UNORM;
	case Engine::ETextureFormat::R8_UNORM: return DXGI_FORMAT_R8_UNORM;
	case Engine::ETextureFormat::R8G8_UNORM: return DXGI_FORMAT_R8G8_UNORM;
	case Engine::ETextureFormat::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case Engine::ETextureFormat::R16_UNORM: return DXGI_FORMAT_R16_UNORM;
	case Engine::ETextureFormat::R16G16_UNORM: return DXGI_FORMAT_R16G16_UNORM;
	case Engine::ETextureFormat::R16G16B16A16_UNORM: return DXGI_FORMAT_R16G16B16A16_UNORM;
	case Engine::ETextureFormat::R10G10B10A2_UNORM: return DXGI_FORMAT_R10G10B10A2_UNORM;
	case Engine::ETextureFormat::B5G6R5_UNORM: return DXGI_FORMAT_B5G6R5_UNORM;
	case Engine::ETextureFormat::B5G5R5A1_UNORM: return DXGI_FORMAT_B5G5R5A1_UNORM;
	case Engine::ETextureFormat::B4G4R4A4_UNORM: return DXGI_FORMAT_B4G4R4A4_UNORM;
	case Engine::ETextureFormat::B8G8R8A8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM;
	case Engine::ETextureFormat::BC1_RGBA_UNORM: return DXGI_FORMAT_BC1_UNORM;
	case Engine::ETextureFormat::BC2_RGBA_UNORM: return DXGI_FORMAT_BC2_UNORM;
	case Engine::ETextureFormat::BC3_RGBA_UNORM: return DXGI_FORMAT_BC3_UNORM;
	case Engine::ETextureFormat::BC4_R_UNORM: return DXGI_FORMAT_BC4_UNORM;
	case Engine::ETextureFormat::BC5_RG_UNORM: return DXGI_FORMAT_BC5_UNORM;
	case Engine::ETextureFormat::BC7_RGBA_UNORM: return DXGI_FORMAT_BC7_UNORM;
	case Engine::ETextureFormat::BC6H_RGB_FLOAT: return DXGI_FORMAT_BC6H_SF16;
	case Engine::ETextureFormat::BC6H_RGB_UFLOAT: return DXGI_FORMAT_BC6H_UF16;
	case Engine::ETextureFormat::R8_SNORM: return DXGI_FORMAT_R8_SNORM;
	case Engine::ETextureFormat::R8G8_SNORM: return DXGI_FORMAT_R8G8_SNORM;
	case Engine::ETextureFormat::R8G8B8A8_SNORM: return DXGI_FORMAT_R8G8B8A8_SNORM;
	case Engine::ETextureFormat::R16_SNORM: return DXGI_FORMAT_R16_SNORM;
	case Engine::ETextureFormat::R16G16_SNORM: return DXGI_FORMAT_R16G16_SNORM;
	case Engine::ETextureFormat::R16G16B16A16_SNORM: return DXGI_FORMAT_R16G16B16A16_SNORM;
	case Engine::ETextureFormat::R16_FLOAT: return DXGI_FORMAT_R16_FLOAT;
	case Engine::ETextureFormat::R16G16_FLOAT: return DXGI_FORMAT_R16G16_FLOAT;
	case Engine::ETextureFormat::R16G16B16A16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case Engine::ETextureFormat::R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
	case Engine::ETextureFormat::R32G32_FLOAT: return DXGI_FORMAT_R32G32_FLOAT;
	case Engine::ETextureFormat::R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case Engine::ETextureFormat::R11G11B10_UFLOAT: return DXGI_FORMAT_R11G11B10_FLOAT;
	case Engine::ETextureFormat::R8_UINT: return DXGI_FORMAT_R8_UINT;
	case Engine::ETextureFormat::R8G8_UINT: return DXGI_FORMAT_R8G8_UINT;
	case Engine::ETextureFormat::R8G8B8A8_UINT: return DXGI_FORMAT_R8G8B8A8_UINT;
	case Engine::ETextureFormat::R16_UINT: return DXGI_FORMAT_R16_UINT;
	case Engine::ETextureFormat::R16G16_UINT: return DXGI_FORMAT_R16G16_UINT;
	case Engine::ETextureFormat::R16G16B16A16_UINT: return DXGI_FORMAT_R16G16B16A16_UINT;
	case Engine::ETextureFormat::R32_UINT: return DXGI_FORMAT_R32_UINT;
	case Engine::ETextureFormat::R32G32_UINT: return DXGI_FORMAT_R32G32_UINT;
	case Engine::ETextureFormat::R32G32B32A32_UINT: return DXGI_FORMAT_R32G32B32A32_UINT;
	case Engine::ETextureFormat::R8_INT: return DXGI_FORMAT_R8_SINT;
	case Engine::ETextureFormat::R8G8_INT: return DXGI_FORMAT_R8G8_SINT;
	case Engine::ETextureFormat::R8G8B8A8_INT: return DXGI_FORMAT_R8G8B8A8_SINT;
	case Engine::ETextureFormat::R16_INT: return DXGI_FORMAT_R16_SINT;
	case Engine::ETextureFormat::R16G16_INT: return DXGI_FORMAT_R16G16_SINT;
	case Engine::ETextureFormat::R16G16B16A16_INT: return DXGI_FORMAT_R16G16B16A16_SINT;
	case Engine::ETextureFormat::R32_INT: return DXGI_FORMAT_R32_SINT;
	case Engine::ETextureFormat::R32G32_INT: return DXGI_FORMAT_R32G32_SINT;
	case Engine::ETextureFormat::R32G32B32A32_INT: return DXGI_FORMAT_R32G32B32A32_SINT;
	case Engine::ETextureFormat::R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case Engine::ETextureFormat::B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	case Engine::ETextureFormat::BC1_RGBA_UNORM_SRGB: return DXGI_FORMAT_BC1_UNORM;
	case Engine::ETextureFormat::BC2_RGBA_UNORM_SRGB: return DXGI_FORMAT_BC2_UNORM;
	case Engine::ETextureFormat::BC3_RGBA_UNORM_SRGB: return DXGI_FORMAT_BC3_UNORM;
	case Engine::ETextureFormat::BC7_RGBA_UNORM_SRGB: return DXGI_FORMAT_BC7_UNORM;
	case Engine::ETextureFormat::D16_UNORM: return DXGI_FORMAT_D16_UNORM;
	case Engine::ETextureFormat::D24_UNORM: return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case Engine::ETextureFormat::D32_FLOAT: return DXGI_FORMAT_D32_FLOAT;
	case Engine::ETextureFormat::D24_UNORM_S8_UINT: return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case Engine::ETextureFormat::D32_FLOAT_S8_UINT: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	case Engine::ETextureFormat::ASTC_4x4_UNORM: 
	case Engine::ETextureFormat::ASTC_5x4_UNORM:
	case Engine::ETextureFormat::ASTC_5x5_UNORM:
	case Engine::ETextureFormat::ASTC_6x5_UNORM:
	case Engine::ETextureFormat::ASTC_6x6_UNORM:
	case Engine::ETextureFormat::ASTC_8x5_UNORM:
	case Engine::ETextureFormat::ASTC_8x6_UNORM:	
	case Engine::ETextureFormat::ASTC_8x8_UNORM:	
	case Engine::ETextureFormat::ASTC_10x5_UNORM:	
	case Engine::ETextureFormat::ASTC_10x6_UNORM:	
	case Engine::ETextureFormat::ASTC_10x8_UNORM:	
	case Engine::ETextureFormat::ASTC_10x10_UNORM:	
	case Engine::ETextureFormat::ASTC_12x10_UNORM:	
	case Engine::ETextureFormat::ASTC_12x12_UNORM:	
	case Engine::ETextureFormat::ASTC_4x4_UNORM_SRGB:	
	case Engine::ETextureFormat::ASTC_5x4_UNORM_SRGB:	
	case Engine::ETextureFormat::ASTC_5x5_UNORM_SRGB:	
	case Engine::ETextureFormat::ASTC_6x5_UNORM_SRGB:	
	case Engine::ETextureFormat::ASTC_6x6_UNORM_SRGB:	
	case Engine::ETextureFormat::ASTC_8x5_UNORM_SRGB:	
	case Engine::ETextureFormat::ASTC_8x6_UNORM_SRGB:	
	case Engine::ETextureFormat::ASTC_8x8_UNORM_SRGB:	
	case Engine::ETextureFormat::ASTC_10x5_UNORM_SRGB:	
	case Engine::ETextureFormat::ASTC_10x6_UNORM_SRGB:	
	case Engine::ETextureFormat::ASTC_10x8_UNORM_SRGB:
	case Engine::ETextureFormat::ASTC_10x10_UNORM_SRGB:
	case Engine::ETextureFormat::ASTC_12x10_UNORM_SRGB:
	case Engine::ETextureFormat::ASTC_12x12_UNORM_SRGB:
	case Engine::ETextureFormat::ASTC_4x4_FLOAT:
	case Engine::ETextureFormat::ASTC_5x4_FLOAT:
	case Engine::ETextureFormat::ASTC_5x5_FLOAT:
	case Engine::ETextureFormat::ASTC_6x5_FLOAT:
	case Engine::ETextureFormat::ASTC_6x6_FLOAT:
	case Engine::ETextureFormat::ASTC_8x5_FLOAT:
	case Engine::ETextureFormat::ASTC_8x6_FLOAT:
	case Engine::ETextureFormat::ASTC_8x8_FLOAT:
	case Engine::ETextureFormat::ASTC_10x5_FLOAT:
	case Engine::ETextureFormat::ASTC_10x6_FLOAT:
	case Engine::ETextureFormat::ASTC_10x8_FLOAT:
	case Engine::ETextureFormat::ASTC_10x10_FLOAT:
	case Engine::ETextureFormat::ASTC_12x10_FLOAT:
	case Engine::ETextureFormat::ASTC_12x12_FLOAT:
	case Engine::ETextureFormat::UNKNOWN:
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}


constexpr D3D12_FILL_MODE ToD3D12FillMode(Engine::EFillMode fillMode)
{
	switch (fillMode)
	{
	case Engine::EFillMode::Wireframe: return D3D12_FILL_MODE_WIREFRAME;
	case Engine::EFillMode::Solid: return D3D12_FILL_MODE_SOLID;
	default: return D3D12_FILL_MODE_SOLID; // Default to SOLID if unknown
	}
}


constexpr D3D12_CULL_MODE ToD3D12CullMode(Engine::ECullMode cullMode)
{
	switch (cullMode)
	{
	case Engine::ECullMode::None: return D3D12_CULL_MODE_NONE;
	case Engine::ECullMode::Front: return D3D12_CULL_MODE_FRONT;
	case Engine::ECullMode::Back: return D3D12_CULL_MODE_BACK;
	default: return D3D12_CULL_MODE_NONE; // Default to NONE if unknown
	}
}

constexpr BOOL ToD3D12FrontFace(Engine::EFrontFace frontFace)
{
	return (frontFace == Engine::EFrontFace::Clockwise) ? TRUE : FALSE;
}

constexpr D3D12_COMPARISON_FUNC ToD3D12ComparisonFunc(Engine::ECompareOp compareOp)
{
	switch (compareOp)
	{
	case Engine::ECompareOp::Never: return D3D12_COMPARISON_FUNC_NEVER;
	case Engine::ECompareOp::Less: return D3D12_COMPARISON_FUNC_LESS;
	case Engine::ECompareOp::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
	case Engine::ECompareOp::LessOrEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case Engine::ECompareOp::Greater: return D3D12_COMPARISON_FUNC_GREATER;
	case Engine::ECompareOp::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
	case Engine::ECompareOp::GreaterOrEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	case Engine::ECompareOp::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
	default: return D3D12_COMPARISON_FUNC_NONE; // Default to NONE if unknown
	}
}

constexpr D3D12_PRIMITIVE_TOPOLOGY_TYPE ToD3D12PrimitiveTopologyType(Engine::ETopology topology)
{
	switch (topology)
	{
	case Engine::ETopology::TriangleList: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case Engine::ETopology::TriangleStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case Engine::ETopology::LineList: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case Engine::ETopology::LineStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case Engine::ETopology::PointList: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	default: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED; // Default to UNDEFINED if unknown
	}
}

constexpr D3D12_PRIMITIVE_TOPOLOGY ToD3D12PrimitiveTopology(Engine::ETopology topology)
{
	switch (topology)
	{
	case Engine::ETopology::TriangleList: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case Engine::ETopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case Engine::ETopology::LineList: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case Engine::ETopology::LineStrip: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case Engine::ETopology::PointList: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	default: return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED; // Default to UNDEFINED if unknown
	}
}

constexpr D3D12_BLEND ToD3D12Blend(Engine::EBlendFactor blendFactor)
{
	switch (blendFactor)
	{
	case Engine::EBlendFactor::Zero: return D3D12_BLEND_ZERO;
	case Engine::EBlendFactor::One: return D3D12_BLEND_ONE;
	case Engine::EBlendFactor::SrcColor: return D3D12_BLEND_SRC_COLOR;
	case Engine::EBlendFactor::InvSrcColor: return D3D12_BLEND_INV_SRC_COLOR;
	case Engine::EBlendFactor::DstColor: return D3D12_BLEND_DEST_COLOR;
	case Engine::EBlendFactor::InvDstColor: return D3D12_BLEND_INV_DEST_COLOR;
	case Engine::EBlendFactor::SrcAlpha: return D3D12_BLEND_SRC_ALPHA;
	case Engine::EBlendFactor::InvSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
	case Engine::EBlendFactor::DstAlpha: return D3D12_BLEND_DEST_ALPHA;
	case Engine::EBlendFactor::InvDstAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
	default: return D3D12_BLEND_ONE; // Default to ONE if unknown
	}
}

constexpr D3D12_BLEND_OP ToD3D12BlendOp(Engine::EBlendOp blendOp)
{
	switch (blendOp)
	{
	case Engine::EBlendOp::Add: return D3D12_BLEND_OP_ADD;
	case Engine::EBlendOp::Subtract: return D3D12_BLEND_OP_SUBTRACT;
	case Engine::EBlendOp::RevSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
	case Engine::EBlendOp::Min: return D3D12_BLEND_OP_MIN;
	case Engine::EBlendOp::Max: return D3D12_BLEND_OP_MAX;
	default: return D3D12_BLEND_OP_ADD; // Default to ADD if unknown
	}
}

constexpr D3D12_COLOR_WRITE_ENABLE ToD3D12ColorWriteEnable(Engine::EColorChannel colorChannel)
{
	uint8 enable = 0;
	if (Engine::HasFlag(colorChannel, Engine::EColorChannel::R))
		enable |= D3D12_COLOR_WRITE_ENABLE_RED;
	if (Engine::HasFlag(colorChannel, Engine::EColorChannel::G))
		enable |= D3D12_COLOR_WRITE_ENABLE_GREEN;
	if (Engine::HasFlag(colorChannel, Engine::EColorChannel::B))
		enable |= D3D12_COLOR_WRITE_ENABLE_BLUE;
	if (Engine::HasFlag(colorChannel, Engine::EColorChannel::A))
		enable |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
	return static_cast<D3D12_COLOR_WRITE_ENABLE>(enable);
}

constexpr D3D12_RESOURCE_DIMENSION ToD3D12ResourceDimension(Engine::ETextureDimension dimension)
{
	switch (dimension)
	{
	case Engine::ETextureDimension::Texture1D: return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
	case Engine::ETextureDimension::Texture2D: return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	case Engine::ETextureDimension::Texture3D: return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	case Engine::ETextureDimension::Texture2DArray: return D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2D Array is treated as 2D in D3D12
	case Engine::ETextureDimension::TextureCube: return D3D12_RESOURCE_DIMENSION_TEXTURE2D; // Cube maps are treated as 2D in D3D12
	case Engine::ETextureDimension::TextureCubeArray: return D3D12_RESOURCE_DIMENSION_TEXTURE2D; // Cube Array maps are treated as 2D in D3D12
	default: return D3D12_RESOURCE_DIMENSION_UNKNOWN; // Default to UNKNOWN if unknown
	}
}

constexpr D3D12_SRV_DIMENSION ToD3D12SRVDimension(Engine::ETextureDimension dimension)
{
	switch (dimension)
	{
	case Engine::ETextureDimension::Texture1D: return D3D12_SRV_DIMENSION_TEXTURE1D;
	case Engine::ETextureDimension::Texture2D: return D3D12_SRV_DIMENSION_TEXTURE2D;
	case Engine::ETextureDimension::Texture2DArray: return D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	case Engine::ETextureDimension::Texture3D: return D3D12_SRV_DIMENSION_TEXTURE3D;
	case Engine::ETextureDimension::TextureCube: return D3D12_SRV_DIMENSION_TEXTURECUBE;
	case Engine::ETextureDimension::TextureCubeArray: return D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
	default: return D3D12_SRV_DIMENSION_UNKNOWN; // Default to UNKNOWN if unknown
	}
}

constexpr D3D12_RESOURCE_FLAGS ToD3D12ResourceFlags(Engine::ETextureUsage usage)
{
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
	if (Engine::HasFlag(usage, Engine::ETextureUsage::RenderTarget))
	{
		flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}
	if (Engine::HasFlag(usage, Engine::ETextureUsage::DepthStencilTarget))
	{
		flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}
	if (Engine::HasFlag(usage, Engine::ETextureUsage::ComputeReadWrite))
	{
		flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	return flags;
}

constexpr D3D12_FILTER ToD3D12Filter(Engine::ESamplerFilter min, Engine::ESamplerFilter mag, Engine::ESamplerFilter mip)
{
	// 하나라도 Anisotropic이면 전체를 Anisotropic으로 덮어씌움
	if (min == Engine::ESamplerFilter::Anisotropic || mag == Engine::ESamplerFilter::Anisotropic || mip == Engine::ESamplerFilter::Anisotropic)
		return D3D12_FILTER_ANISOTROPIC;
	if (min == Engine::ESamplerFilter::Point && mag == Engine::ESamplerFilter::Point && mip == Engine::ESamplerFilter::Point) return D3D12_FILTER_MIN_MAG_MIP_POINT;
	if (min == Engine::ESamplerFilter::Point && mag == Engine::ESamplerFilter::Point && mip == Engine::ESamplerFilter::Linear) return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
	if (min == Engine::ESamplerFilter::Point && mag == Engine::ESamplerFilter::Linear && mip == Engine::ESamplerFilter::Point) return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
	if (min == Engine::ESamplerFilter::Point && mag == Engine::ESamplerFilter::Linear && mip == Engine::ESamplerFilter::Linear) return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
	if (min == Engine::ESamplerFilter::Linear && mag == Engine::ESamplerFilter::Point && mip == Engine::ESamplerFilter::Point) return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
	if (min == Engine::ESamplerFilter::Linear && mag == Engine::ESamplerFilter::Point && mip == Engine::ESamplerFilter::Linear) return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
	if (min == Engine::ESamplerFilter::Linear && mag == Engine::ESamplerFilter::Linear && mip == Engine::ESamplerFilter::Point) return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	if (min == Engine::ESamplerFilter::Linear && mag == Engine::ESamplerFilter::Linear && mip == Engine::ESamplerFilter::Linear) return D3D12_FILTER_MIN_MAG_MIP_LINEAR;

	return D3D12_FILTER_MIN_MAG_MIP_POINT;
}

constexpr D3D12_TEXTURE_ADDRESS_MODE ToD3D12AddressMode(Engine::ESamplerAddressMode mode)
{
	switch (mode)
	{
	case Engine::ESamplerAddressMode::Wrap: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	case Engine::ESamplerAddressMode::Mirror: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	case Engine::ESamplerAddressMode::Clamp: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	case Engine::ESamplerAddressMode::Border: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	case Engine::ESamplerAddressMode::MirrorOnce: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
	default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
}

constexpr DXGI_FORMAT ToDXGIFormat(Engine::EVertexElementFormat format)
{
	switch (format)
	{
	case Engine::EVertexElementFormat::Float: return DXGI_FORMAT_R32_FLOAT;
	case Engine::EVertexElementFormat::Float2: return DXGI_FORMAT_R32G32_FLOAT;
	case Engine::EVertexElementFormat::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
	case Engine::EVertexElementFormat::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case Engine::EVertexElementFormat::Int: return DXGI_FORMAT_R32_SINT;
	case Engine::EVertexElementFormat::Int2: return DXGI_FORMAT_R32G32_SINT;
	case Engine::EVertexElementFormat::Int3: return DXGI_FORMAT_R32G32B32_SINT;
	case Engine::EVertexElementFormat::Int4: return DXGI_FORMAT_R32G32B32A32_SINT;

	case Engine::EVertexElementFormat::UInt: return DXGI_FORMAT_R32_UINT;
	case Engine::EVertexElementFormat::UInt2: return DXGI_FORMAT_R32G32_UINT;
	case Engine::EVertexElementFormat::UInt3: return DXGI_FORMAT_R32G32B32_UINT;
	case Engine::EVertexElementFormat::UInt4: return DXGI_FORMAT_R32G32B32A32_UINT;

	case Engine::EVertexElementFormat::Byte4: return DXGI_FORMAT_R8G8B8A8_SINT;
	case Engine::EVertexElementFormat::UByte4: return DXGI_FORMAT_R8G8B8A8_UINT;

	case Engine::EVertexElementFormat::Byte4_Norm: return DXGI_FORMAT_R8G8B8A8_SNORM;
	case Engine::EVertexElementFormat::UByte4_Norm: return DXGI_FORMAT_R8G8B8A8_UNORM;

	default: return DXGI_FORMAT_UNKNOWN; // Default to UNKNOWN if unknown
	}
}

constexpr D3D12_INPUT_CLASSIFICATION ToD3D12InputClassification(Engine::EVertexInputRate inputRate)
{
	switch (inputRate)
	{
	case Engine::EVertexInputRate::PerVertex: return D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	case Engine::EVertexInputRate::PerInstance: return D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
	default: return D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA; // Default to PER_VERTEX if unknown
	}
}

constexpr D3D12_STENCIL_OP ToD3D12StencilOp(Engine::EStencilOp stencilOp)
{
	switch (stencilOp)
	{
	case Engine::EStencilOp::Keep: return D3D12_STENCIL_OP_KEEP;
	case Engine::EStencilOp::Zero: return D3D12_STENCIL_OP_ZERO;
	case Engine::EStencilOp::Replace: return D3D12_STENCIL_OP_REPLACE;
	case Engine::EStencilOp::IncrementClamp: return D3D12_STENCIL_OP_INCR_SAT;
	case Engine::EStencilOp::DecrementClamp: return D3D12_STENCIL_OP_DECR_SAT;
	case Engine::EStencilOp::Invert: return D3D12_STENCIL_OP_INVERT;
	case Engine::EStencilOp::IncrementWrap: return D3D12_STENCIL_OP_INCR;
	case Engine::EStencilOp::DecrementWrap: return D3D12_STENCIL_OP_DECR;
	default: return D3D12_STENCIL_OP_KEEP; // Default to KEEP if unknown
	}
}