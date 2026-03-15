#pragma once

#include "Engine_Includes.h"
#include "RenderTypes.h"
#include <SDL3/SDL_GPU.h>

static const SDL_GPUTextureFormat SDL_GPUTextureFormats[] = {
    SDL_GPU_TEXTUREFORMAT_INVALID,

    /* Unsigned Normalized Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_A8_UNORM,
    SDL_GPU_TEXTUREFORMAT_R8_UNORM,
    SDL_GPU_TEXTUREFORMAT_R8G8_UNORM,
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    SDL_GPU_TEXTUREFORMAT_R16_UNORM,
    SDL_GPU_TEXTUREFORMAT_R16G16_UNORM,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM,
    SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM,
    SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM,
    SDL_GPU_TEXTUREFORMAT_B5G5R5A1_UNORM,
    SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM,
    SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
    /* Compressed Unsigned Normalized Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM,
    SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM,
    SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM,
    SDL_GPU_TEXTUREFORMAT_BC4_R_UNORM,
    SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM,
    SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM,
    /* Compressed Signed Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_BC6H_RGB_FLOAT,
    /* Compressed Unsigned Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_BC6H_RGB_UFLOAT,
    /* Signed Normalized Float Color Formats  */
    SDL_GPU_TEXTUREFORMAT_R8_SNORM,
    SDL_GPU_TEXTUREFORMAT_R8G8_SNORM,
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM,
    SDL_GPU_TEXTUREFORMAT_R16_SNORM,
    SDL_GPU_TEXTUREFORMAT_R16G16_SNORM,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SNORM,
    /* Signed Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_R16_FLOAT,
    SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
    SDL_GPU_TEXTUREFORMAT_R32_FLOAT,
    SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT,
    SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
    /* Unsigned Float Color Formats */
    SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,
    /* Unsigned Integer Color Formats */
    SDL_GPU_TEXTUREFORMAT_R8_UINT,
    SDL_GPU_TEXTUREFORMAT_R8G8_UINT,
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT,
    SDL_GPU_TEXTUREFORMAT_R16_UINT,
    SDL_GPU_TEXTUREFORMAT_R16G16_UINT,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT,
    SDL_GPU_TEXTUREFORMAT_R32_UINT,
    SDL_GPU_TEXTUREFORMAT_R32G32_UINT,
    SDL_GPU_TEXTUREFORMAT_R32G32B32A32_UINT,
    /* Signed Integer Color Formats */
    SDL_GPU_TEXTUREFORMAT_R8_INT,
    SDL_GPU_TEXTUREFORMAT_R8G8_INT,
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_INT,
    SDL_GPU_TEXTUREFORMAT_R16_INT,
    SDL_GPU_TEXTUREFORMAT_R16G16_INT,
    SDL_GPU_TEXTUREFORMAT_R16G16B16A16_INT,
    SDL_GPU_TEXTUREFORMAT_R32_INT,
    SDL_GPU_TEXTUREFORMAT_R32G32_INT,
    SDL_GPU_TEXTUREFORMAT_R32G32B32A32_INT,
    /* SRGB Unsigned Normalized Color Formats */
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB,
    /* Compressed SRGB Unsigned Normalized Color Formats */
    SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB,
    /* Depth Formats */
    SDL_GPU_TEXTUREFORMAT_D16_UNORM,
    SDL_GPU_TEXTUREFORMAT_D24_UNORM,
    SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
    SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
    /* Compressed ASTC Normalized Float Color Formats*/
    SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM,
    SDL_GPU_TEXTUREFORMAT_ASTC_5x4_UNORM,
    SDL_GPU_TEXTUREFORMAT_ASTC_5x5_UNORM,
    SDL_GPU_TEXTUREFORMAT_ASTC_6x5_UNORM,
    SDL_GPU_TEXTUREFORMAT_ASTC_6x6_UNORM,
    SDL_GPU_TEXTUREFORMAT_ASTC_8x5_UNORM,
    SDL_GPU_TEXTUREFORMAT_ASTC_8x6_UNORM,
    SDL_GPU_TEXTUREFORMAT_ASTC_8x8_UNORM,
    SDL_GPU_TEXTUREFORMAT_ASTC_10x5_UNORM,
    SDL_GPU_TEXTUREFORMAT_ASTC_10x6_UNORM,
    SDL_GPU_TEXTUREFORMAT_ASTC_10x8_UNORM,
    SDL_GPU_TEXTUREFORMAT_ASTC_10x10_UNORM,
    SDL_GPU_TEXTUREFORMAT_ASTC_12x10_UNORM,
    SDL_GPU_TEXTUREFORMAT_ASTC_12x12_UNORM,
    /* Compressed SRGB ASTC Normalized Float Color Formats*/
    SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_ASTC_5x4_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_ASTC_5x5_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_ASTC_6x5_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_ASTC_6x6_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_ASTC_8x5_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_ASTC_8x6_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_ASTC_8x8_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_ASTC_10x5_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_ASTC_10x6_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_ASTC_10x8_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_ASTC_10x10_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_ASTC_12x10_UNORM_SRGB,
    SDL_GPU_TEXTUREFORMAT_ASTC_12x12_UNORM_SRGB,
    /* Compressed ASTC Signed Float Color Formats*/
    SDL_GPU_TEXTUREFORMAT_ASTC_4x4_FLOAT,
    SDL_GPU_TEXTUREFORMAT_ASTC_5x4_FLOAT,
    SDL_GPU_TEXTUREFORMAT_ASTC_5x5_FLOAT,
    SDL_GPU_TEXTUREFORMAT_ASTC_6x5_FLOAT,
    SDL_GPU_TEXTUREFORMAT_ASTC_6x6_FLOAT,
    SDL_GPU_TEXTUREFORMAT_ASTC_8x5_FLOAT,
    SDL_GPU_TEXTUREFORMAT_ASTC_8x6_FLOAT,
    SDL_GPU_TEXTUREFORMAT_ASTC_8x8_FLOAT,
    SDL_GPU_TEXTUREFORMAT_ASTC_10x5_FLOAT,
    SDL_GPU_TEXTUREFORMAT_ASTC_10x6_FLOAT,
    SDL_GPU_TEXTUREFORMAT_ASTC_10x8_FLOAT,
    SDL_GPU_TEXTUREFORMAT_ASTC_10x10_FLOAT,
    SDL_GPU_TEXTUREFORMAT_ASTC_12x10_FLOAT,
    SDL_GPU_TEXTUREFORMAT_ASTC_12x12_FLOAT
};

static const SDL_GPUTextureType SDL_GPUTextureTypes[] = {
    SDL_GPU_TEXTURETYPE_2D,
    SDL_GPU_TEXTURETYPE_CUBE,
    SDL_GPU_TEXTURETYPE_3D,
    SDL_GPU_TEXTURETYPE_2D_ARRAY,
    SDL_GPU_TEXTURETYPE_CUBE_ARRAY
};

static const SDL_GPULoadOp SDL_GPURenderPassLoadOperations[] = {
    SDL_GPU_LOADOP_LOAD,
    SDL_GPU_LOADOP_CLEAR,
    SDL_GPU_LOADOP_DONT_CARE
};

static const SDL_GPUStoreOp SDL_GPURenderPassStoreOperations[] = {
    SDL_GPU_STOREOP_STORE,
    SDL_GPU_STOREOP_DONT_CARE,
    SDL_GPU_STOREOP_RESOLVE,
    SDL_GPU_STOREOP_RESOLVE_AND_STORE
};

#pragma region Helper

#pragma region Pipeline
constexpr SDL_GPUCullMode ToSDLCullMode(ECullMode mode) {
    switch (mode) {
    case ECullMode::None: return SDL_GPU_CULLMODE_NONE;
    case ECullMode::Front: return SDL_GPU_CULLMODE_FRONT;
    case ECullMode::Back: return SDL_GPU_CULLMODE_BACK;
    default: return SDL_GPU_CULLMODE_NONE;
    }
}

constexpr SDL_GPUFillMode ToSDLFillMode(EFillMode mode) {
    return (mode == EFillMode::Wireframe) ? SDL_GPU_FILLMODE_LINE : SDL_GPU_FILLMODE_FILL;
}

constexpr SDL_GPUFrontFace ToSDLFrontFace(EFrontFace face) {
    return (face == EFrontFace::CounterClockwise) ? SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE : SDL_GPU_FRONTFACE_CLOCKWISE;
}

constexpr SDL_GPUCompareOp ToSDLCompareOp(ECompareOp op) {
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

constexpr SDL_GPUPrimitiveType ToSDLTopology(ETopology topology) {
    switch (topology) {
    case ETopology::TriangleList: return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    case ETopology::TriangleStrip: return SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
    case ETopology::LineList: return SDL_GPU_PRIMITIVETYPE_LINELIST;
    case ETopology::LineStrip: return SDL_GPU_PRIMITIVETYPE_LINESTRIP;
    case ETopology::PointList: return SDL_GPU_PRIMITIVETYPE_POINTLIST;
    default: return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    }
}

#pragma endregion

#pragma region SDLGPUTexture
constexpr SDL_GPUTextureType ToSDLGPUTextureType(Engine::ETextureDimension dimension)
{
    switch (dimension)
    {
    case Engine::ETextureDimension::Texture2D: return SDL_GPU_TEXTURETYPE_2D;
    case Engine::ETextureDimension::TextureCube: return SDL_GPU_TEXTURETYPE_CUBE;
    case Engine::ETextureDimension::Texture3D: return SDL_GPU_TEXTURETYPE_3D;
    case Engine::ETextureDimension::Texture2DArray: return SDL_GPU_TEXTURETYPE_2D_ARRAY;
    case Engine::ETextureDimension::TextureCubeArray: return SDL_GPU_TEXTURETYPE_CUBE_ARRAY;
    default: return SDL_GPU_TEXTURETYPE_2D;
    }
}

constexpr SDL_GPUTextureFormat ToSDLGPUTextureFormat(Engine::ETextureFormat format)
{
    using namespace Engine; // 타이핑을 줄이고 가독성을 높이기 위해 namespace 사용

    switch (format)
    {
        /* Unsigned Normalized Float Color Formats */
    case ETextureFormat::A8_UNORM:                  return SDL_GPU_TEXTUREFORMAT_A8_UNORM;
    case ETextureFormat::R8_UNORM:                  return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    case ETextureFormat::R8G8_UNORM:                return SDL_GPU_TEXTUREFORMAT_R8G8_UNORM;
    case ETextureFormat::R8G8B8A8_UNORM:            return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    case ETextureFormat::R16_UNORM:                 return SDL_GPU_TEXTUREFORMAT_R16_UNORM;
    case ETextureFormat::R16G16_UNORM:              return SDL_GPU_TEXTUREFORMAT_R16G16_UNORM;
    case ETextureFormat::R16G16B16A16_UNORM:        return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM;
    case ETextureFormat::R10G10B10A2_UNORM:         return SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM;
    case ETextureFormat::B5G6R5_UNORM:              return SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM;
    case ETextureFormat::B5G5R5A1_UNORM:            return SDL_GPU_TEXTUREFORMAT_B5G5R5A1_UNORM;
    case ETextureFormat::B4G4R4A4_UNORM:            return SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM;
    case ETextureFormat::B8G8R8A8_UNORM:            return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;

        /* Compressed Unsigned Normalized Float Color Formats */
    case ETextureFormat::BC1_RGBA_UNORM:            return SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM;
    case ETextureFormat::BC2_RGBA_UNORM:            return SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM;
    case ETextureFormat::BC3_RGBA_UNORM:            return SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM;
    case ETextureFormat::BC4_R_UNORM:               return SDL_GPU_TEXTUREFORMAT_BC4_R_UNORM;
    case ETextureFormat::BC5_RG_UNORM:              return SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM;
    case ETextureFormat::BC7_RGBA_UNORM:            return SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM;

        /* Compressed Signed Float Color Formats */
    case ETextureFormat::BC6H_RGB_FLOAT:            return SDL_GPU_TEXTUREFORMAT_BC6H_RGB_FLOAT;

        /* Compressed Unsigned Float Color Formats */
    case ETextureFormat::BC6H_RGB_UFLOAT:           return SDL_GPU_TEXTUREFORMAT_BC6H_RGB_UFLOAT;

        /* Signed Normalized Float Color Formats  */
    case ETextureFormat::R8_SNORM:                  return SDL_GPU_TEXTUREFORMAT_R8_SNORM;
    case ETextureFormat::R8G8_SNORM:                return SDL_GPU_TEXTUREFORMAT_R8G8_SNORM;
    case ETextureFormat::R8G8B8A8_SNORM:            return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM;
    case ETextureFormat::R16_SNORM:                 return SDL_GPU_TEXTUREFORMAT_R16_SNORM;
    case ETextureFormat::R16G16_SNORM:              return SDL_GPU_TEXTUREFORMAT_R16G16_SNORM;
    case ETextureFormat::R16G16B16A16_SNORM:        return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SNORM;

        /* Signed Float Color Formats */
    case ETextureFormat::R16_FLOAT:                 return SDL_GPU_TEXTUREFORMAT_R16_FLOAT;
    case ETextureFormat::R16G16_FLOAT:              return SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT;
    case ETextureFormat::R16G16B16A16_FLOAT:        return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    case ETextureFormat::R32_FLOAT:                 return SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    case ETextureFormat::R32G32_FLOAT:              return SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT;
    case ETextureFormat::R32G32B32A32_FLOAT:        return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;

        /* Unsigned Float Color Formats */
    case ETextureFormat::R11G11B10_UFLOAT:          return SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT;

        /* Unsigned Integer Color Formats */
    case ETextureFormat::R8_UINT:                   return SDL_GPU_TEXTUREFORMAT_R8_UINT;
    case ETextureFormat::R8G8_UINT:                 return SDL_GPU_TEXTUREFORMAT_R8G8_UINT;
    case ETextureFormat::R8G8B8A8_UINT:             return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT;
    case ETextureFormat::R16_UINT:                  return SDL_GPU_TEXTUREFORMAT_R16_UINT;
    case ETextureFormat::R16G16_UINT:               return SDL_GPU_TEXTUREFORMAT_R16G16_UINT;
    case ETextureFormat::R16G16B16A16_UINT:         return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT;
    case ETextureFormat::R32_UINT:                  return SDL_GPU_TEXTUREFORMAT_R32_UINT;
    case ETextureFormat::R32G32_UINT:               return SDL_GPU_TEXTUREFORMAT_R32G32_UINT;
    case ETextureFormat::R32G32B32A32_UINT:         return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_UINT;

        /* Signed Integer Color Formats */
    case ETextureFormat::R8_INT:                    return SDL_GPU_TEXTUREFORMAT_R8_INT;
    case ETextureFormat::R8G8_INT:                  return SDL_GPU_TEXTUREFORMAT_R8G8_INT;
    case ETextureFormat::R8G8B8A8_INT:              return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_INT;
    case ETextureFormat::R16_INT:                   return SDL_GPU_TEXTUREFORMAT_R16_INT;
    case ETextureFormat::R16G16_INT:                return SDL_GPU_TEXTUREFORMAT_R16G16_INT;
    case ETextureFormat::R16G16B16A16_INT:          return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_INT;
    case ETextureFormat::R32_INT:                   return SDL_GPU_TEXTUREFORMAT_R32_INT;
    case ETextureFormat::R32G32_INT:                return SDL_GPU_TEXTUREFORMAT_R32G32_INT;
    case ETextureFormat::R32G32B32A32_INT:          return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_INT;

        /* SRGB Unsigned Normalized Color Formats */
    case ETextureFormat::R8G8B8A8_UNORM_SRGB:       return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    case ETextureFormat::B8G8R8A8_UNORM_SRGB:       return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;

        /* Compressed SRGB Unsigned Normalized Color Formats */
    case ETextureFormat::BC1_RGBA_UNORM_SRGB:       return SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM_SRGB;
    case ETextureFormat::BC2_RGBA_UNORM_SRGB:       return SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM_SRGB;
    case ETextureFormat::BC3_RGBA_UNORM_SRGB:       return SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM_SRGB;
    case ETextureFormat::BC7_RGBA_UNORM_SRGB:       return SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB;

        /* Depth Formats */
    case ETextureFormat::D16_UNORM:                 return SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    case ETextureFormat::D24_UNORM:                 return SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    case ETextureFormat::D32_FLOAT:                 return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    case ETextureFormat::D24_UNORM_S8_UINT:         return SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    case ETextureFormat::D32_FLOAT_S8_UINT:         return SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;

        /* Compressed ASTC Normalized Float Color Formats*/
    case ETextureFormat::ASTC_4x4_UNORM:            return SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM;
    case ETextureFormat::ASTC_5x4_UNORM:            return SDL_GPU_TEXTUREFORMAT_ASTC_5x4_UNORM;
    case ETextureFormat::ASTC_5x5_UNORM:            return SDL_GPU_TEXTUREFORMAT_ASTC_5x5_UNORM;
    case ETextureFormat::ASTC_6x5_UNORM:            return SDL_GPU_TEXTUREFORMAT_ASTC_6x5_UNORM;
    case ETextureFormat::ASTC_6x6_UNORM:            return SDL_GPU_TEXTUREFORMAT_ASTC_6x6_UNORM;
    case ETextureFormat::ASTC_8x5_UNORM:            return SDL_GPU_TEXTUREFORMAT_ASTC_8x5_UNORM;
    case ETextureFormat::ASTC_8x6_UNORM:            return SDL_GPU_TEXTUREFORMAT_ASTC_8x6_UNORM;
    case ETextureFormat::ASTC_8x8_UNORM:            return SDL_GPU_TEXTUREFORMAT_ASTC_8x8_UNORM;
    case ETextureFormat::ASTC_10x5_UNORM:           return SDL_GPU_TEXTUREFORMAT_ASTC_10x5_UNORM;
    case ETextureFormat::ASTC_10x6_UNORM:           return SDL_GPU_TEXTUREFORMAT_ASTC_10x6_UNORM;
    case ETextureFormat::ASTC_10x8_UNORM:           return SDL_GPU_TEXTUREFORMAT_ASTC_10x8_UNORM;
    case ETextureFormat::ASTC_10x10_UNORM:          return SDL_GPU_TEXTUREFORMAT_ASTC_10x10_UNORM;
    case ETextureFormat::ASTC_12x10_UNORM:          return SDL_GPU_TEXTUREFORMAT_ASTC_12x10_UNORM;
    case ETextureFormat::ASTC_12x12_UNORM:          return SDL_GPU_TEXTUREFORMAT_ASTC_12x12_UNORM;

        /* Compressed SRGB ASTC Normalized Float Color Formats*/
    case ETextureFormat::ASTC_4x4_UNORM_SRGB:       return SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM_SRGB;
    case ETextureFormat::ASTC_5x4_UNORM_SRGB:       return SDL_GPU_TEXTUREFORMAT_ASTC_5x4_UNORM_SRGB;
    case ETextureFormat::ASTC_5x5_UNORM_SRGB:       return SDL_GPU_TEXTUREFORMAT_ASTC_5x5_UNORM_SRGB;
    case ETextureFormat::ASTC_6x5_UNORM_SRGB:       return SDL_GPU_TEXTUREFORMAT_ASTC_6x5_UNORM_SRGB;
    case ETextureFormat::ASTC_6x6_UNORM_SRGB:       return SDL_GPU_TEXTUREFORMAT_ASTC_6x6_UNORM_SRGB;
    case ETextureFormat::ASTC_8x5_UNORM_SRGB:       return SDL_GPU_TEXTUREFORMAT_ASTC_8x5_UNORM_SRGB;
    case ETextureFormat::ASTC_8x6_UNORM_SRGB:       return SDL_GPU_TEXTUREFORMAT_ASTC_8x6_UNORM_SRGB;
    case ETextureFormat::ASTC_8x8_UNORM_SRGB:       return SDL_GPU_TEXTUREFORMAT_ASTC_8x8_UNORM_SRGB;
    case ETextureFormat::ASTC_10x5_UNORM_SRGB:      return SDL_GPU_TEXTUREFORMAT_ASTC_10x5_UNORM_SRGB;
    case ETextureFormat::ASTC_10x6_UNORM_SRGB:      return SDL_GPU_TEXTUREFORMAT_ASTC_10x6_UNORM_SRGB;
    case ETextureFormat::ASTC_10x8_UNORM_SRGB:      return SDL_GPU_TEXTUREFORMAT_ASTC_10x8_UNORM_SRGB;
    case ETextureFormat::ASTC_10x10_UNORM_SRGB:     return SDL_GPU_TEXTUREFORMAT_ASTC_10x10_UNORM_SRGB;
    case ETextureFormat::ASTC_12x10_UNORM_SRGB:     return SDL_GPU_TEXTUREFORMAT_ASTC_12x10_UNORM_SRGB;
    case ETextureFormat::ASTC_12x12_UNORM_SRGB:     return SDL_GPU_TEXTUREFORMAT_ASTC_12x12_UNORM_SRGB;

        /* Compressed ASTC Signed Float Color Formats*/
    case ETextureFormat::ASTC_4x4_FLOAT:            return SDL_GPU_TEXTUREFORMAT_ASTC_4x4_FLOAT;
    case ETextureFormat::ASTC_5x4_FLOAT:            return SDL_GPU_TEXTUREFORMAT_ASTC_5x4_FLOAT;
    case ETextureFormat::ASTC_5x5_FLOAT:            return SDL_GPU_TEXTUREFORMAT_ASTC_5x5_FLOAT;
    case ETextureFormat::ASTC_6x5_FLOAT:            return SDL_GPU_TEXTUREFORMAT_ASTC_6x5_FLOAT;
    case ETextureFormat::ASTC_6x6_FLOAT:            return SDL_GPU_TEXTUREFORMAT_ASTC_6x6_FLOAT;
    case ETextureFormat::ASTC_8x5_FLOAT:            return SDL_GPU_TEXTUREFORMAT_ASTC_8x5_FLOAT;
    case ETextureFormat::ASTC_8x6_FLOAT:            return SDL_GPU_TEXTUREFORMAT_ASTC_8x6_FLOAT;
    case ETextureFormat::ASTC_8x8_FLOAT:            return SDL_GPU_TEXTUREFORMAT_ASTC_8x8_FLOAT;
    case ETextureFormat::ASTC_10x5_FLOAT:           return SDL_GPU_TEXTUREFORMAT_ASTC_10x5_FLOAT;
    case ETextureFormat::ASTC_10x6_FLOAT:           return SDL_GPU_TEXTUREFORMAT_ASTC_10x6_FLOAT;
    case ETextureFormat::ASTC_10x8_FLOAT:           return SDL_GPU_TEXTUREFORMAT_ASTC_10x8_FLOAT;
    case ETextureFormat::ASTC_10x10_FLOAT:          return SDL_GPU_TEXTUREFORMAT_ASTC_10x10_FLOAT;
    case ETextureFormat::ASTC_12x10_FLOAT:          return SDL_GPU_TEXTUREFORMAT_ASTC_12x10_FLOAT;
    case ETextureFormat::ASTC_12x12_FLOAT:          return SDL_GPU_TEXTUREFORMAT_ASTC_12x12_FLOAT;

        /* 알 수 없는 포맷이나 매핑되지 않은 값 처리 */
    case ETextureFormat::UNKNOWN:
    default:
        return SDL_GPU_TEXTUREFORMAT_INVALID;
    }
}

constexpr SDL_GPUTextureUsageFlags ToSDLGPUTextureUsage(Engine::ETextureUsage usage)
{
    SDL_GPUTextureUsageFlags flags = 0;

    if (Engine::HasFlag(usage, Engine::ETextureUsage::Sampler))
    {
        flags |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
    }

    if (Engine::HasFlag(usage, Engine::ETextureUsage::RenderTarget))
    {
        flags |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    }

    if (Engine::HasFlag(usage, Engine::ETextureUsage::DepthStencilTarget))
    {
        flags |= SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    }

    if (Engine::HasFlag(usage, Engine::ETextureUsage::ComputeReadWrite))
    {
        flags |= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
    }

    return flags;
}

constexpr SDL_GPUSampleCount ToSDLGPUTextureSampleCount(Engine::ETextureSampleCount sampleCount)
{
    switch (sampleCount)
    {
    case Engine::ETextureSampleCount::TextureSampleCount1: return SDL_GPU_SAMPLECOUNT_1;
    case Engine::ETextureSampleCount::TextureSampleCount2: return SDL_GPU_SAMPLECOUNT_2;
    case Engine::ETextureSampleCount::TextureSampleCount4: return SDL_GPU_SAMPLECOUNT_4;
    case Engine::ETextureSampleCount::TextureSampleCount8: return SDL_GPU_SAMPLECOUNT_8;
    default: return SDL_GPU_SAMPLECOUNT_1;
    }
}
#pragma endregion




#pragma region Vertex
constexpr SDL_GPUVertexElementFormat ToSDLVertexFormat(EVertexElementFormat format) {
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


#pragma endregion