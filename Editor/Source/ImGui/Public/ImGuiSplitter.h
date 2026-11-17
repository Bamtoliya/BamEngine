#pragma once
#include "Editor_Includes.h"
#include <imgui_internal.h>

namespace Editor
{
    // 모든 패널 스플리터의 공통 규격: 레이아웃이 차지하는 보이는 막대 두께, 막대 양옆으로 넓힌 잡기 영역
    inline constexpr float SplitterThickness = 2.0f;
    inline constexpr float SplitterHoverExtend = 4.0f;

    /**
     * @brief 크기 계산을 호출자가 하는 레이아웃용 스플리터 막대
     * @param str_id 위젯 고유 식별자
     * @param axis ImGuiAxis_X면 세로 막대(좌우 분할), ImGuiAxis_Y면 가로 막대(상하 분할)
     * @param length 막대 길이 (0.0f 이하이면 가용 영역 전체)
     * @return 이번 프레임에 끈 거리(축 방향 px). 드래그 중이 아니면 0
     */
    inline float SplitterBar(const char* str_id, ImGuiAxis axis, float length = 0.0f)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return 0.0f;
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        if (length <= 0.0f) length = (axis == ImGuiAxis_X) ? avail.y : avail.x;
        const ImVec2 pos = window->DC.CursorPos;
        const ImVec2 size = (axis == ImGuiAxis_X) ? ImVec2(SplitterThickness, length) : ImVec2(length, SplitterThickness);
        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ImGui::ItemSize(bb);

        // 넉넉한 가상 크기 두 개를 넘겨 끈 거리만 얻는다. 실제 패널 크기의 제한은 호출자가 한다.
        constexpr float virtualSize = 10000.0f;
        float before = virtualSize, after = virtualSize;
        const bool active = ImGui::SplitterBehavior(bb, window->GetID(str_id), axis, &before, &after, 0.0f, 0.0f, SplitterHoverExtend);
        const bool hovered = ImGui::IsItemHovered();
        window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(active ? ImGuiCol_SeparatorActive : (hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator)));
        return active ? before - virtualSize : 0.0f;
    }

    // 다른 자식 창 위에 겹쳐 놓는 스플리터 손잡이. 판정만 하고 그리기는 호출자가 한다.
    // SplitterBar(SplitterBehavior)는 겹침 허용이라 뒤에 제출된 항목에 호버를 빼앗긴다. 이 손잡이는 겹침 허용을 쓰지 않으므로
    // 같은 자리의 다른 항목보다 먼저 제출하면 호버를 우선 가져간다. ItemSize를 호출하지 않아 배치에 영향이 없다.
    struct SplitterHandleState { bool Hovered = false; bool Held = false; };
    inline SplitterHandleState SplitterHandle(const char* str_id, const ImRect& bb, ImGuiAxis axis)
    {
        SplitterHandleState state;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return state;
        const ImGuiID id = window->GetID(str_id);
        if (!ImGui::ItemAdd(bb, id, nullptr, ImGuiItemFlags_NoNav)) return state;
        // FlattenChildren: 마우스가 이 창의 자식 창 위에 있어도 이 창의 항목처럼 판정한다.
        ImGui::ButtonBehavior(bb, id, &state.Hovered, &state.Held, ImGuiButtonFlags_FlattenChildren);
        if (state.Hovered || state.Held) ImGui::SetMouseCursor(axis == ImGuiAxis_X ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        return state;
    }

    // SplitterHandle로 만든 세로 스플리터 막대를 그린다 (SplitterBar와 같은 두께·색 규칙).
    // 손잡이가 여러 창에 걸쳐 있으면 창마다 자기 그리기 목록으로 호출한다.
    inline void DrawVerticalSplitterLine(ImDrawList& draw, const SplitterHandleState& state, float x, float top, float bottom)
    {
        const ImU32 color = ImGui::GetColorU32(state.Held ? ImGuiCol_SeparatorActive : (state.Hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator));
        const float half = SplitterThickness * 0.5f;
        draw.AddRectFilled(ImVec2(x - half, top), ImVec2(x + half, bottom), color);
    }

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
        float thickness = SplitterThickness, float hover_extend = SplitterHoverExtend)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;
        if (window->SkipItems) return false;
        const ImGuiID id = window->GetID(str_id);
        if (height <= 0.0f) height = ImGui::GetContentRegionAvail().y;
        const ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + thickness, pos.y + height));
        ImGui::ItemSize(bb);
        const bool active = ImGui::SplitterBehavior(bb, id, ImGuiAxis_X, size1, size2, min_size1, min_size2, hover_extend);
        const bool hovered = ImGui::IsItemHovered();
        // 얇은 세퍼레이터 라인 시각화
        const ImU32 col = active ? ImGui::GetColorU32(ImGuiCol_SeparatorActive)
            : (hovered ? ImGui::GetColorU32(ImGuiCol_SeparatorHovered)
                : ImGui::GetColorU32(ImGuiCol_Separator));
        window->DrawList->AddRectFilled(bb.Min, bb.Max, col);
        return active;
    }

    template<typename DrawSidebar, typename DrawContent>
    inline void DrawSidebarLayout(const char* sidebarId, const char* splitterId, const char* contentId,
        float& preferredSidebarWidth, float height, float minSidebarWidth, float minContentWidth,
        DrawSidebar&& drawSidebar, DrawContent&& drawContent,
        ImGuiWindowFlags sidebarWindowFlags = ImGuiWindowFlags_HorizontalScrollbar,
        bool reserveHalfForContent = true)
    {
        constexpr float splitterThickness = SplitterThickness;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float paneWidth = std::max(1.0f, availableWidth - splitterThickness - spacing * 2.0f);
        const float halfWidth = paneWidth * 0.5f;

        const float minSidebar = std::min(minSidebarWidth, halfWidth);
        const float minContent = std::clamp(minContentWidth, reserveHalfForContent ? halfWidth : 0.0f, paneWidth - minSidebar);
        float sidebarWidth = std::clamp(preferredSidebarWidth, minSidebar, paneWidth - minContent);
        float contentWidth = paneWidth - sidebarWidth;
        const float previousSidebarWidth = sidebarWidth;

        if (ImGui::BeginChild(sidebarId, ImVec2(sidebarWidth, height), ImGuiChildFlags_Borders, sidebarWindowFlags))
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
        float thickness = SplitterThickness, float hover_extend = SplitterHoverExtend)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;
        if (window->SkipItems) return false;
        const ImGuiID id = window->GetID(str_id);
        if (width <= 0.0f) width = ImGui::GetContentRegionAvail().x;
        const ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + width, pos.y + thickness));
        ImGui::ItemSize(bb);
        const bool active = ImGui::SplitterBehavior(bb, id, ImGuiAxis_Y, size1, size2, min_size1, min_size2, hover_extend);
        const bool hovered = ImGui::IsItemHovered();
        // 얇은 세퍼레이터 라인 시각화
        const ImU32 col = active ? ImGui::GetColorU32(ImGuiCol_SeparatorActive)
            : (hovered ? ImGui::GetColorU32(ImGuiCol_SeparatorHovered)
                : ImGui::GetColorU32(ImGuiCol_Separator));
        window->DrawList->AddRectFilled(bb.Min, bb.Max, col);
        return active;
    }
}