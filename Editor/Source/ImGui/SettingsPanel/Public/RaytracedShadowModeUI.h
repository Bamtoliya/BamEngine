#pragma once

#include "Editor_Includes.h"
#include "LocalizationManager.h"
#include "RaytracedShadowSettings.h"

BEGIN(Editor)
// 프로젝트·그래픽·뷰포트가 공유하는 레이트레이싱 그림자 3상태 문구. 인덱스는 ERaytracedShadowMode와 같다.
inline constexpr const char* RaytracedShadowModeKeys[] = {
    "GraphicsSettings.RaytracedShadowInherit",
    "GraphicsSettings.RaytracedShadowOn",
    "GraphicsSettings.RaytracedShadowOff"
};

inline bool DrawRaytracedShadowModeCombo(const char* id, int32& mode)
{
    const int32 current = static_cast<int32>(Engine::ToRaytracedShadowMode(mode));
    bool changed = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo(id, LOCAL_CSTR(RaytracedShadowModeKeys[current])))
    {
        for (int32 index = 0; index < IM_ARRAYSIZE(RaytracedShadowModeKeys); ++index)
        {
            if (ImGui::Selectable(LOCAL_LABEL(RaytracedShadowModeKeys[index]).c_str(), current == index) && mode != index)
            {
                mode = index;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", LOCAL_CSTR("GraphicsSettings.RaytracedShadowHint"));
    return changed;
}
END
