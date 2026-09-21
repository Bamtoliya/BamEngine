#pragma once

#include "ImGuiInterface.h"
#include "RHIFrameMetrics.h"
#include "ResourceMetrics.h"

#define MAX_METRICS_HISTORY 240

BEGIN(Editor)
class MetricsPanel : public ImGuiInterface
{
public:
	MetricsPanel() { m_Name = L"Metrics"; }
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
	Engine::ResourceMetrics m_ResourceMetrics{};

	f32 m_ResourceRefreshElapsed = 0.5f;
};
END