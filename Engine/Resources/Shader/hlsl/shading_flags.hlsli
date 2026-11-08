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

// ── 머티리얼 텍스처 유효성 플래그 (MaterialFeatureFlags) ──
static const uint FLAG_MATERIAL_NONE        = 0u;
static const uint FLAG_HAS_DIFFUSE_MAP      = 1u << 0u; // 슬롯 0: Base Color / Albedo Map
static const uint FLAG_HAS_NORMAL_MAP       = 1u << 1u; // 슬롯 1: Normal Map
static const uint FLAG_HAS_PBR_MAP          = 1u << 2u; // 슬롯 2: PBR Map (Roughness/Metallic/Cavity/AO)
static const uint FLAG_HAS_EMISSIVE_MAP     = 1u << 3u; // 슬롯 3: Emissive Map

float PackShadingData(uint shadingModel, uint flags)
{
	return (float)((shadingModel & SHADING_MODEL_MASK) | (flags & ~SHADING_MODEL_MASK));
}

uint UnpackShadingData(float packedFloat)
{
	return (uint)(packedFloat + 0.5f);
}

#endif