#pragma once

#include "ImGuiInterface.h"
#include "RHIFrameMetrics.h"
#include "ResourceMetrics.h"
#include "AssetTaskMetrics.h"

#define MAX_METRICS_HISTORY 240

BEGIN(Editor)
class MetricsPanel : public IImGui
{
public:
	MetricsPanel() { m_Name = L"Metrics"; m_DisplayNameKey = "UI_ENGINE_METRICS_PANEL"; }
	virtual ~MetricsPanel() override = default;
	virtual void Draw() override;
	virtual void Update(f32 dt) override;
private:
	void DrawToolbar();
	void DrawFrameSummary();
	void DrawFrameGraph();
	void DrawEngineStatus();

	void DrawRenderMetrics();

	void ResetHistory();
	void RecalculateStatistics();

	void UpdateResourceMetrics(f32 dt);
	void DrawResourceMetrics();

	void DrawAssetTaskMetrics();
	void UpdatePassStats();
	void UpdateDrawSnapshot(f32 dt);
private:
	array<f32, MAX_METRICS_HISTORY> m_FrameTimes{};

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
	// 쓰로틀링된 패스가 매 프레임 나타났다 사라지며 표가 깜빡이는 것을 막습니다.
	static constexpr f64 PassHoldSeconds = 1.0;
	static constexpr f64 PassStaleSeconds = 0.25;
	static constexpr f32 PassSmoothing = 0.1f;
	std::unordered_map<std::wstring, PassStat> m_PassStats;
	f64 m_ElapsedTime = 0.0;
	// 쓰로틀링으로 프레임마다 부하가 달라도 숫자가 깜빡이지 않도록 구간 최대 프레임을 표시합니다.
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