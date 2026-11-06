#pragma once
#include "Editor_Includes.h"
#include <imgui_internal.h>

namespace Editor
{
    /**
     * @brief ImGui 순정 SplitterBehavior 기반 수직 스플리터 (좌우 분할)
     * @param str_id 위젯 고유 식별자 (예: "##LeftSplitter")
     * @param size1 좌측 영역 너비 포인터
     * @param size2 우측 영역 너비 포인터
     * @param min_size1 좌측 최소 너비
     * @param min_size2 우측 최소 너비
     * @param height 스플리터 막대 높이 (0.0f 이하이면 가용 세로 높이 자동 사용)
     * @param thickness 스플리터 막대 두께 (기본 4.0f)
     * @param hover_extend 마우스 호버 감지 여유 폭 (기본 4.0f)
     * @return 현재 스플리터를 드래그 중이면 true
     */
    inline bool SplitterV(const char* str_id, float* size1, float* size2,
        float min_size1, float min_size2, float height = 0.0f,
        float thickness = 4.0f, float hover_extend = 4.0f)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;
        if (window->SkipItems) return false;

        const ImGuiID id = window->GetID(str_id);
        if (height <= 0.0f) height = ImGui::GetContentRegionAvail().y;

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + thickness, pos.y + height));

        ImGui::ItemSize(bb);
        return ImGui::SplitterBehavior(bb, id, ImGuiAxis_X, size1, size2, min_size1, min_size2, hover_extend);
    }

    template<typename DrawSidebar, typename DrawContent>
    inline void DrawSidebarLayout(const char* sidebarId, const char* splitterId, const char* contentId,
        float& preferredSidebarWidth, float height, float minSidebarWidth, float minContentWidth,
        DrawSidebar&& drawSidebar, DrawContent&& drawContent)
    {
        constexpr float splitterThickness = 4.0f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float paneWidth = std::max(1.0f, availableWidth - splitterThickness - spacing * 2.0f);
        const float halfWidth = paneWidth * 0.5f;

        const float minSidebar = std::min(minSidebarWidth, halfWidth);
        const float minContent = std::clamp(minContentWidth, halfWidth, paneWidth - minSidebar);
        float sidebarWidth = std::clamp(preferredSidebarWidth, minSidebar, paneWidth - minContent);
        float contentWidth = paneWidth - sidebarWidth;
        const float previousSidebarWidth = sidebarWidth;

        if (ImGui::BeginChild(sidebarId, ImVec2(sidebarWidth, height),
            ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar))
        {
            drawSidebar();
        }
        ImGui::EndChild();

        ImGui::SameLine();
        if (SplitterV(splitterId, &sidebarWidth, &contentWidth, minSidebar, minContent, height, splitterThickness) &&
            sidebarWidth != previousSidebarWidth)
        {
            preferredSidebarWidth = sidebarWidth;
        }

        ImGui::SameLine();
        if (ImGui::BeginChild(contentId, ImVec2(0.0f, height), ImGuiChildFlags_Borders))
            drawContent();
        ImGui::EndChild();
    }

    /**
     * @brief ImGui 순정 SplitterBehavior 기반 수평 스플리터 (상하 분할)
     * @param str_id 위젯 고유 식별자 (예: "##BottomSplitter")
     * @param size1 상단 영역 높이 포인터
     * @param size2 하단 영역 높이 포인터
     * @param min_size1 상단 최소 높이
     * @param min_size2 하단 최소 높이
     * @param width 스플리터 막대 너비 (0.0f 이하이면 가용 가로 너비 자동 사용)
     * @param thickness 스플리터 막대 두께 (기본 4.0f)
     * @param hover_extend 마우스 호버 감지 여유 높이 (기본 4.0f)
     * @return 현재 스플리터를 드래그 중이면 true
     */
    inline bool SplitterH(const char* str_id, float* size1, float* size2,
        float min_size1, float min_size2, float width = 0.0f,
        float thickness = 4.0f, float hover_extend = 4.0f)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;
        if (window->SkipItems) return false;

        const ImGuiID id = window->GetID(str_id);
        if (width <= 0.0f) width = ImGui::GetContentRegionAvail().x;

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + width, pos.y + thickness));

        ImGui::ItemSize(bb);
        return ImGui::SplitterBehavior(bb, id, ImGuiAxis_Y, size1, size2, min_size1, min_size2, hover_extend);
    }
}