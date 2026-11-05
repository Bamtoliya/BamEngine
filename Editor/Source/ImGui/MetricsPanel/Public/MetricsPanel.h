#pragma once

#include "ImGuiInterface.h"
#include "RHIFrameMetrics.h"
#include "ResourceMetrics.h"
#include "AssetTaskMetrics.h"
#include <array>
#include <unordered_map>
#include <string>

#define MAX_METRICS_HISTORY 240

BEGIN(Editor)

// 대시보드 상세 보기 대상 도메인
enum class EMetricsDomain : std::uint8_t
{
    None = 0,
    Performance = 1,
    Rendering = 2,
    Memory = 3,
    EngineTasks = 4,
    Network = 5
};

class MetricsPanel : public IImGui
{
public:
    MetricsPanel() { m_Name = L"Metrics"; m_DisplayNameKey = "Metrics.Title"; }
    virtual ~MetricsPanel() override = default;
    virtual void Draw() override;
    virtual void Update(f32 dt) override;

private:
    void DrawToolbar();

    // 상단 반응형 요약 카드 그리드
    void DrawDashboardCards();
    void DrawCardPerformance(float cardWidth);
    void DrawCardRendering(float cardWidth);
    void DrawCardMemory(float cardWidth);
    void DrawCardEngineTasks(float cardWidth);
    void DrawCardNetwork(float cardWidth);

    // 하단 마스터-디테일 심층 분석 뷰
    void DrawDetailedInspectionView();

    // 기존 상세 렌더링 함수들 (보존)
    void DrawFrameSummary();
    void DrawFrameGraph();
    void DrawEngineStatus();
    void DrawRenderMetrics();
    void DrawResourceMetrics();
    void DrawAssetTaskMetrics();
    void DrawNetworkMetrics();

    void ResetHistory();
    void RecalculateStatistics();
    void UpdateResourceMetrics(f32 dt);
    void UpdatePassStats();
    void UpdateDrawSnapshot(f32 dt);

private:
    EMetricsDomain m_SelectedDomain = EMetricsDomain::None;

    std::array<f32, MAX_METRICS_HISTORY> m_FrameTimes{};
    size_t m_WriteIndex = 0;
    size_t m_SampleCount = 0;

    f32 m_CurrentMs = 0.0f;
    f32 m_AverageMs = 0.0f;
    f32 m_MinMs = 0.0f;
    f32 m_MaxMs = 0.0f;

    uint32 m_TargetFPS = 0;
    std::size_t m_ActiveSceneCount = 0;
    std::size_t m_RegisteredPassCount = 0;

    bool m_Paused = false;

    Engine::RHIFrameMetrics m_RenderMetrics{};
    Engine::RHICapabilities m_Capabilities{};
    Engine::ResourceMetrics m_ResourceMetrics{};
    f32 m_ResourceRefreshElapsed = 0.5f;

    AssetTaskMetrics m_AssetTaskMetrics{};

    struct PassStat
    {
        std::string DisplayName;
        f32 CPUTimeMs = 0.0f;
        f32 GPUTimeMs = 0.0f;
        f64 LastSeenTime = 0.0;
    };
    static constexpr f64 PassHoldSeconds = 1.0;
    static constexpr f64 PassStaleSeconds = 0.25;
    static constexpr f32 PassSmoothing = 0.1f;
    std::unordered_map<std::wstring, PassStat> m_PassStats;
    f64 m_ElapsedTime = 0.0;

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
    static constexpr f64 DrawSnapshotWindowSeconds = 0.5;
    DrawSnapshot m_WindowPeak{};
    DrawSnapshot m_DisplaySnapshot{};
    f64 m_DrawSnapshotElapsed = 0.0;
};

END