#pragma once
#include "EnumBit.h"

namespace Engine
{
#define MAX_RENDER_TARGET_COUNT 8
#define MAX_TEXTURE_SLOTS 128

	using RenderPassID = uint32;
	constexpr RenderPassID INVALID_PASS_ID = 0xFFFFFFFF;

	enum class EVertexElementFormat
	{
		Invalid,

		/* 32-bit Signed Integers */
		Int,
		Int2,
		Int3,
		Int4,

		/* 32-bit Unsigned Integers */
		UInt,
		UInt2,
		UInt3,
		UInt4,

		/* 32-bit Floats (가장 많이 사용됨) */
		Float,
		Float2,
		Float3,
		Float4,

		/* 8-bit Signed Integers */
		Byte2,
		Byte4,

		/* 8-bit Unsigned Integers */
		UByte2,
		UByte4,

		/* 8-bit Signed Normalized (정규화: -1.0 ~ 1.0) */
		Byte2_Norm,
		Byte4_Norm,

		/* 8-bit Unsigned Normalized (정규화: 0.0 ~ 1.0, 텍스처/컬러 압축에 자주 쓰임) */
		UByte2_Norm,
		UByte4_Norm,

		/* 16-bit Signed Integers */
		Short2,
		Short4,

		/* 16-bit Unsigned Integers */
		UShort2,
		UShort4,

		/* 16-bit Signed Normalized */
		Short2_Norm,
		Short4_Norm,

		/* 16-bit Unsigned Normalized */
		UShort2_Norm,
		UShort4_Norm,

		/* 16-bit Floats (Half Precision) */
		Half2,
		Half4,
	};

	struct tagVertexElementDesc
	{
		uint32 Location = { 0 };
		EVertexElementFormat Format = EVertexElementFormat::Float3;
		uint32 Offset = { 0 };

		bool operator==(const tagVertexElementDesc& other) const
		{
			return Location == other.Location &&
				Format == other.Format &&
				Offset == other.Offset;
		}
	};

	struct tagInputLayoutDesc
	{
		vector<tagVertexElementDesc> Elements;
		uint32 Stride = { 0 };

		bool operator==(const tagInputLayoutDesc& other) const
		{
			return Stride == other.Stride &&
				Elements == other.Elements;
		}
	};

	enum class ERenderSortType
	{
		None = 0,
		FrontToBack = 1,
		BackToFront = 2,
	};

	enum class ETextureFormat : uint8
	{
		UNKNOWN = 0,
		/* Unsigned Normalized Float Color Formats */
		A8_UNORM,
		R8_UNORM,
		R8G8_UNORM,
		R8G8B8A8_UNORM,
		R16_UNORM,
		R16G16_UNORM,
		R16G16B16A16_UNORM,
		R10G10B10A2_UNORM,
		B5G6R5_UNORM,
		B5G5R5A1_UNORM,
		B4G4R4A4_UNORM,
		B8G8R8A8_UNORM,
		/* Compressed Unsigned Normalized Float Color Formats */
		BC1_RGBA_UNORM,
		BC2_RGBA_UNORM,
		BC3_RGBA_UNORM,
		BC4_R_UNORM,
		BC5_RG_UNORM,
		BC7_RGBA_UNORM,
		/* Compressed Signed Float Color Formats */
		BC6H_RGB_FLOAT,
		/* Compressed Unsigned Float Color Formats */
		BC6H_RGB_UFLOAT,
		/* Signed Normalized Float Color Formats  */
		R8_SNORM,
		R8G8_SNORM,
		R8G8B8A8_SNORM,
		R16_SNORM,
		R16G16_SNORM,
		R16G16B16A16_SNORM,
		/* Signed Float Color Formats */
		R16_FLOAT,
		R16G16_FLOAT,
		R16G16B16A16_FLOAT,
		R32_FLOAT,
		R32G32_FLOAT,
		R32G32B32A32_FLOAT,
		/* Unsigned Float Color Formats */
		R11G11B10_UFLOAT,
		/* Unsigned Integer Color Formats */
		R8_UINT,
		R8G8_UINT,
		R8G8B8A8_UINT,
		R16_UINT,
		R16G16_UINT,
		R16G16B16A16_UINT,
		R32_UINT,
		R32G32_UINT,
		R32G32B32A32_UINT,
		/* Signed Integer Color Formats */
		R8_INT,
		R8G8_INT,
		R8G8B8A8_INT,
		R16_INT,
		R16G16_INT,
		R16G16B16A16_INT,
		R32_INT,
		R32G32_INT,
		R32G32B32A32_INT,
		/* SRGB Unsigned Normalized Color Formats */
		R8G8B8A8_UNORM_SRGB,
		B8G8R8A8_UNORM_SRGB,
		/* Compressed SRGB Unsigned Normalized Color Formats */
		BC1_RGBA_UNORM_SRGB,
		BC2_RGBA_UNORM_SRGB,
		BC3_RGBA_UNORM_SRGB,
		BC7_RGBA_UNORM_SRGB,
		/* Depth Formats */
		D16_UNORM,
		D24_UNORM,
		D32_FLOAT,
		D24_UNORM_S8_UINT,
		D32_FLOAT_S8_UINT,
		/* Compressed ASTC Normalized Float Color Formats*/
		ASTC_4x4_UNORM,
		ASTC_5x4_UNORM,
		ASTC_5x5_UNORM,
		ASTC_6x5_UNORM,
		ASTC_6x6_UNORM,
		ASTC_8x5_UNORM,
		ASTC_8x6_UNORM,
		ASTC_8x8_UNORM,
		ASTC_10x5_UNORM,
		ASTC_10x6_UNORM,
		ASTC_10x8_UNORM,
		ASTC_10x10_UNORM,
		ASTC_12x10_UNORM,
		ASTC_12x12_UNORM,
		/* Compressed SRGB ASTC Normalized Float Color Formats*/
		ASTC_4x4_UNORM_SRGB,
		ASTC_5x4_UNORM_SRGB,
		ASTC_5x5_UNORM_SRGB,
		ASTC_6x5_UNORM_SRGB,
		ASTC_6x6_UNORM_SRGB,
		ASTC_8x5_UNORM_SRGB,
		ASTC_8x6_UNORM_SRGB,
		ASTC_8x8_UNORM_SRGB,
		ASTC_10x5_UNORM_SRGB,
		ASTC_10x6_UNORM_SRGB,
		ASTC_10x8_UNORM_SRGB,
		ASTC_10x10_UNORM_SRGB,
		ASTC_12x10_UNORM_SRGB,
		ASTC_12x12_UNORM_SRGB,
		/* Compressed ASTC Signed Float Color Formats*/
		ASTC_4x4_FLOAT,
		ASTC_5x4_FLOAT,
		ASTC_5x5_FLOAT,
		ASTC_6x5_FLOAT,
		ASTC_6x6_FLOAT,
		ASTC_8x5_FLOAT,
		ASTC_8x6_FLOAT,
		ASTC_8x8_FLOAT,
		ASTC_10x5_FLOAT,
		ASTC_10x6_FLOAT,
		ASTC_10x8_FLOAT,
		ASTC_10x10_FLOAT,
		ASTC_12x10_FLOAT,
		ASTC_12x12_FLOAT
	};

	enum class ETextureUsage : uint8
	{
		None				= 0,
		Sampler				= 1 << 0,
		RenderTarget		= 1 << 1,
		DepthStencilTarget	= 1 << 2,
		ComputeReadWrite	= 1 << 3,
	};
	ENABLE_BITMASK_OPERATORS(ETextureUsage)

	enum class ERenderTargetBindFlag : uint8
	{
		RTBF_None = 0,
		RTBF_ShaderResource = 1 << 0,
		RTBF_RenderTarget = 1 << 1,
		RTBF_DepthStencil = 1 << 2,
	};

	ENABLE_BITMASK_OPERATORS(ERenderTargetBindFlag)

	enum class ERenderTargetClearFlag : uint8
	{
		RTCF_None = 0,
		RTCF_Color = 1 << 0,
		RTCF_Depth = 1 << 1,
		RTCF_Stencil = 1 << 2,
	};

	
	enum class ERenderTargetType : uint8
	{
		Color,
		GBuffer,
		DepthStencil,
	};

	enum class ETextureDimension : uint8
	{
		Texture1D,
		Texture2D,
		Texture2DArray,
		Texture3D,
		TextureCube,
		TextureCubeArray,
	};

	enum class ETextureSampleCount : uint8
	{
		TextureSampleCount1 = 1,
		TextureSampleCount2 = 2,
		TextureSampleCount4 = 4,
		TextureSampleCount8 = 8,
	};

	enum class ERenderPassLoadOperation : uint8
	{
		RPLO_Load,
		RPLO_Clear,
		RPLO_Discard,
	};

	enum class ERenderPassStoreOperation : uint8
	{
		RPSO_Store,
		RPSO_Discard,
		RPSO_Resolve,
		RPSO_ResolveAndStore,
	};

	struct tagRenderPassDesc
	{
		uint32 Priority = { 0 };
		RenderPassID ID = INVALID_PASS_ID;
		wstring Name = { L"" };
		wstring DepthStencilName = { L"" };
		vector<wstring> RenderTargetNames;
		ERenderSortType SortType = ERenderSortType::None;
		ERenderPassLoadOperation LoadOperation = ERenderPassLoadOperation::RPLO_Load;
		ERenderPassStoreOperation StoreOperation = ERenderPassStoreOperation::RPSO_Store;
		ERenderPassLoadOperation StencilLoadOperation = ERenderPassLoadOperation::RPLO_Load;
		ERenderPassStoreOperation StencilStoreOperation = ERenderPassStoreOperation::RPSO_Store;
		vec4 OverrideClearColor = vec4(0.0f, 0.0f, 0.0f, -1.0f);
	};
}