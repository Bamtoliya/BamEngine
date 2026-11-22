#pragma once

#include "ComponentAnnotations.h"
#include "VolumetricCloudComponent.h"
#include "Types.h"
#include "ResourceHandle.h"
#include "Texture.h"

namespace Engine
{
    // HLSL register(b1, space3)와 메모리 배치가 일치하는 256바이트 GPU 구조체
    struct SkyAtmosphereData
    {
        vec3 ZenithColor{ 0.18f, 0.42f, 0.85f };  float _pad0 = 0.0f;
        vec3 HorizonColor{ 0.70f, 0.82f, 0.95f }; float _pad1 = 0.0f;
        vec3 GroundColor{ 0.15f, 0.13f, 0.12f };  float _pad2 = 0.0f;
        vec3 SunDirection{ 0.4f, 0.7f, -0.5f };   float HorizonBlend = 0.35f;
        vec3 SunColor{ 1.0f, 0.95f, 0.85f };      float SunIntensity = 2.5f;
        float SkyIntensity = 1.0f; float SunSize = 0.035f; vec2 _pad3{ 0.0f };

        // ── 채우지 않는 이전 구름 필드 (32바이트). 실제 값은 아래 Cloud에 있다 ──
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
    static_assert(offsetof(SkyAtmosphereData, Cloud) == 128, "SkyAtmosphereData::Cloud offset must be 128 bytes");

    // 구름은 VolumetricCloudComponent에서만 설정한다.
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_SKY_ATMOSPHERE"), CATEGORY("CATEGORY_RENDER"))
    struct SkyAtmosphereComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"), META("ComponentEnable", true))
        bool IsEnabled = true;
        static constexpr auto ComponentEnableMember = &SkyAtmosphereComponent::IsEnabled;

        // 지평선 아래로 보이는 지표의 색
        PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("CATEGORY_ATMOSPHERE"), DISPLAY_NAME("PROP_PLANET_SURFACE_COLOR"))
        vec3 GroundColor{ 0.15f, 0.13f, 0.12f };

        // 하늘에는 쓰지 않는다. AutoSyncAtmosphere를 켠 Fog의 색에 섞는다.
        PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("CATEGORY_ATMOSPHERE"), DISPLAY_NAME("PROP_FOG_BLEND_COLOR"))
        vec3 HorizonColor{ 0.70f, 0.82f, 0.95f };

        // 현재 하늘 셰이더가 읽지 않는 이전 값. 저장 형식을 유지하려고 남긴다.
        PROPERTY(EDITABLE, CATEGORY("CATEGORY_ATMOSPHERE"), DISPLAY_NAME("PROP_HORIZON_BLEND"), VISIBLE(false))
        f32 HorizonBlend = 0.35f;

        PROPERTY(EDITABLE, CATEGORY("CATEGORY_ATMOSPHERE"), DISPLAY_NAME("PROP_SKY_INTENSITY"))
        f32 SkyIntensity = 1.0f;

        PROPERTY(EDITABLE, CATEGORY("CATEGORY_SUN_DISC"), DISPLAY_NAME("PROP_SUN_SIZE"))
        f32 SunSize = 0.035f;

        PROPERTY(EDITABLE, CATEGORY("CATEGORY_SUN_DISC"), DISPLAY_NAME("PROP_AUTO_SYNC_DIRECTIONAL_LIGHT"))
        bool AutoSyncDirectionalLight = true;

        PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("CATEGORY_SUN_DISC"), DISPLAY_NAME("PROP_SUN_COLOR"), EDITCONDITION(!AutoSyncDirectionalLight))
        vec3 CustomSunColor{ 1.0f, 0.95f, 0.85f };

        PROPERTY(EDITABLE, CATEGORY("CATEGORY_SUN_DISC"), DISPLAY_NAME("PROP_SUN_INTENSITY"), EDITCONDITION(!AutoSyncDirectionalLight))
        f32 CustomSunIntensity = 2.5f;

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_TIME_OF_DAY"), DISPLAY_NAME("PROP_TIME_OF_DAY"))
        f32 TimeOfDay = 12.0f; // 0.0 ~ 24.0 (예: 12.0 = 정오, 18.0 = 일몰)

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_TIME_OF_DAY"), DISPLAY_NAME("PROP_ENABLE_TIME_CYCLE"))
        bool EnableTimeProgression = false; // 실시간 시간 흐름 켜기/끄기

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_TIME_OF_DAY"), DISPLAY_NAME("PROP_DAY_DURATION"), EDITCONDITION(EnableTimeProgression))
        f32 DayCycleDuration = 120.0f; // 현실 시간 120초(2분) 동안 인게임 24시간이 경과

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_TIME_OF_DAY"), DISPLAY_NAME("PROP_SUN_LATITUDE"))
        f32 Latitude = 45.0f; // 태양 궤도 기울기(위도, 도 단위: 45도)

        // ── 환경광: IBL 맵, Sky Light, ZenithColor 순으로 하나만 사용하며 아래 강도는 세 경로에 공통으로 적용한다 ──
        PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("CATEGORY_AMBIENT"), DISPLAY_NAME("PROP_FALLBACK_AMBIENT_COLOR"))
        vec3 ZenithColor{ 0.18f, 0.42f, 0.85f };
        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_AMBIENT"), DISPLAY_NAME("PROP_IRRADIANCE_MAP"))
        ResourceHandle<Texture> IrradianceMap;
        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_AMBIENT"), DISPLAY_NAME("PROP_PREFILTERED_MAP"))
        ResourceHandle<Texture> PrefilteredMap;
        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_AMBIENT"), DISPLAY_NAME("PROP_BRDF_LUT"))
        ResourceHandle<Texture> BRDFLutMap;
        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_AMBIENT"), DISPLAY_NAME("PROP_IBL_INTENSITY"))
        f32 IBLIntensity = 1.0f;

        PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_AMBIENT"), DISPLAY_NAME("PROP_SHADOW_AMBIENT_FLOOR"), RANGE(0.0f, 1.0f, 0.01f))
        f32 ShadowAmbientFloor = 0.15f;
    };
}
