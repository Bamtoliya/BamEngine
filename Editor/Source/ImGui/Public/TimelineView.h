#pragma once

#include <imgui.h>
#include <cstddef>
#include <string_view>

// 타임라인 공통 기본 기능: 시간 축, 눈금자, 재생 헤드, 줌·이동, 트랙 행 시각 규격.
// 애니메이션 타임라인과 이펙트(통합 VFX·파티클) 타임라인이 함께 쓴다. 이펙트 전용 위젯은 EffectTimelineView.h에 둔다.
namespace EditorUI
{
    class ToolbarLayout;

    inline constexpr float MinimumTimelineDuration = 0.05f;
    inline constexpr ImU32 TimelineRulerBackground = IM_COL32(28, 30, 36, 255);
    inline constexpr ImU32 TimelinePlayheadColor = IM_COL32(255, 65, 45, 255);
    inline constexpr ImU32 TimelineEndMarkerColor = IM_COL32(110, 200, 255, 200); // 재생이 끝나는 시점(편집 범위와 다를 때)

    // ── 타임라인 트랙 공통 시각 규격 ──
    inline constexpr ImU32 TimelineHeaderBackground = IM_COL32(30, 33, 38, 255);    // 트랙 이름 칸
    inline constexpr ImU32 TimelineTrackBackground = IM_COL32(40, 44, 52, 255);     // 막대 칸(짝수 행)
    inline constexpr ImU32 TimelineTrackBackgroundAlt = IM_COL32(36, 40, 47, 255);  // 막대 칸(홀수 행)
    inline constexpr ImU32 TimelineRowSeparator = IM_COL32(50, 54, 62, 255);
    inline constexpr ImU32 TimelineGridMajor = IM_COL32(70, 78, 92, 160);           // 눈금자 라벨 위치
    inline constexpr ImU32 TimelineGridMinor = IM_COL32(45, 50, 60, 95);
    inline constexpr ImU32 TimelineLabelText = IM_COL32(205, 212, 224, 255);       // 선택되지 않은 트랙 이름

    enum class TimelineLabelState { Normal, Selected, Disabled }; // Disabled: 음소거·비활성 트랙

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

    // 막대 칸 휠 규칙 (두 타임라인 공통): Alt+휠은 행 세로 스크롤에 쓰므로 무시, Shift+휠은 좌우 이동, 그 외는 줌
    struct TimelineWheel { float Zoom; float PanPixels; };
    TimelineWheel SplitTimelineWheel(float wheel, bool shift, bool alt);

    // 트랙 머리 칸(눈금자 왼쪽): "트랙" 제목과 오른쪽 끝의 트랙 추가(+) 버튼. 버튼을 누른 프레임에 true.
    // 버튼은 ImGui 항목이므로 같은 자리에 겹치는 히트박스보다 먼저 호출하고, 배경은 이 함수보다 먼저 그린다.
    // addTooltipKey가 nullptr이면 버튼 없이 제목만 그린다.
    bool DrawTimelineCorner(ImDrawList& draw, const ImVec2& min, const ImVec2& max, const char* addTooltipKey);

    // 재생 바 버튼: 단축키 액션 "<prefix>.<action>"을 실행하고, 툴팁에 로캘 문구와 단축키 힌트를 표시한다.
    void DrawTimelineAction(ToolbarLayout& toolbar, std::string_view prefix, const char* action, const char* icon, const char* label, bool active = false);
    void DrawTimelineTransport(ToolbarLayout& toolbar, std::string_view prefix, bool playing, bool looping);

    float GetTimelineRowHeight();   // 글꼴 기준 트랙 행 높이
    float GetTimelineBarInset();    // 막대의 위아래 여백
    // 눈금자와 같은 간격의 세로 그리드 (라벨 위치는 주 선, 나머지는 보조 선)
    void DrawTimelineGrid(ImDrawList& draw, const TimelineScale& scale, float top, float bottom, float fps);
    // 트랙 한 행의 배경: 이름 칸, 막대 칸(짝·홀 구분), 선택 강조, 아래 구분선
    void DrawTimelineRowBackground(ImDrawList& draw, float headerLeft, const TimelineScale& scale, float top, float bottom,
        std::size_t index, bool selected);
    float GetTimelineIconSlotWidth();     // 트랙 아이콘 칸 폭 (글리프 폭과 무관하게 고정)
    float GetTimelineLabelTextOffset();   // 이름 칸 왼쪽에서 트랙 이름이 시작하는 거리
    // 트랙 이름 칸: 고정 폭 아이콘 칸 + 이름 (이름 칸 밖으로 넘치지 않게 잘라 그림)
    void DrawTimelineRowLabel(ImDrawList& draw, float headerLeft, float headerRight, float top, float bottom,
        const char* icon, ImU32 iconColor, std::string_view label, TimelineLabelState state);
    // 구간 막대: 채움, 선택 외곽선, 양끝 핸들
    void DrawTimelineBar(ImDrawList& draw, const ImVec2& min, const ImVec2& max, ImU32 color, bool selected, bool showHandles);

    float GetTimelineMarkerHalfSize(); // 시점 마커(다이아몬드)의 반지름
    // 시점 마커: 채움, 외곽선(선택 시 흰색 굵게)
    void DrawTimelineMarker(ImDrawList& draw, const ImVec2& center, ImU32 color, bool selected);
}
