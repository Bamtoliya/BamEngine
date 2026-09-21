#pragma once 

#include "MetricsPanel.h"
#include "TimeManager.h"
#include "SceneManager.h"
#include "RenderPassManager.h"
#include "ResourceManager.h"

void MetricsPanel::Draw()
{
    if (!m_Open)
        return;

    if (ImGui::Begin("Engine Metrics", &m_Open))
    {
        m_Focused = ImGui::IsWindowFocused();
        m_Hovered = ImGui::IsWindowHovered();

        DrawToolbar();

        ImGui::Separator();

        DrawFrameSummary();
        DrawFrameGraph();

        ImGui::Separator();

        DrawEngineStatus();

        ImGui::Separator();
        DrawRenderMetrics();

        ImGui::Separator();
        DrawResourceMetrics();
    }

    ImGui::End();
}

void MetricsPanel::Update(f32 dt)
{
    if (m_Paused)
        return;

    UpdateResourceMetrics(dt);

    m_RenderMetrics = {};

    if (auto* rhi = Engine::Renderer::Get().GetRHI())
    {
        m_RenderMetrics = rhi->GetLastFrameMetrics();
    }

    // 이 패널은 기존 ImGuiManager의 메인 스레드 Update에서 호출됩니다.
    m_TargetFPS = Engine::TimeManager::Get().GetTargetFPS();

    m_ActiveSceneCount =
        Engine::SceneManager::Get().GetActiveScenes().size();

    m_RegisteredPassCount =
        Engine::RenderPassManager::Get().GetAllRenderPasses().size();

    // 그래프와 FPS 계산에 사용할 수 없는 샘플은 제외합니다.
    if (!std::isfinite(dt) || dt <= 0.0f)
        return;

    const f32 frameMs = dt * 1000.0f;

    if (!std::isfinite(frameMs))
        return;

    m_CurrentMs = frameMs;
    m_FrameTimes[m_WriteIndex] = frameMs;

    m_WriteIndex = (m_WriteIndex + 1) % MAX_METRICS_HISTORY;

    if (m_SampleCount < MAX_METRICS_HISTORY)
        ++m_SampleCount;

    RecalculateStatistics();
}

void MetricsPanel::DrawToolbar()
{
    ImGui::Checkbox("Pause capture", &m_Paused);

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Freeze metrics only; the engine keeps running.");

    ImGui::SameLine();

    if (ImGui::Button("Reset history"))
        ResetHistory();
}

void MetricsPanel::DrawFrameSummary()
{
    if (m_SampleCount == 0)
    {
        ImGui::TextDisabled("No frame samples.");
        return;
    }

    const f32 averageFPS =
        m_AverageMs > 0.0f ? 1000.0f / m_AverageMs : 0.0f;

    ImGui::Text("Average FPS: %.1f", averageFPS);

    if (ImGui::BeginTable(
        "FrameStatistics",
        2,
        ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Current frame");
        ImGui::TableNextColumn();
        ImGui::Text("%.3f ms", m_CurrentMs);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Average");
        ImGui::TableNextColumn();
        ImGui::Text("%.3f ms", m_AverageMs);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Minimum");
        ImGui::TableNextColumn();
        ImGui::Text("%.3f ms", m_MinMs);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Maximum");
        ImGui::TableNextColumn();
        ImGui::Text("%.3f ms", m_MaxMs);

        ImGui::EndTable();
    }

    if (m_TargetFPS > 0)
    {
        ImGui::Text(
            "FPS cap: %u  (%.3f ms)",
            m_TargetFPS,
            1000.0 / static_cast<f64>(m_TargetFPS));
    }
    else
    {
        ImGui::TextUnformatted("FPS cap: Unlimited");
    }
}

void MetricsPanel::DrawFrameGraph()
{
    if (m_SampleCount == 0)
        return;

    // 버퍼가 가득 찼다면 다음 쓰기 위치가 가장 오래된 샘플입니다.
    const int32 offset =
        m_SampleCount == MAX_METRICS_HISTORY
        ? static_cast<int32>(m_WriteIndex)
        : 0;

    // 기본 범위 0~33.33ms, 큰 지연이 발생하면 범위를 확장합니다.
    const f32 scaleMax =
        m_MaxMs > 30.0f ? m_MaxMs * 1.1f : 33.333f;

    ImGui::PlotLines(
        "##FrameTimeHistory",
        m_FrameTimes.data(),
        static_cast<int32>(m_SampleCount),
        offset,
        "Frame interval (ms)",
        0.0f,
        scaleMax,
        ImVec2(ImGui::GetContentRegionAvail().x, 130.0f));

    ImGui::Text(
        "Recent %d frames",
        static_cast<int32>(m_SampleCount));

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(
        "Includes frame limiting and waits. "
        "This is not CPU or GPU execution time.");
    ImGui::PopTextWrapPos();
}

void MetricsPanel::DrawEngineStatus()
{
    ImGui::TextUnformatted("Engine state");

    const auto text = fmt::format(
        "Active scenes: {}", m_ActiveSceneCount);

    ImGui::TextUnformatted(text.c_str());

    const auto text2 = fmt::format(
        "Registered render passes: {}", m_RegisteredPassCount);

    ImGui::TextUnformatted(text2.c_str());
}

void MetricsPanel::DrawRenderMetrics()
{
    ImGui::TextUnformatted("RHI draw commands — previous frame");

    if (!m_RenderMetrics.Available)
    {
        ImGui::TextDisabled("No completed capture.");
        return;
    }

    if (!m_RenderMetrics.RenderSucceeded)
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.5f, 0.2f, 1.0f),
            "Render failed: partial counts");
    }

    const auto drawCalls = fmt::format(
        "Draw calls: {}",
        m_RenderMetrics.GetTotalDrawCalls());

    const auto breakdown = fmt::format(
        "Non-indexed: {} / Indexed: {}",
        m_RenderMetrics.DrawCalls,
        m_RenderMetrics.IndexedDrawCalls);

    const auto vertices = fmt::format(
        "Submitted vertices (non-indexed): {}",
        m_RenderMetrics.SubmittedVertices);

    const auto indices = fmt::format(
        "Submitted indices: {}",
        m_RenderMetrics.SubmittedIndices);

    ImGui::TextUnformatted(drawCalls.c_str());
    ImGui::TextUnformatted(breakdown.c_str());

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(vertices.c_str());
    ImGui::TextUnformatted(indices.c_str());
    ImGui::TextUnformatted(
        "Counts engine RHI draws. Excludes direct ImGui backend "
        "draws and texture blits.");
    ImGui::PopTextWrapPos();
}

void MetricsPanel::ResetHistory()
{
    m_FrameTimes.fill(0.0f);

    m_WriteIndex = 0;
    m_SampleCount = 0;

    m_CurrentMs = 0.0f;
    m_AverageMs = 0.0f;
    m_MinMs = 0.0f;
    m_MaxMs = 0.0f;
}

void MetricsPanel::RecalculateStatistics()
{
    if (m_SampleCount == 0)
    {
        m_AverageMs = 0.0f;
        m_MinMs = 0.0f;
        m_MaxMs = 0.0f;
        return;
    }

    f64 totalMs = 0.0;

    m_MinMs = m_FrameTimes[0];
    m_MaxMs = m_FrameTimes[0];

    for (std::size_t i = 0; i < m_SampleCount; ++i)
    {
        const f32 value = m_FrameTimes[i];

        totalMs += value;

        if (value < m_MinMs)
            m_MinMs = value;

        if (value > m_MaxMs)
            m_MaxMs = value;
    }

    m_AverageMs = static_cast<f32>(
        totalMs / static_cast<f64>(m_SampleCount));
}


void MetricsPanel::UpdateResourceMetrics(f32 dt)
{
    if (!std::isfinite(dt) || dt <= 0.0f)
        return;

    m_ResourceRefreshElapsed += dt;

    if (m_ResourceRefreshElapsed < 0.5f)
        return;

    // 긴 프레임 뒤에도 한 번만 조회합니다.
    m_ResourceRefreshElapsed = 0.0f;

    m_ResourceMetrics =
        Engine::ResourceManager::Get().GetMetrics();
}

void MetricsPanel::DrawResourceMetrics()
{
    if (!ImGui::CollapsingHeader(
        "Resources",
        ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    const auto summary = fmt::format(
        "Loaded: {}   Slots: {}   Reusable: {}",
        m_ResourceMetrics.ActiveResources,
        m_ResourceMetrics.TotalSlots,
        m_ResourceMetrics.FreeSlots);

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(summary.c_str());
    ImGui::PopTextWrapPos();

    static constexpr const char* typeNames[] = {
        "Unknown",
        "Texture",
        "Sprite",
        "Material",
        "Material Instance",
        "Shader",
        "Mesh",
        "Model",
        "Animation",
        "Audio Clip",
        "Skeleton",
        "Prefab",
        "Script",
        "Scene",
        "Save"
    };

    static_assert(
        IM_ARRAYSIZE(typeNames) ==
        Engine::ResourceMetrics::TypeCount);

    if (ImGui::BeginTable(
        "ResourceCounts",
        2,
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerH |
        ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Count");
        ImGui::TableHeadersRow();

        for (std::size_t i = 0;
            i < m_ResourceMetrics.CountsByType.size();
            ++i)
        {
            const auto count =
                m_ResourceMetrics.CountsByType[i];

            // 등록된 항목이 없는 종류는 숨깁니다.
            if (count == 0)
                continue;

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(typeNames[i]);

            ImGui::TableNextColumn();

            const auto countText = fmt::format("{}", count);
            ImGui::TextUnformatted(countText.c_str());
        }

        ImGui::EndTable();
    }

    ImGui::TextDisabled("Refresh interval: 0.5 seconds");
}