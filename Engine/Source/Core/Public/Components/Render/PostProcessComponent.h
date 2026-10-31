#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "Types.h"

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_POST_PROCESS"), CATEGORY("CATEGORY_RENDER"))
        struct PostProcessComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
        bool IsEnabled = true;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UNBOUND"))
        bool IsUnbound = true; // 전역 볼륨 여부 (true면 씬 전체에 적용)

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PRIORITY"))
        f32 Priority = 0.0f; // 볼륨 간 우선순위

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PRESET_NAME"))
        string PresetName = "Cinematic"; // 적용할 프리셋 이름 (Resources/Config/PostProcess/Presets/*.json)

        PROPERTY(READONLY, DISPLAY_NAME("PROP_PIPELINE_SETTINGS"), ALLOW_MISSING)
        string PipelineSettings; // 씬 파일에 인라인으로 저장/복원되는 실시간 파이프라인 JSON 데이터
    };
}