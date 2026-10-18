#ifndef BAM_SHADING_FLAGS_HLSLI
#define BAM_SHADING_FLAGS_HLSLI

static const uint SHADING_MODEL_MASK        = 0x0Fu;
static const uint SHADING_MODEL_DEFAULT_LIT = 0u;
static const uint SHADING_MODEL_UNLIT       = 1u;
static const uint SHADING_MODEL_TOON        = 2u;
static const uint SHADING_MODEL_SUBSURFACE  = 3u;

static const uint FLAG_RECEIVE_SHADOWS      = 1u << 4u; // 0x10 (그림자 수신)
static const uint FLAG_RECEIVE_DECALS       = 1u << 5u; // 0x20 (데칼 투영 수신)
static const uint FLAG_APPLY_SSAO           = 1u << 6u; // 0x40 (SSAO 적용)
static const uint FLAG_OUTLINE              = 1u << 7u; // 0x80 (외곽선 마스크)

float PackShadingData(uint shadingModel, uint flags)
{
	return (float)((shadingModel & SHADING_MODEL_MASK) | (flags & ~SHADING_MODEL_MASK));
}

uint UnpackShadingData(float packedFloat)
{
	return (uint)(packedFloat + 0.5f);
}

#endif