#pragma once

#include <imgui.h>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

namespace EditorUI
{
    class ToolbarLayout;
    inline constexpr float EffectTimelineFPS = 30.0f;
    inline constexpr float MinimumTimelineDuration = 0.05f;
    inline constexpr ImU32 TimelineRulerBackground = IM_COL32(28, 30, 36, 255);
    inline constexpr ImU32 TimelinePlayheadColor = IM_COL32(255, 65, 45, 255);

    struct TimelineScale
    {
        float Left;
        float Width;
        float Start;
        float Span;
        float ToX(float time) const;
        float ToTime(float x) const;
    };

    TimelineScale MakeTimelineScale(float left, float width, float duration, float zoom, float start);
    void NavigateTimeline(float mouseX, float wheel, float panPixels, const TimelineScale& scale,
        float duration, float& zoom, float& start);
    void DrawTimelineRuler(ImDrawList& draw, const TimelineScale& scale, float top, float bottom, float fps);
    void DrawTimelinePlayhead(ImDrawList& draw, const ImVec2& min, const ImVec2& max, float x);

    enum class TimelineAction { PlayPause, Previous, Next, First, Last, Loop, Fit, Delete, Undo, Redo, Save };
    enum class TimelineRowKind { Interval, Marker, OpenEnd };

    struct TimelineRow
    {
        std::uint64_t Id;
        std::string_view Label;
        const char* Icon;
        ImU32 Color;
        float Start;
        float End;
        bool Selected;
        TimelineRowKind Kind = TimelineRowKind::Interval;
    };

    enum class TimelineDragMode { Seek, Header, Move, Start, End, Duration };

    struct TimelineEdit
    {
        TimelineDragMode Mode;
        std::uint64_t Row;
        float Start;
        float End;
    };

    struct EffectTimelineState
    {
        float Zoom = 1.0f;
        float Start = 0.0f;
        float HeaderWidth = 180.0f;
        std::optional<TimelineEdit> Drag;
        TimelineScale DragScale{};
        float GrabX = 0.0f;
        float OriginalStart = 0.0f;
        float OriginalEnd = 0.0f;
        void Fit() { Zoom = 1.0f; Start = 0.0f; }
    };

    struct TimelineInteraction
    {
        std::optional<std::uint64_t> Selection;
        std::optional<float> Seek;
        std::optional<TimelineEdit> Commit;
    };

    // The widget previews a drag locally and returns one document edit when the mouse is released.
    TimelineInteraction DrawEffectTimeline(EffectTimelineState& state, std::span<const TimelineRow> rows,
        float duration, float playhead, float minimumDuration);

    void DrawEffectEditorLayout(float& listWidth, float& inspectorWidth, float& timelineHeight,
        const std::function<void()>& list, const std::function<void()>& preview,
        const std::function<void()>& inspector, const std::function<void()>& timeline);
    void DrawTimelineAction(ToolbarLayout& toolbar, std::string_view prefix, const char* action,
        const char* icon, const char* label, bool active = false);
    void DrawTimelineTransport(ToolbarLayout& toolbar, std::string_view prefix, bool playing, bool looping);
}
