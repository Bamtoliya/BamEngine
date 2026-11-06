#pragma once

#include "Editor_Includes.h"

#include <array>
#include <cstddef>

BEGIN(Editor)

class FrameMetricsWidget
{
public:
    void Update(f32 dt, uint32 targetFPS);
    void ResetHistory();

    [[nodiscard]] bool DrawCard(float cardWidth, bool selected) const;
    void DrawDetails() const;

private:
    void DrawSummary() const;
    void DrawGraph() const;
    void RecalculateStatistics();

    static constexpr std::size_t HistoryCapacity = 240;

    array<f32, HistoryCapacity> m_FrameTimes{};
    size_t m_WriteIndex = 0;
    size_t m_SampleCount = 0;

    f32 m_CurrentMs = 0.0f;
    f32 m_AverageMs = 0.0f;
    f32 m_MinMs = 0.0f;
    f32 m_MaxMs = 0.0f;
    uint32 m_TargetFPS = 0;
};

END