#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "Components/Render/VolumetricCloudComponent.h"
#include "Types.h"

namespace Engine
{
    // HLSL register(b1, space3)와 1:1로 일치하는 128바이트 GPU 구조체
    struct SkyAtmosphereData
    {
        vec3 ZenithColor{ 0.18f, 0.42f, 0.85f };  float _pad0 = 0.0f;
        vec3 HorizonColor{ 0.70f, 0.82f, 0.95f }; float _pad1 = 0.0f;
        vec3 GroundColor{ 0.15f, 0.13f, 0.12f };  float _pad2 = 0.0f;
        vec3 SunDirection{ 0.4f, 0.7f, -0.5f };   float HorizonBlend = 0.35f;
        vec3 SunColor{ 1.0f, 0.95f, 0.85f };      float SunIntensity = 2.5f;
        float SkyIntensity = 1.0f; float SunSize = 0.035f; vec2 _pad3{ 0.0f };

        // ── 구름 파라미터 (32바이트 추가 -> 총 128바이트) ──
        float CloudCoverage = 0.45f;
        float CloudDensity = 0.8f;
        float CloudSpeed = 0.03f;
        float CloudAltitude = 2500.0f; // <-- 구름 층 고도 반영!

        uint32 EnableClouds = 1u;
        vec3 _pad4{ 0.0f };

        // ── 볼류메트릭 클라우드 데이터 (128바이트) ──
        VolumetricCloudData Cloud{};
    };
    static_assert(sizeof(SkyAtmosphereData) == 256, "SkyAtmosphereData size must be 256 bytes");

    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_SKY_ATMOSPHERE"), CATEGORY("CATEGORY_RENDER"))
    struct SkyAtmosphereComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
        bool IsEnabled = true;

        PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("Sky Color"), DISPLAY_NAME("PROP_ZENITH_COLOR"))
        vec3 ZenithColor{ 0.18f, 0.42f, 0.85f };

        PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("Sky Color"), DISPLAY_NAME("PROP_HORIZON_COLOR"))
        vec3 HorizonColor{ 0.70f, 0.82f, 0.95f };

        PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("Sky Color"), DISPLAY_NAME("PROP_GROUND_COLOR"))
        vec3 GroundColor{ 0.15f, 0.13f, 0.12f };

        PROPERTY(EDITABLE, CATEGORY("Sky Color"), DISPLAY_NAME("PROP_HORIZON_BLEND"))
        f32 HorizonBlend = 0.35f;

        PROPERTY(EDITABLE, CATEGORY("Atmosphere"), DISPLAY_NAME("PROP_SKY_INTENSITY"))
        f32 SkyIntensity = 1.0f;

        PROPERTY(EDITABLE, CATEGORY("Sun Disc"), DISPLAY_NAME("PROP_SUN_SIZE"))
        f32 SunSize = 0.035f;

        PROPERTY(EDITABLE, CATEGORY("Sun Disc"), DISPLAY_NAME("PROP_AUTO_SYNC_DIRECTIONAL_LIGHT"))
        bool AutoSyncDirectionalLight = true;

        PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("Sun Disc"), DISPLAY_NAME("PROP_SUN_COLOR"))
        vec3 CustomSunColor{ 1.0f, 0.95f, 0.85f };

        PROPERTY(EDITABLE, CATEGORY("Sun Disc"), DISPLAY_NAME("PROP_SUN_INTENSITY"))
        f32 CustomSunIntensity = 2.5f;

        // Engine/Source/Core/Public/Components/Render/SkyAtmosphereComponent.h (55~66행 교체)
        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Time of Day"), DISPLAY_NAME("PROP_TIME_OF_DAY"))
        f32 TimeOfDay = 12.0f; // 0.0 ~ 24.0 (예: 12.0 = 정오, 18.0 = 일몰)

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Time of Day"), DISPLAY_NAME("PROP_ENABLE_TIME_CYCLE"))
        bool EnableTimeProgression = false; // 실시간 시간 흐름 켜기/끄기

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Time of Day"), DISPLAY_NAME("PROP_DAY_DURATION"))
        f32 DayCycleDuration = 120.0f; // 현실 시간 120초(2분) 동안 인게임 24시간이 경과

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("Time of Day"), DISPLAY_NAME("PROP_SUN_LATITUDE"))
        f32 Latitude = 45.0f; // 태양 궤도 기울기(위도, 도 단위: 45도)

        PROPERTY(EDITABLE, CATEGORY("Clouds"), DISPLAY_NAME("PROP_ENABLE_CLOUDS"))
        bool EnableClouds = true;

        PROPERTY(EDITABLE, CATEGORY("Clouds"), DISPLAY_NAME("PROP_CLOUD_COVERAGE"))
        f32 CloudCoverage = 0.45f; // 구름 양 (0.0: 맑은 하늘 ~ 1.0: 흐린 하늘)

        PROPERTY(EDITABLE, CATEGORY("Clouds"), DISPLAY_NAME("PROP_CLOUD_DENSITY"))
        f32 CloudDensity = 0.8f; // 구름 두께 / 불투명도

        PROPERTY(EDITABLE, CATEGORY("Clouds"), DISPLAY_NAME("PROP_CLOUD_SPEED"))
        f32 CloudSpeed = 0.03f; // 구름이 흘러가는 바람 속도

        PROPERTY(EDITABLE, CATEGORY("Clouds"), DISPLAY_NAME("PROP_CLOUD_ALTITUDE"))
        f32 CloudAltitude = 2500.0f; // 구름 층 고도 (미터 단위)
    };
}