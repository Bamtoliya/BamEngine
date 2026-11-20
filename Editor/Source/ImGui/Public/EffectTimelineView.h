#pragma once

#include "TimelineView.h"
#include <imgui.h>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

namespace Engine { struct FloatCurve; }

// 이펙트(통합 VFX·파티클) 전용 타임라인 위젯과 편집기 레이아웃. 공통 기본 기능은 TimelineView.h에 있다.
namespace EditorUI
{
    inline constexpr float EffectTimelineFPS = 30.0f;

    enum class TimelineAction { PlayPause, Previous, Next, First, Last, Loop, Fit, Delete, Undo, Redo, Save };
    enum class TimelineRowKind { Interval, Marker, OpenEnd };

    struct ParticleTimelineTiming
    {
        float LifetimeMin;
        float LifetimeMax;
        float EmissionLimit;
        bool EventDriven;
        bool Enabled;
        bool Supported;
    };

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
        const Engine::FloatCurve* Curve = nullptr;
        const char* CurveLabel = nullptr;
        bool Disabled = false;
        std::optional<ParticleTimelineTiming> ParticleTiming;
    };

    enum class TimelineDragMode { Seek, Move, Start, End, Duration };

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
        float Start = -0.25f;
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
        bool AddTrack = false; // 트랙 머리 칸의 + 버튼을 누름
    };

    // addTrackTooltipKey를 주면 트랙 머리 칸에 + 버튼을 표시하고, 누르면 TimelineInteraction::AddTrack을 돌려준다.
    TimelineInteraction DrawEffectTimeline(EffectTimelineState& state, std::span<const TimelineRow> rows,
        float duration, float playhead, float minimumDuration, const char* addTrackTooltipKey = nullptr);

    void DrawEffectEditorLayout(float& listWidth, float& inspectorWidth, float& timelineHeight,
        const std::function<void()>& list, const std::function<void()>& preview,
        const std::function<void()>& inspector, const std::function<void()>& timeline,
        const std::function<void()>& transport = nullptr);
}
