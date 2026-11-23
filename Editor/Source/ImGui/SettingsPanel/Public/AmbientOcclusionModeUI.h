#pragma once

#include "Editor_Includes.h"
#include "LocalizationManager.h"
#include "AmbientOcclusion.h"

BEGIN(Editor)
// 프로젝트·그래픽·뷰포트가 공유하는 앰비언트 오클루전 방식 문구. 인덱스는 EAmbientOcclusionMode와 같다.
inline constexpr const char* AmbientOcclusionModeKeys[] = {
    "GraphicsSettings.AmbientOcclusionInherit",
    "GraphicsSettings.AmbientOcclusionOff",
    "GraphicsSettings.AmbientOcclusionScreenSpace",
    "GraphicsSettings.AmbientOcclusionRaytraced"
};

// 실제로 실행한 방식의 표시 문구
inline const char* GetAmbientOcclusionActiveKey(Engine::EAmbientOcclusionMode mode)
{
    switch (mode)
    {
    case Engine::EAmbientOcclusionMode::ScreenSpace: return "GraphicsSettings.AmbientOcclusionActiveScreenSpace";
    case Engine::EAmbientOcclusionMode::Raytraced: return "GraphicsSettings.AmbientOcclusionActiveRaytraced";
    default: return "GraphicsSettings.AmbientOcclusionActiveOff";
    }
}

inline bool DrawAmbientOcclusionModeCombo(const char* id, int32& mode)
{
    const int32 current = static_cast<int32>(Engine::ToAmbientOcclusionMode(mode));
    bool changed = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo(id, LOCAL_CSTR(AmbientOcclusionModeKeys[current])))
    {
        for (int32 index = 0; index < IM_ARRAYSIZE(AmbientOcclusionModeKeys); ++index)
        {
            if (ImGui::Selectable(LOCAL_LABEL(AmbientOcclusionModeKeys[index]).c_str(), current == index) && mode != index)
            {
                mode = index;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", LOCAL_CSTR("GraphicsSettings.AmbientOcclusionHint"));
    return changed;
}
END
