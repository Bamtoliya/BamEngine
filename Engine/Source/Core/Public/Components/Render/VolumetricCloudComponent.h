#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "Types.h"

namespace Engine
{
    // GPU ConstantBuffer와 1:1로 매핑되는 128바이트 볼류메트릭 클라우드 데이터
    struct VolumetricCloudData
    {
        float LayerBottomAltitude = 1500.0f; // 구름 시작 고도 (미터)
        float LayerHeight = 2500.0f;         // 구름층 두께 (미터)
        float TracingMaxDistance = 50000.0f; // 최대 광선 추적 거리 (50km)
        float CloudCoverage = 0.50f;         // 구름 커버리지 (0.0 ~ 1.0)

        float CloudDensity = 1.0f;           // 구름 농도
        float WeatherScale = 0.00004f;       // 1/25000m (거시적 웨더 맵 타일링)
        float ShapeScale = 0.00067f;         // 1/1500m (3D 뭉게구름 셰이프 타일링)
        float DetailScale = 0.0067f;         // 1/150m (에로전 디테일 타일링)

        vec3 WindDirection{ 1.0f, 0.0f, 0.3f }; float WindSpeed = 8.0f; // 바람 방향 및 속도
        vec3 CloudAlbedo{ 1.0f, 1.0f, 1.0f };   float ExtinctionFactor = 1.0f;

        float MorphSpeed = 0.05f;            // 대류 피어오름 속도
        float PowderExtinction = 0.5f;       // 파우더 효과 강도
        uint32 EnableClouds = 1u;            // 활성화 여부
        float _pad0 = 0.0f;
        float _pad1[12] = { 0.0f };
    };
    static_assert(sizeof(VolumetricCloudData) == 128, "VolumetricCloudData must be 128 bytes");

    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_VOLUMETRIC_CLOUD"), CATEGORY("CATEGORY_RENDER"))
        struct VolumetricCloudComponent
    {
        REFLECT_BODY()

            PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
            bool IsEnabled = true;

        // ── 구름층 고도 및 범위 ──
        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Layer Placement"), DISPLAY_NAME("PROP_LAYER_BOTTOM"))
            f32 LayerBottomAltitude = 1500.0f; // 지표면으로부터 구름 밑면 고도 (미터)

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Layer Placement"), DISPLAY_NAME("PROP_LAYER_HEIGHT"))
            f32 LayerHeight = 2500.0f; // 구름층 두께 (미터)

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Layer Placement"), DISPLAY_NAME("PROP_MAX_DISTANCE"))
            f32 TracingMaxDistance = 50000.0f; // 최대 추적 거리 (50km)

        // ── 뭉게구름 형태 및 스케일 ──
        PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0f, 1.0f), CATEGORY("Cloud Shape"), DISPLAY_NAME("PROP_COVERAGE"))
            f32 CloudCoverage = 0.50f; // 구름 양 (0.0: 맑음 ~ 1.0: 흐림)

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Cloud Shape"), DISPLAY_NAME("PROP_DENSITY"))
            f32 CloudDensity = 1.0f; // 구름 밀도/농도

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Cloud Shape"), DISPLAY_NAME("PROP_WEATHER_SIZE"))
            f32 WeatherMapSize = 25000.0f; // 거시적 구름 군집 크기 (미터 단위, 기본 25km)

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Cloud Shape"), DISPLAY_NAME("PROP_SHAPE_SIZE"))
            f32 ShapeNoiseSize = 1500.0f; // 기본 뭉게구름 덩어리 크기 (미터 단위, 기본 1.5km)

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Cloud Shape"), DISPLAY_NAME("PROP_DETAIL_SIZE"))
            f32 DetailNoiseSize = 150.0f; // 솜털 디테일 크기 (미터 단위, 기본 150m)

        // ── 대류 및 바람 ──
        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Wind & Evolution"), DISPLAY_NAME("PROP_WIND_DIR"))
            vec3 WindDirection{ 1.0f, 0.0f, 0.3f };

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Wind & Evolution"), DISPLAY_NAME("PROP_WIND_SPEED"))
            f32 WindSpeed = 8.0f; // 바람 속도 (m/s)

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Wind & Evolution"), DISPLAY_NAME("PROP_MORPH_SPEED"))
            f32 MorphSpeed = 0.05f; // 구름이 피어오르고 흩어지는 대류 속도

        // ── 빛 및 음영 (Lighting) ──
        PROPERTY(EDITABLE, COLOR_PROPERTY, ALLOW_MISSING, CATEGORY("Lighting"), DISPLAY_NAME("PROP_ALBEDO"))
            vec3 CloudAlbedo{ 1.0f, 1.0f, 1.0f };

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Lighting"), DISPLAY_NAME("PROP_EXTINCTION"))
            f32 ExtinctionFactor = 1.0f; // 빛 흡수 계수 (두툼한 음영)

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Lighting"), DISPLAY_NAME("PROP_POWDER_EFFECT"))
            f32 PowderExtinction = 0.5f; // 파우더 슈가 효과 (구름 테두리 보슬보슬한 산란)
    };
}