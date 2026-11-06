#pragma once

#include "Editor_Includes.h"
#include "RHIFrameMetrics.h"

#include <string>
#include <unordered_map>

BEGIN(Editor)

class RenderMetricsWidget
{
public:
    void Update(f32 dt);
    void ResetHistory();

    [[nodiscard]] bool DrawCard(float cardWidth, bool isSelected) const;
    void DrawDetails() const;

private:
    void UpdatePassStats();
    void UpdateDrawSnapshot(f32 dt);

    struct PassStat
    {
        string DisplayName;
        f32 CPUTimeMs = 0.0f;
        f32 GPUTimeMs = 0.0f;
        f64 LastSeenTime = 0.0;
    };

    struct DrawSnapshot
    {
        uint64 DrawCalls = 0;
        uint64 IndexedDrawCalls = 0;
        uint64 SubmittedVertices = 0;
        uint64 SubmittedIndices = 0;
        f32 TotalCPUTimeMs = 0.0f;
        f32 TotalGPUTimeMs = 0.0f;
        bool Available = false;
        bool RenderFailed = false;

        uint64 GetTotalDrawCalls() const { return DrawCalls + IndexedDrawCalls; }
    };

    static constexpr f64 PassHoldSeconds = 1.0;
    static constexpr f64 PassStaleSeconds = 0.25;
    static constexpr f32 PassSmoothing = 0.1f;
    static constexpr f64 DrawSnapshotWindowSeconds = 0.5;

    RHIFrameMetrics m_RenderMetrics{};
    RHICapabilities m_Capabilities{};
    unordered_map<wstring, PassStat> m_PassStats;
    f64 m_ElapsedTime = 0.0;

    DrawSnapshot m_WindowPeak{};
    DrawSnapshot m_DisplaySnapshot{};
    f64 m_DrawSnapshotElapsed = 0.0;
};

END