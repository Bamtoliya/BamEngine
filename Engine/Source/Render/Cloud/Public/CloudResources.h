#pragma once

#include "Engine_API.h"

namespace Engine
{
    // 하늘 재질이 쓰는 구름 노이즈 텍스처를 준비하고 연결한다.
    // 키는 ResourceManager::LoadFile이 등록하는 가상 URI와 같아야 미리 로드한 리소스를 찾을 수 있다.
    class CloudResources final
    {
    public:
        static constexpr const wchar_t* BaseShapeKey = L"Project://Texture/CloudBaseShape3D.bamtex";
        static constexpr const wchar_t* WeatherMapKey = L"Project://Texture/CloudWeatherMap2D.bamtex";
        static constexpr const wchar_t* SkyMaterialKey = L"Engine://Material/SkyMaterial.bammat.json";

        // GPU 업로드가 있을 수 있으므로 패스 기록 전에 호출한다. 텍스처는 처음 필요할 때 로드하거나 만든다.
        // 하늘 재질에 두 텍스처를 모두 연결했으면 true.
        ENGINE_API static bool Prepare();
    };
}
