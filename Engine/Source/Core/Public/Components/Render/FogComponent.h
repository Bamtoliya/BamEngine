#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "Types.h"

namespace Engine
{
    ENUM(DISPLAY_NAME("ENUM_FOG_TYPE"))
    enum class EFogType : uint32
    {
        Linear = 0,             // 단순 선형 거리 안개 (Start ~ End)
        Exponential = 1,        // 지수 거리 안개 (exp(-d * Density))
        ExponentialHeight = 2   // 지수 높이 안개 (거리 + 고도 Y 감쇠, Unreal 방식)
    };

    // HLSL register(b0, space3)와 1:1로 일치하는 48바이트 GPU 구조체
    struct FogData
    {
        vec3 FogColor{ 0.7f, 0.82f, 0.95f };    f32 FogDensity = 0.02f;
        f32 FogHeight = 0.0f;                   f32 FogHeightFalloff = 0.15f;
        f32 FogStartDistance = 5.0f;            f32 FogEndDistance = 150.0f;
        uint32 FogType = 2u;                    f32 FogMaxOpacity = 1.0f;
        uint32 FogEnabled = 1u;                 float _pad = 0.0f;
    };
    static_assert(sizeof(FogData) == 48, "FogData size must be 48 bytes");

    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_FOG"), CATEGORY("CATEGORY_RENDER"))
    struct FogComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
        bool IsEnabled = true;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FOG_TYPE"))
        EFogType FogType = EFogType::ExponentialHeight;

        PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("Fog Color"), DISPLAY_NAME("PROP_FOG_COLOR"))
        vec3 FogColor{ 0.7f, 0.82f, 0.95f };

        PROPERTY(EDITABLE, CATEGORY("Fog Parameters"), DISPLAY_NAME("PROP_FOG_DENSITY"))
        f32 FogDensity = 0.02f;

        PROPERTY(EDITABLE, CATEGORY("Fog Parameters"), DISPLAY_NAME("PROP_FOG_HEIGHT_FALLOFF"))
        f32 FogHeightFalloff = 0.15f;

        PROPERTY(EDITABLE, CATEGORY("Fog Placement"), DISPLAY_NAME("PROP_FOG_HEIGHT"))
        f32 FogHeight = 0.0f;

        PROPERTY(EDITABLE, CATEGORY("Fog Placement"), DISPLAY_NAME("PROP_FOG_START_DIST"))
        f32 FogStartDistance = 5.0f;

        PROPERTY(EDITABLE, CATEGORY("Fog Placement"), DISPLAY_NAME("PROP_FOG_END_DIST"))
        f32 FogEndDistance = 150.0f;

        PROPERTY(EDITABLE, CATEGORY("Fog Placement"), DISPLAY_NAME("PROP_FOG_MAX_OPACITY"))
        f32 FogMaxOpacity = 1.0f;

        PROPERTY(EDITABLE, CATEGORY("Atmosphere Sync"), DISPLAY_NAME("PROP_AUTO_SYNC_ATMOSPHERE"))
        bool AutoSyncAtmosphere = true;
    };
}