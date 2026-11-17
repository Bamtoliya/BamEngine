#include "Editor_Includes.h"
#include "TimelineView.h"
#include "EditorUI.h"
#include "IconsFontAwesome7.h"
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "ShortcutManager.h"


namespace EditorUI
{
    namespace
    {
        // 눈금자와 그리드가 같은 간격을 쓰도록 눈금 간격을 한 곳에서 계산한다.
        struct TimelineTicks { float Tick; float LabelStep; };

        TimelineTicks ComputeTimelineTicks(const TimelineScale& scale, float fps)
        {
            const float minTick = scale.Span * ImGui::GetFontSize() / scale.Width;
            float tick = 1.0f / std::max(1.0f, fps);
            while (tick < minTick) tick *= 2.0f;
            float labelStep = tick * 4.0f;
            const float labelWidth = ImGui::CalcTextSize("0000  00.00s").x;
            while (labelStep / scale.Span * scale.Width < labelWidth) labelStep *= 2.0f;
            return { tick, labelStep };
        }
    }

    float TimelineScale::ToX(float time) const { return Left + (time - Start) / Span * Width; }
    float TimelineScale::ToTime(float x) const { return Start + (x - Left) / Width * Span; }

    TimelineScale MakeTimelineScale(float left, float width, float duration, float zoom, float start)
    {
        constexpr float padBefore = 0.25f; // 0.00s 앞쪽 여백
        constexpr float padAfter = 1.0f;   // 끝 지점 뒤쪽 여백 (1.00s로 넉넉하게 확장)
        // 최소 줌 배율을 0.1f로 완화 (최대 10배 축소 허용)
        const float span = std::max(duration, MinimumTimelineDuration) / std::clamp(zoom, 0.1f, 50.0f);
        const float minStart = -std::max(padBefore, span * 0.15f);
        const float maxStart = std::max(minStart, duration + padAfter - span * 0.85f);
        return { left, std::max(1.0f, width), std::clamp(start, minStart, maxStart), span };
    }

    void NavigateTimeline(float mouseX, float wheel, float panPixels, const TimelineScale& scale,
        float duration, float& zoom, float& start)
    {
        constexpr float padBefore = 0.25f;
        constexpr float padAfter = 1.0f;
        if (wheel != 0.0f)
        {
            const float anchor = std::clamp((mouseX - scale.Left) / scale.Width, 0.0f, 1.0f);
            // 휠 줌 범위를 0.1f ~ 50.0f 로 확대
            zoom = std::clamp(zoom * std::pow(1.25f, wheel), 0.1f, 50.0f);
            start = scale.Start + anchor * (scale.Span - (duration + padBefore + padAfter) / zoom);
        }
        start -= panPixels / scale.Width * (duration / zoom);
        const float span = std::max(duration, MinimumTimelineDuration) / zoom;
        const float minStart = -std::max(padBefore, span * 0.15f);
        const float maxStart = std::max(minStart, duration + padAfter - span * 0.85f);
        start = std::clamp(start, minStart, maxStart);
    }

    void DrawTimelineRuler(ImDrawList& draw, const TimelineScale& scale, float top, float bottom, float fps)
    {
        const float right = scale.Left + scale.Width;
        const float font = ImGui::GetFontSize();
        draw.AddRectFilled(ImVec2(scale.Left, top), ImVec2(right, bottom), TimelineRulerBackground);
        draw.AddLine(ImVec2(scale.Left, bottom), ImVec2(right, bottom), IM_COL32(55, 60, 70, 255));
        draw.PushClipRect(ImVec2(scale.Left, top), ImVec2(right, bottom), true);
        const auto [tick, labelStep] = ComputeTimelineTicks(scale, fps);
        float time = std::ceil(scale.Start / tick) * tick;
        for (int i = 0; i < 512 && time <= scale.Start + scale.Span; ++i, time += tick)
        {
            const float x = scale.ToX(time);
            const float snapped = std::round(time / labelStep) * labelStep;
            const bool isOrigin = std::abs(time) <= tick * 0.25f; // 0.00초 원점선
            const bool major = isOrigin || (std::abs(time - snapped) <= tick * 0.25f);

            draw.AddLine(ImVec2(x, bottom - font * (isOrigin ? 0.9f : (major ? 0.6f : 0.3f))), ImVec2(x, bottom),
                isOrigin ? IM_COL32(255, 200, 80, 255) : (major ? IM_COL32(180, 190, 205, 255) : IM_COL32(90, 95, 105, 200)));
            if (major && time >= -0.001f) // 음수 여백 구간은 지저분하지 않게 라벨 생략
            {
                char label[40];
                std::snprintf(label, sizeof(label), "%.0f  %.2fs", std::round(time * fps), std::max(0.0f, time));
                draw.AddText(ImVec2(x + 2.0f, top + font * 0.15f), isOrigin ? IM_COL32(255, 200, 80, 255) : ImGui::GetColorU32(ImGuiCol_TextDisabled), label);
            }
        }
        draw.PopClipRect();
    }

    void DrawTimelinePlayhead(ImDrawList& draw, const ImVec2& min, const ImVec2& max, float x)
    {
        if (x < min.x || x > max.x) return;
        const float width = ImGui::GetFontSize() * 0.35f;
        draw.PushClipRect(min, max, true);
        draw.AddLine(ImVec2(x, min.y), ImVec2(x, max.y), TimelinePlayheadColor, 2.0f);
        draw.AddTriangleFilled(ImVec2(x - width, min.y), ImVec2(x + width, min.y),
            ImVec2(x, min.y + width * 1.5f), TimelinePlayheadColor);
        draw.PopClipRect();
    }

    TimelineWheel SplitTimelineWheel(float wheel, bool shift, bool alt)
    {
        if (alt) return { 0.0f, 0.0f };
        if (shift) return { 0.0f, wheel * 60.0f };
        return { wheel, 0.0f };
    }

    bool DrawTimelineCorner(ImDrawList& draw, const ImVec2& min, const ImVec2& max, const char* addTooltipKey)
    {
        const float edge = ImGui::GetFontSize() * 0.4f;
        float titleRight = max.x;
        bool addPressed = false;
        if (addTooltipKey)
        {
            const ImVec2 size = GetIconButtonSize(ICON_FA_PLUS, true);
            const ImVec2 pos(max.x - edge - size.x, min.y + (max.y - min.y - size.y) * 0.5f);
            if (pos.x >= min.x + edge) // 이름 칸이 버튼보다 좁으면 버튼을 숨긴다
            {
                ImGui::SetCursorScreenPos(pos);
                IconButtonOptions options;
                options.Compact = true;
                options.NoWrap = true;
                addPressed = IconButton("TimelineAddTrack", ICON_FA_PLUS, addTooltipKey, options);
                titleRight = pos.x - edge * 0.5f;
            }
        }
        const std::string title = std::string(ICON_FA_LAYER_GROUP " ") + LOCAL("Timeline.Tracks");
        draw.PushClipRect(min, ImVec2(titleRight, max.y), true);
        draw.AddText(ImVec2(min.x + edge, min.y + (max.y - min.y - ImGui::GetFontSize()) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), title.c_str());
        draw.PopClipRect();
        return addPressed;
    }

    float GetTimelineRowHeight() { return ImGui::GetFrameHeightWithSpacing(); }
    float GetTimelineBarInset() { return ImGui::GetFontSize() * 0.2f; }

    void DrawTimelineGrid(ImDrawList& draw, const TimelineScale& scale, float top, float bottom, float fps)
    {
        const auto [tick, labelStep] = ComputeTimelineTicks(scale, fps);
        draw.PushClipRect(ImVec2(scale.Left, top), ImVec2(scale.Left + scale.Width, bottom), true);
        float time = std::ceil(scale.Start / tick) * tick;
        for (int i = 0; i < 512 && time <= scale.Start + scale.Span; ++i, time += tick)
        {
            const bool major = std::abs(time - std::round(time / labelStep) * labelStep) <= tick * 0.25f;
            const float x = scale.ToX(time);
            draw.AddLine(ImVec2(x, top), ImVec2(x, bottom), major ? TimelineGridMajor : TimelineGridMinor);
        }
        draw.PopClipRect();
    }

    void DrawTimelineRowBackground(ImDrawList& draw, float headerLeft, const TimelineScale& scale, float top, float bottom,
        std::size_t index, bool selected)
    {
        const float right = scale.Left + scale.Width;
        draw.AddRectFilled(ImVec2(headerLeft, top), ImVec2(scale.Left, bottom), TimelineHeaderBackground);
        draw.AddRectFilled(ImVec2(scale.Left, top), ImVec2(right, bottom), (index % 2 == 0) ? TimelineTrackBackground : TimelineTrackBackgroundAlt);
        if (selected) draw.AddRectFilled(ImVec2(headerLeft, top), ImVec2(right, bottom), ImGui::GetColorU32(ImGuiCol_Header));
        draw.AddLine(ImVec2(headerLeft, bottom - 1.0f), ImVec2(right, bottom - 1.0f), TimelineRowSeparator);
    }

    float GetTimelineIconSlotWidth() { return ImGui::GetFontSize() * 1.3f; }
    float GetTimelineLabelTextOffset()
    {
        const float pad = ImGui::GetFontSize() * 0.4f;
        return pad + GetTimelineIconSlotWidth() + pad * 0.5f;
    }

    void DrawTimelineRowLabel(ImDrawList& draw, float headerLeft, float headerRight, float top, float bottom,
        const char* icon, ImU32 iconColor, std::string_view label, TimelineLabelState state)
    {
        const float pad = ImGui::GetFontSize() * 0.4f;
        const float textY = top + (bottom - top - ImGui::GetFontSize()) * 0.5f;
        const ImU32 disabledColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        draw.PushClipRect(ImVec2(headerLeft, top), ImVec2(headerRight, bottom), true);
        // 아이콘은 고정 폭 칸의 가운데에 둔다. 글리프 폭이 달라도 이름 시작 위치가 모든 행에서 같다.
        if (icon && *icon)
        {
            const float iconX = headerLeft + pad + (GetTimelineIconSlotWidth() - ImGui::CalcTextSize(icon).x) * 0.5f;
            draw.AddText(ImVec2(iconX, textY), state == TimelineLabelState::Disabled ? disabledColor : iconColor, icon);
        }
        const ImU32 textColor = state == TimelineLabelState::Selected ? ImGui::GetColorU32(ImGuiCol_Text)
            : (state == TimelineLabelState::Disabled ? disabledColor : TimelineLabelText);
        draw.AddText(ImVec2(headerLeft + GetTimelineLabelTextOffset(), textY), textColor, label.data(), label.data() + label.size());
        draw.PopClipRect();
    }

    void DrawTimelineBar(ImDrawList& draw, const ImVec2& min, const ImVec2& max, ImU32 color, bool selected, bool showHandles)
    {
        draw.AddRectFilled(min, max, color, 2.0f);
        if (selected) draw.AddRect(min, max, IM_COL32(255, 255, 255, 230), 2.0f);
        if (showHandles)
        {
            draw.AddLine(ImVec2(min.x + 2.0f, min.y), ImVec2(min.x + 2.0f, max.y), IM_COL32_WHITE, 2.0f);
            draw.AddLine(ImVec2(max.x - 2.0f, min.y), ImVec2(max.x - 2.0f, max.y), IM_COL32_WHITE, 2.0f);
        }
    }

    float GetTimelineMarkerHalfSize() { return ImGui::GetFontSize() * 0.4f; }

    void DrawTimelineMarker(ImDrawList& draw, const ImVec2& center, ImU32 color, bool selected)
    {
        const float half = GetTimelineMarkerHalfSize();
        const ImVec2 top(center.x, center.y - half), right(center.x + half, center.y);
        const ImVec2 bottom(center.x, center.y + half), left(center.x - half, center.y);
        draw.AddQuadFilled(top, right, bottom, left, color);
        draw.AddQuad(top, right, bottom, left, selected ? IM_COL32_WHITE : IM_COL32(20, 20, 20, 200), selected ? 2.0f : 1.0f);
    }

    void DrawTimelineAction(ToolbarLayout& toolbar, std::string_view prefix, const char* action,
        const char* icon, const char* label, bool active)
    {
        auto& shortcuts = Editor::ShortcutManager::Get();
        const auto id = std::string(prefix) + "." + action;
        IconButtonOptions options;
        options.Active = active;
        options.Enabled = shortcuts.CanExecuteAction(id);
        options.ShortcutHint = shortcuts.GetShortcutHint(id);
        if (toolbar.Icon(action, icon, label, options)) shortcuts.ExecuteAction(id);
    }

    void DrawTimelineTransport(ToolbarLayout& toolbar, std::string_view prefix, bool playing, bool looping)
    {
        DrawTimelineAction(toolbar, prefix, "FirstFrame", ICON_FA_BACKWARD_FAST, "Action.Timeline.FirstFrame");
        DrawTimelineAction(toolbar, prefix, "PrevFrame", ICON_FA_BACKWARD_STEP, "Action.Timeline.PrevFrame");
        DrawTimelineAction(toolbar, prefix, "PlayPause", playing ? ICON_FA_PAUSE : ICON_FA_PLAY, "Action.Timeline.PlayPause");
        DrawTimelineAction(toolbar, prefix, "NextFrame", ICON_FA_FORWARD_STEP, "Action.Timeline.NextFrame");
        DrawTimelineAction(toolbar, prefix, "LastFrame", ICON_FA_FORWARD_FAST, "Action.Timeline.LastFrame");
        DrawTimelineAction(toolbar, prefix, "ToggleLoop", ICON_FA_REPEAT, "Action.Timeline.ToggleLoop", looping);
        DrawTimelineAction(toolbar, prefix, "ZoomReset", ICON_FA_MAXIMIZE, "Action.Timeline.ZoomReset");
    }
}
