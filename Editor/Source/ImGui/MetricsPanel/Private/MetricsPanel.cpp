#include "MetricsPanel.h"
#include "TimeManager.h"
#include "SceneManager.h"
#include "RenderPassManager.h"
#include "ResourceManager.h"
#include "AssetManager.h"
#include "RenderPass.h"
#include "SystemManager.h"
#include "NetworkSystem.h"
#include <algorithm>
#include <format>

void MetricsPanel::Draw()
{
    if (!m_Open) return;

    const string windowTitle = LOCAL("UI_ENGINE_METRICS_PANEL") + "###EngineMetricsPanel";
    if (ImGui::Begin(windowTitle.c_str(), &m_Open))
    {
        m_Focused = ImGui::IsWindowFocused();
        m_Hovered = ImGui::IsWindowHovered();

        DrawToolbar();
        ImGui::Separator();

        // 1. 상단: 반응형 핵심 KPI 요약 카드 그리드
        DrawDashboardCards();

        // 2. 하단: 사용자가 카드를 클릭했을 때만 심층 디테일 전개
        if (m_SelectedDomain != EMetricsDomain::None)
        {
            ImGui::Spacing();
            ImGui::Separator();
            DrawDetailedInspectionView();
        }
    }
    ImGui::End();
}

void MetricsPanel::DrawDashboardCards()
{
    const float availWidth = ImGui::GetContentRegionAvail().x;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;

    // 창 너비에 따른 반응형 열(Column) 수 결정
    int cols = 4;
    if (availWidth < 550.0f) cols = 1;
    else if (availWidth < 850.0f) cols = 2;
    else if (availWidth < 1100.0f) cols = 3;

    const float cardWidth = (availWidth - (spacing * (cols - 1))) / static_cast<float>(cols);

    int currentCol = 0;
    auto BeginNextCard = [&]() {
        if (currentCol > 0 && currentCol % cols != 0) ImGui::SameLine();
        currentCol++;
        };

    BeginNextCard();
    DrawCardPerformance(cardWidth);

    BeginNextCard();
    DrawCardRendering(cardWidth);

    BeginNextCard();
    DrawCardMemory(cardWidth);

    BeginNextCard();
    DrawCardEngineTasks(cardWidth);

    BeginNextCard();
    DrawCardNetwork(cardWidth);
}

void MetricsPanel::DrawCardPerformance(float cardWidth)
{
    const bool isSelected = (m_SelectedDomain == EMetricsDomain::Performance);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, isSelected ? ImVec4(0.15f, 0.25f, 0.35f, 0.8f) : ImVec4(0.14f, 0.14f, 0.16f, 0.7f));
    ImGui::BeginChild("Card_Performance", ImVec2(cardWidth, 125.0f), true, ImGuiWindowFlags_NoScrollbar);

    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), LOCAL_CSTR("Metrics.Performance"));
    ImGui::SameLine(cardWidth - 75.0f);
    const float fps = m_AverageMs > 0.0f ? 1000.0f / m_AverageMs : 0.0f;
    if (fps >= 55.0f) ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), LOCAL_CSTR("Metrics.Good"));
    else if (fps >= 30.0f) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), LOCAL_CSTR("Metrics.Warn"));
    else ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), LOCAL_CSTR("Metrics.Poor"));

    ImGui::Text(LOCAL_CSTR("Metrics.Fps"), fps);
    ImGui::TextDisabled(LOCAL_CSTR("Metrics.FrameMsAvg"), m_CurrentMs, m_AverageMs);

    ImGui::Spacing();
    if (ImGui::SmallButton((LOCAL(isSelected ? "Metrics.CloseDetail" : "Metrics.ViewDetails") + "###Details").c_str()))
    {
        m_SelectedDomain = isSelected ? EMetricsDomain::None : EMetricsDomain::Performance;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void MetricsPanel::DrawCardRendering(float cardWidth)
{
    const bool isSelected = (m_SelectedDomain == EMetricsDomain::Rendering);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, isSelected ? ImVec4(0.15f, 0.25f, 0.35f, 0.8f) : ImVec4(0.14f, 0.14f, 0.16f, 0.7f));
    ImGui::BeginChild("Card_Rendering", ImVec2(cardWidth, 125.0f), true, ImGuiWindowFlags_NoScrollbar);

    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), LOCAL_CSTR("Metrics.GpuRendering"));
    ImGui::SameLine(cardWidth - 75.0f);
    if (m_DisplaySnapshot.RenderFailed) ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), LOCAL_CSTR("Metrics.Fail"));
    else ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), LOCAL_CSTR("Metrics.Ok"));

    ImGui::Text(LOCAL_CSTR("Metrics.Draws"), m_DisplaySnapshot.GetTotalDrawCalls());
    ImGui::TextDisabled(LOCAL_CSTR("Metrics.GpuTimeMsVerts"), m_DisplaySnapshot.TotalGPUTimeMs, m_DisplaySnapshot.SubmittedVertices);

    ImGui::Spacing();
    if (ImGui::SmallButton((LOCAL(isSelected ? "Metrics.CloseDetail" : "Metrics.ViewDetails") + "###Details").c_str()))
    {
        m_SelectedDomain = isSelected ? EMetricsDomain::None : EMetricsDomain::Rendering;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void MetricsPanel::DrawCardMemory(float cardWidth)
{
    const bool isSelected = (m_SelectedDomain == EMetricsDomain::Memory);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, isSelected ? ImVec4(0.15f, 0.25f, 0.35f, 0.8f) : ImVec4(0.14f, 0.14f, 0.16f, 0.7f));
    ImGui::BeginChild("Card_Memory", ImVec2(cardWidth, 125.0f), true, ImGuiWindowFlags_NoScrollbar);

    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), LOCAL_CSTR("Metrics.MemoryCache"));
    ImGui::SameLine(cardWidth - 75.0f);
    ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), LOCAL_CSTR("Metrics.Healthy"));

    ImGui::Text(LOCAL_CSTR("Metrics.ActiveSlots"), m_ResourceMetrics.ActiveResources, m_ResourceMetrics.TotalSlots);
    const auto modelCount = m_ResourceMetrics.CountsByType[static_cast<size_t>(Engine::EResourceType::Model)];
    const auto texCount = m_ResourceMetrics.CountsByType[static_cast<size_t>(Engine::EResourceType::Texture)];
    ImGui::TextDisabled(LOCAL_CSTR("Metrics.ModelsTextures"), modelCount, texCount);

    ImGui::Spacing();
    if (ImGui::SmallButton((LOCAL(isSelected ? "Metrics.CloseDetail" : "Metrics.ViewDetails") + "###Details").c_str()))
    {
        m_SelectedDomain = isSelected ? EMetricsDomain::None : EMetricsDomain::Memory;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void MetricsPanel::DrawCardEngineTasks(float cardWidth)
{
    const bool isSelected = (m_SelectedDomain == EMetricsDomain::EngineTasks);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, isSelected ? ImVec4(0.15f, 0.25f, 0.35f, 0.8f) : ImVec4(0.14f, 0.14f, 0.16f, 0.7f));
    ImGui::BeginChild("Card_EngineTasks", ImVec2(cardWidth, 125.0f), true, ImGuiWindowFlags_NoScrollbar);

    ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), LOCAL_CSTR("Metrics.TasksEngine"));
    ImGui::SameLine(cardWidth - 75.0f);
    if (m_AssetTaskMetrics.Outstanding > 0) ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), LOCAL_CSTR("Metrics.Busy"));
    else ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), LOCAL_CSTR("Metrics.Idle"));

    ImGui::Text(LOCAL_CSTR("Metrics.PassesScenes"), m_RegisteredPassCount, m_ActiveSceneCount);
    ImGui::TextDisabled(LOCAL_CSTR("Metrics.TasksActiveDone"), m_AssetTaskMetrics.Outstanding, m_AssetTaskMetrics.GetCompleted());

    ImGui::Spacing();
    if (ImGui::SmallButton((LOCAL(isSelected ? "Metrics.CloseDetail" : "Metrics.ViewDetails") + "###Details").c_str()))
    {
        m_SelectedDomain = isSelected ? EMetricsDomain::None : EMetricsDomain::EngineTasks;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void MetricsPanel::DrawCardNetwork(float cardWidth)
{
    const bool isSelected = (m_SelectedDomain == EMetricsDomain::Network);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, isSelected ? ImVec4(0.15f, 0.25f, 0.35f, 0.8f) : ImVec4(0.14f, 0.14f, 0.16f, 0.7f));
    ImGui::BeginChild("Card_Network", ImVec2(cardWidth, 125.0f), true, ImGuiWindowFlags_NoScrollbar);

    ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.85f, 1.0f), LOCAL_CSTR("Metrics.Network"));
    ImGui::SameLine(cardWidth - 75.0f);

    auto* sysMgr = &Engine::SystemManager::Get();
    auto* netSystem = sysMgr ? sysMgr->GetSystem<Engine::NetworkSystem>() : nullptr;
    if (!netSystem || netSystem->GetDriver().GetNetworkMode() == Engine::Network::ENetworkMode::Standalone)
    {
        ImGui::TextDisabled(LOCAL_CSTR("Metrics.Offline"));
        ImGui::Text(LOCAL_CSTR("Metrics.ModeStandalone"));
        ImGui::TextDisabled(LOCAL_CSTR("Metrics.TrafficKbS"));
    }
    else
    {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), LOCAL_CSTR("Metrics.Online"));
        if (netSystem->GetDriver().IsClient())
            ImGui::Text(LOCAL_CSTR("Metrics.PingMs"), netSystem->GetDriver().GetSmoothedRTT());
        else
            ImGui::Text(LOCAL_CSTR("Metrics.ServerPort"), netSystem->GetDriver().GetConfig().ServerPort);
        ImGui::TextDisabled(LOCAL_CSTR("Metrics.StatusConnected"));
    }

    ImGui::Spacing();
    if (ImGui::SmallButton((LOCAL(isSelected ? "Metrics.CloseDetail" : "Metrics.ViewDetails") + "###Details").c_str()))
    {
        m_SelectedDomain = isSelected ? EMetricsDomain::None : EMetricsDomain::Network;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void MetricsPanel::DrawDetailedInspectionView()
{
    std::string domainTitle;
    switch (m_SelectedDomain)
    {
    case EMetricsDomain::Performance: domainTitle = LOCAL_CSTR("Metrics.DetailPerformance"); break;
    case EMetricsDomain::Rendering:   domainTitle = LOCAL_CSTR("Metrics.DetailRendering"); break;
    case EMetricsDomain::Memory:      domainTitle = LOCAL_CSTR("Metrics.DetailMemory"); break;
    case EMetricsDomain::EngineTasks: domainTitle = LOCAL_CSTR("Metrics.DetailTasks"); break;
    case EMetricsDomain::Network:     domainTitle = LOCAL_CSTR("Metrics.DetailNetwork"); break;
    default: break;
    }

    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), ">> %s", domainTitle.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 110.0f);
    if (ImGui::Button(LOCAL_LABEL("Metrics.CloseDetailX").c_str()))
    {
        m_SelectedDomain = EMetricsDomain::None;
        return;
    }

    ImGui::Separator();
    ImGui::Spacing();

    switch (m_SelectedDomain)
    {
    case EMetricsDomain::Performance:
        DrawFrameSummary();
        ImGui::Separator();
        DrawFrameGraph();
        break;

    case EMetricsDomain::Rendering:
        DrawRenderMetrics();
        break;

    case EMetricsDomain::Memory:
        DrawResourceMetrics();
        break;

    case EMetricsDomain::EngineTasks:
        DrawEngineStatus();
        ImGui::Separator();
        DrawAssetTaskMetrics();
        break;

    case EMetricsDomain::Network:
        DrawNetworkMetrics();
        break;

    default:
        break;
    }
}

void MetricsPanel::DrawNetworkMetrics()
{
    auto* sysMgr = &Engine::SystemManager::Get();
    auto* netSystem = sysMgr ? sysMgr->GetSystem<Engine::NetworkSystem>() : nullptr;
    if (!netSystem)
    {
        ImGui::TextDisabled(LOCAL_CSTR("Metrics.NetworkSystemUnavailable"));
        return;
    }

    auto& driver = netSystem->GetDriver();
    ImGui::Text(LOCAL_CSTR("Metrics.NetworkDriverStatus"), driver.IsServer() ? LOCAL_CSTR("NetworkProfiler.ServerMode") : (driver.IsClient() ? LOCAL_CSTR("NetworkProfiler.ClientMode") : LOCAL_CSTR("NetworkProfiler.StandaloneMode")));
    if (driver.IsClient())
    {
        ImGui::BulletText(LOCAL_CSTR("Metrics.SmoothedRttMs"), driver.GetSmoothedRTT());
    }
    else if (driver.IsServer())
    {
        ImGui::BulletText(LOCAL_CSTR("Metrics.HostingOnPort"), driver.GetConfig().ServerPort);
    }
}

void MetricsPanel::Update(f32 dt)
{
    if (m_Paused) return;

    m_AssetTaskMetrics = AssetManager::Get().GetTaskMetrics();
    UpdateResourceMetrics(dt);

    m_RenderMetrics = {};
    if (auto* rhi = Engine::Renderer::Get().GetRHI())
    {
        m_RenderMetrics = rhi->GetLastFrameMetrics();
        m_Capabilities = rhi->GetCapabilities();
    }

    if (std::isfinite(dt) && dt > 0.0f)
        m_ElapsedTime += dt;
    UpdatePassStats();
    UpdateDrawSnapshot(dt);

    m_TargetFPS = Engine::TimeManager::Get().GetTargetFPS();
    m_ActiveSceneCount = Engine::SceneManager::Get().GetActiveScenes().size();
    m_RegisteredPassCount = Engine::RenderPassManager::Get().GetAllRenderPasses().size();

    if (!std::isfinite(dt) || dt <= 0.0f) return;

    const f32 frameMs = dt * 1000.0f;
    if (!std::isfinite(frameMs)) return;

    m_CurrentMs = frameMs;
    m_FrameTimes[m_WriteIndex] = frameMs;
    m_WriteIndex = (m_WriteIndex + 1) % MAX_METRICS_HISTORY;

    if (m_SampleCount < MAX_METRICS_HISTORY)
        ++m_SampleCount;

    RecalculateStatistics();
}

void MetricsPanel::DrawToolbar()
{
    ImGui::Checkbox(LOCAL_LABEL("Metrics.PauseCapture").c_str(), &m_Paused);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", LOCAL_CSTR("Metrics.PauseHint"));

    ImGui::SameLine();
    if (ImGui::Button(LOCAL_LABEL("Metrics.ResetHistory").c_str()))
        ResetHistory();
}

void MetricsPanel::DrawFrameSummary()
{
    if (m_SampleCount == 0)
    {
        ImGui::TextDisabled("%s", LOCAL_CSTR("Metrics.NoSamples"));
        return;
    }

    const f32 averageFPS = m_AverageMs > 0.0f ? 1000.0f / m_AverageMs : 0.0f;
    ImGui::Text("%s", LOCAL_FMT("Metrics.AverageFPS", averageFPS).c_str());

    if (ImGui::BeginTable("FrameStatistics", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextUnformatted(LOCAL_CSTR("Metrics.CurrentFrame"));
        ImGui::TableNextColumn(); ImGui::Text("%.3f ms", m_CurrentMs);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextUnformatted(LOCAL_CSTR("Metrics.Average"));
        ImGui::TableNextColumn(); ImGui::Text("%.3f ms", m_AverageMs);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextUnformatted(LOCAL_CSTR("Metrics.Minimum"));
        ImGui::TableNextColumn(); ImGui::Text("%.3f ms", m_MinMs);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextUnformatted(LOCAL_CSTR("Metrics.Maximum"));
        ImGui::TableNextColumn(); ImGui::Text("%.3f ms", m_MaxMs);

        ImGui::EndTable();
    }

    if (m_TargetFPS > 0)
    {
        ImGui::Text("%s", LOCAL_FMT("Metrics.FPSCap", m_TargetFPS, 1000.0 / static_cast<f64>(m_TargetFPS)).c_str());
    }
    else
    {
        ImGui::TextUnformatted(LOCAL_CSTR("Metrics.FPSUnlimited"));
    }
}

void MetricsPanel::DrawFrameGraph()
{
    if (m_SampleCount == 0) return;

    const int32 offset = (m_SampleCount == MAX_METRICS_HISTORY) ? static_cast<int32>(m_WriteIndex) : 0;
    const f32 scaleMax = (m_MaxMs > 30.0f) ? m_MaxMs * 1.1f : 33.333f;

    ImGui::PlotLines("##FrameTimeHistory", m_FrameTimes.data(), static_cast<int32>(m_SampleCount), offset,
        LOCAL_CSTR("Metrics.FrameInterval"), 0.0f, scaleMax, ImVec2(ImGui::GetContentRegionAvail().x, 130.0f));

    ImGui::Text("%s", LOCAL_FMT("Metrics.RecentFrames", static_cast<int32>(m_SampleCount)).c_str());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(LOCAL_CSTR("Metrics.FrameTimeHint"));
    ImGui::PopTextWrapPos();
}

void MetricsPanel::DrawEngineStatus()
{
    ImGui::TextUnformatted(LOCAL_CSTR("Metrics.EngineState"));
    const auto text = LOCAL_FMT("Metrics.ActiveScenes", m_ActiveSceneCount);
    ImGui::TextUnformatted(text.c_str());
    const auto text2 = LOCAL_FMT("Metrics.RegisteredPasses", m_RegisteredPassCount);
    ImGui::TextUnformatted(text2.c_str());
}

void MetricsPanel::DrawRenderMetrics()
{
    ImGui::TextUnformatted(LOCAL_CSTR("Metrics.RHIDraws"));
    if (!m_DisplaySnapshot.Available)
    {
        ImGui::TextDisabled("%s", LOCAL_CSTR("Metrics.NoCapture"));
        return;
    }
    if (m_DisplaySnapshot.RenderFailed)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "%s", LOCAL_CSTR("Metrics.RenderFailed"));
    }
    const auto& snapshot = m_DisplaySnapshot;
    const auto drawCalls = LOCAL_FMT("Metrics.DrawCalls", snapshot.GetTotalDrawCalls());
    const auto breakdown = LOCAL_FMT("Metrics.DrawBreakdown", snapshot.DrawCalls, snapshot.IndexedDrawCalls);
    const auto vertices = LOCAL_FMT("Metrics.Vertices", snapshot.SubmittedVertices);
    const auto indices = LOCAL_FMT("Metrics.Indices", snapshot.SubmittedIndices);
    ImGui::TextUnformatted(drawCalls.c_str());
    ImGui::TextUnformatted(breakdown.c_str());
    ImGui::TextUnformatted(vertices.c_str());
    ImGui::TextUnformatted(indices.c_str());
    ImGui::Spacing();
    ImGui::Separator();

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("Metrics.RenderTimes"));
    ImGui::Text("%s", LOCAL_FMT("Metrics.TotalCPUSubmit", snapshot.TotalCPUTimeMs).c_str());
    ImGui::Text("%s", LOCAL_FMT("Metrics.TotalGPUExecute", snapshot.TotalGPUTimeMs).c_str());

    if (!m_PassStats.empty())
    {
        if (ImGui::BeginTable("PassMetricsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(LOCAL_LABEL("Metrics.RenderPass").c_str(), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(LOCAL_LABEL("Metrics.CPUTime").c_str(), ImGuiTableColumnFlags_WidthFixed, 65.0f);
            ImGui::TableSetupColumn(LOCAL_LABEL("Metrics.GPUTime").c_str(), ImGuiTableColumnFlags_WidthFixed, 65.0f);
            ImGui::TableHeadersRow();

            const ImVec4 staleColor = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
            for (const auto* renderPass : Engine::RenderPassManager::Get().GetAllRenderPasses())
            {
                const auto found = m_PassStats.find(renderPass->GetName());
                if (found == m_PassStats.end()) continue;

                const auto& stat = found->second;
                const bool stale = (m_ElapsedTime - stat.LastSeenTime) > PassStaleSeconds;

                if (stale) ImGui::PushStyleColor(ImGuiCol_Text, staleColor);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(stat.DisplayName.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f", stat.CPUTimeMs);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", stat.GPUTimeMs);
                if (stale) ImGui::PopStyleColor();
            }
            ImGui::EndTable();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // VRAM 및 디스크립터 통계 (RHIMemoryStats 멤버 바인딩)
    const auto& mem = m_RenderMetrics.MemoryStats;
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("Metrics.VRAMDescriptors"));
    if (mem.DedicatedVRAMBudgetMB > 0)
    {
        const float vramRatio = static_cast<float>(mem.DedicatedVRAMUsageMB) / static_cast<float>(mem.DedicatedVRAMBudgetMB);
        char overlay[64];
        std::snprintf(overlay, sizeof(overlay), "%llu / %llu MB (%.1f%%)", mem.DedicatedVRAMUsageMB, mem.DedicatedVRAMBudgetMB, vramRatio * 100.0f);
        ImGui::ProgressBar(vramRatio, ImVec2(-1.0f, 0.0f), overlay);
    }
    else
    {
        ImGui::Text("%s", LOCAL_FMT("Metrics.DedicatedVRAM", mem.DedicatedVRAMUsageMB).c_str());
    }
    ImGui::Text("%s", LOCAL_FMT("Metrics.AllocatedDescriptors", mem.AllocatedDescriptors).c_str());

    ImGui::Spacing();
    ImGui::Separator();

    // 하드웨어 기능 지원 (RHICapabilities 멤버 바인딩)
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("Metrics.HardwareCapabilities"));
    ImGui::BulletText("%s", LOCAL_FMT("Metrics.ComputeShader", LOCAL(m_Capabilities.SupportsCompute ? "Metrics.Supported" : "Metrics.NotSupported")).c_str());
    ImGui::BulletText("%s", LOCAL_FMT("Metrics.IndirectDraw", LOCAL(m_Capabilities.SupportsIndirectDraw ? "Metrics.Supported" : "Metrics.NotSupported")).c_str());
    ImGui::BulletText("%s", LOCAL_FMT("Metrics.MeshShader", LOCAL(m_Capabilities.SupportsMeshShaders ? "Metrics.SupportedTier1Plus" : "Metrics.NotSupported")).c_str());
    ImGui::BulletText("%s", LOCAL_FMT("Metrics.DXRRaytracing", LOCAL(m_Capabilities.SupportsRaytracing ? "Metrics.SupportedTier1_0Plus" : "Metrics.NotSupported")).c_str());
    ImGui::BulletText("%s", LOCAL_FMT("Metrics.DescriptorIndexing", LOCAL(m_Capabilities.SupportsDescriptorIndexing ? "Metrics.SupportedTier3" : "Metrics.Tier1Or2")).c_str());
}

void MetricsPanel::UpdatePassStats()
{
    if (!m_RenderMetrics.Available) return;

    for (const auto& pass : m_RenderMetrics.PassMetrics)
    {
        const auto [it, inserted] = m_PassStats.try_emplace(pass.PassName);
        auto& stat = it->second;

        if (inserted)
        {
            stat.DisplayName = WStrToStr(pass.PassName);
            stat.CPUTimeMs = pass.CPUTimeMs;
            stat.GPUTimeMs = pass.GPUTimeMs;
        }
        else
        {
            stat.CPUTimeMs += (pass.CPUTimeMs - stat.CPUTimeMs) * PassSmoothing;
            stat.GPUTimeMs += (pass.GPUTimeMs - stat.GPUTimeMs) * PassSmoothing;
        }

        stat.LastSeenTime = m_ElapsedTime;
    }

    std::erase_if(m_PassStats, [this](const auto& pair) { return m_ElapsedTime - pair.second.LastSeenTime > PassHoldSeconds; });
}

void MetricsPanel::UpdateDrawSnapshot(f32 dt)
{
    if (m_RenderMetrics.Available)
    {
        const auto& frame = m_RenderMetrics;
        const bool heavier = !m_WindowPeak.Available || frame.GetTotalDrawCalls() >= m_WindowPeak.GetTotalDrawCalls();
        const bool failed = m_WindowPeak.RenderFailed || !frame.RenderSucceeded;

        if (heavier)
        {
            m_WindowPeak.DrawCalls = frame.DrawCalls;
            m_WindowPeak.IndexedDrawCalls = frame.IndexedDrawCalls;
            m_WindowPeak.SubmittedVertices = frame.SubmittedVertices;
            m_WindowPeak.SubmittedIndices = frame.SubmittedIndices;
            m_WindowPeak.TotalCPUTimeMs = frame.TotalCPUTimeMs;
            m_WindowPeak.TotalGPUTimeMs = frame.TotalGPUTimeMs;
            m_WindowPeak.Available = true;
        }

        m_WindowPeak.RenderFailed = failed;
    }

    if (std::isfinite(dt) && dt > 0.0f)
        m_DrawSnapshotElapsed += dt;

    if (m_DrawSnapshotElapsed >= DrawSnapshotWindowSeconds || !m_DisplaySnapshot.Available)
    {
        if (m_WindowPeak.Available)
            m_DisplaySnapshot = m_WindowPeak;
        m_WindowPeak = {};
        m_DrawSnapshotElapsed = 0.0;
    }
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
    m_WindowPeak = {};
    m_DisplaySnapshot = {};
    m_DrawSnapshotElapsed = 0.0;
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
        if (value < m_MinMs) m_MinMs = value;
        if (value > m_MaxMs) m_MaxMs = value;
    }

    m_AverageMs = static_cast<f32>(totalMs / static_cast<f64>(m_SampleCount));
}

void MetricsPanel::UpdateResourceMetrics(f32 dt)
{
    if (!std::isfinite(dt) || dt <= 0.0f) return;
    m_ResourceRefreshElapsed += dt;
    if (m_ResourceRefreshElapsed < 0.5f) return;
    m_ResourceRefreshElapsed = 0.0f;
    m_ResourceMetrics = Engine::ResourceManager::Get().GetMetrics();
}

void MetricsPanel::DrawResourceMetrics()
{
    const auto summary = LOCAL_FMT("Metrics.ResourceSummary", m_ResourceMetrics.ActiveResources,
        m_ResourceMetrics.TotalSlots, m_ResourceMetrics.FreeSlots);

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(summary.c_str());
    ImGui::PopTextWrapPos();

    static constexpr const char* typeKeys[] = {
        "ResourceType.Unknown", "ResourceType.Texture", "ResourceType.Sprite", "ResourceType.Material",
        "ResourceType.MaterialInstance", "ResourceType.Shader", "ResourceType.Mesh", "ResourceType.Model",
        "ResourceType.Animation", "ResourceType.AudioClip", "ResourceType.Skeleton", "ResourceType.Prefab",
        "ResourceType.Script", "ResourceType.Scene", "ResourceType.Save"
    };

    static_assert(IM_ARRAYSIZE(typeKeys) == Engine::ResourceMetrics::TypeCount);

    if (ImGui::BeginTable("ResourceCounts", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(LOCAL_LABEL("Metrics.Type").c_str());
        ImGui::TableSetupColumn(LOCAL_LABEL("Metrics.Count").c_str());
        ImGui::TableHeadersRow();

        for (std::size_t i = 0; i < m_ResourceMetrics.CountsByType.size(); ++i)
        {
            const auto count = m_ResourceMetrics.CountsByType[i];
            if (count == 0) continue;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(LOCAL_CSTR(typeKeys[i]));
            ImGui::TableNextColumn();
            const auto countText = fmt::format("{}", count);
            ImGui::TextUnformatted(countText.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled("%s", LOCAL_CSTR("Metrics.RefreshInterval"));
}

void MetricsPanel::DrawAssetTaskMetrics()
{
    const auto outstanding = LOCAL_FMT("Metrics.Outstanding", m_AssetTaskMetrics.Outstanding);
    const auto completed = LOCAL_FMT("Metrics.Completed", m_AssetTaskMetrics.GetCompleted());
    const auto succeeded = LOCAL_FMT("Metrics.Succeeded", m_AssetTaskMetrics.Succeeded);
    const auto failed = LOCAL_FMT("Metrics.Failed", m_AssetTaskMetrics.Failed);

    ImGui::TextUnformatted(outstanding.c_str());
    ImGui::TextUnformatted(completed.c_str());
    ImGui::TextUnformatted(succeeded.c_str());

    if (m_AssetTaskMetrics.Failed > 0)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));

    ImGui::TextUnformatted(failed.c_str());

    if (m_AssetTaskMetrics.Failed > 0)
        ImGui::PopStyleColor();

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(LOCAL_CSTR("Metrics.TaskHint"));
    ImGui::PopTextWrapPos();
}