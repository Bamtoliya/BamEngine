#include "RenderMetricsWidget.h"
#include "Renderer.h"
#include "RHI.h"
#include "RenderPass.h"
#include "RenderPassManager.h"
#include "RaytracingSceneManager.h"
#include <map>
#include <unordered_set>
#include "MetricsCardUI.h"

void RenderMetricsWidget::Update(f32 dt)
{
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
}

void RenderMetricsWidget::ResetHistory()
{
    m_WindowPeak = {};
    m_DisplaySnapshot = {};
    m_DrawSnapshotElapsed = 0.0f;
}

bool RenderMetricsWidget::DrawCard(float cardWidth, bool isSelected) const
{
    const bool failed = m_DisplaySnapshot.RenderFailed;
    const char* status = failed ? "Metrics.Fail" : "Metrics.Ok";
    const auto color = failed ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
    return MetricsCardUI::Draw("Card_Rendering", cardWidth, isSelected, LOCAL_CSTR("Metrics.GpuRendering"),
        ImVec4(0.4f, 0.9f, 0.4f, 1.0f), LOCAL_CSTR(status), color, [&]
        {
            ImGui::Text(LOCAL_CSTR("Metrics.Draws"), m_DisplaySnapshot.GetTotalDrawCalls());
            ImGui::TextDisabled(LOCAL_CSTR("Metrics.GpuTimeMsVerts"), m_DisplaySnapshot.TotalGPUTimeMs,
                m_DisplaySnapshot.SubmittedVertices);
        });
}

void RenderMetricsWidget::DrawDetails() const
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
            const auto drawPassRow = [&](const PassStat& stat)
                {
                    const bool stale = (m_ElapsedTime - stat.LastSeenTime) > PassStaleSeconds;

                    if (stale) ImGui::PushStyleColor(ImGuiCol_Text, staleColor);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(stat.DisplayName.c_str());
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f", stat.CPUTimeMs);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", stat.GPUTimeMs);
                    if (stale) ImGui::PopStyleColor();
                };

            std::unordered_set<std::wstring> registeredNames;
            for (const auto* renderPass : Engine::RenderPassManager::Get().GetAllRenderPasses())
            {
                registeredNames.insert(renderPass->GetName());
                if (const auto found = m_PassStats.find(renderPass->GetName()); found != m_PassStats.end())
                    drawPassRow(found->second);
            }

            // 렌더 패스로 등록되지 않은 계측 구간(가속 구조 빌드, 그림자 마스크 등)은 이름순으로 뒤에 표시한다.
            std::map<std::wstring, const PassStat*> extraStats;
            for (const auto& [name, stat] : m_PassStats)
            {
                if (!registeredNames.contains(name))
                    extraStats.emplace(name, &stat);
            }
            for (const auto& [name, stat] : extraStats)
                drawPassRow(*stat);
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

    if (Engine::RaytracingSceneManager::IsCreated() && Engine::RaytracingSceneManager::Get().GetLastStats().TopLevelCount > 0)
    {
        const auto& raytracing = Engine::RaytracingSceneManager::Get().GetLastStats();
        constexpr double bytesPerMB = 1024.0 * 1024.0;
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("Metrics.RaytracingStats"));
        ImGui::TextUnformatted(LOCAL_FMT("Metrics.RaytracingInstances", raytracing.TopLevelCount, raytracing.InstanceCount).c_str());
        ImGui::TextUnformatted(LOCAL_FMT("Metrics.RaytracingMemory", raytracing.BottomLevelCount,
            static_cast<double>(raytracing.BottomLevelBytes) / bytesPerMB, static_cast<double>(raytracing.TopLevelBytes) / bytesPerMB).c_str());
        ImGui::TextUnformatted(LOCAL_FMT("Metrics.RaytracingFallback", raytracing.UnsupportedCasterCount).c_str());
        if (raytracing.PendingCasterCount > 0)
            ImGui::TextUnformatted(LOCAL_FMT("Metrics.RaytracingPending", raytracing.PendingCasterCount).c_str());
        if (raytracing.FailedCasterCount > 0)
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "%s", LOCAL_FMT("Metrics.RaytracingFailed", raytracing.FailedCasterCount).c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();

    // 하드웨어 기능 지원 (RHICapabilities 멤버 바인딩)
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("Metrics.HardwareCapabilities"));
    if (!m_Capabilities.AdapterName.empty())
        MetricsCardUI::Bullet("%s", LOCAL_FMT("Metrics.GraphicsAdapter", WStrToStr(m_Capabilities.AdapterName)).c_str());
    if (m_Capabilities.HighestShaderModel != 0)
        MetricsCardUI::Bullet("%s", LOCAL_FMT("Metrics.ShaderModel", (m_Capabilities.HighestShaderModel >> 4) & 0xF, m_Capabilities.HighestShaderModel & 0xF).c_str());
    MetricsCardUI::Bullet("%s", LOCAL_FMT("Metrics.ComputeShader", LOCAL(m_Capabilities.SupportsCompute ? "Metrics.Supported" : "Metrics.NotSupported")).c_str());
    MetricsCardUI::Bullet("%s", LOCAL_FMT("Metrics.IndirectDraw", LOCAL(m_Capabilities.SupportsIndirectDraw ? "Metrics.Supported" : "Metrics.NotSupported")).c_str());
    MetricsCardUI::Bullet("%s", LOCAL_FMT("Metrics.MeshShader", LOCAL(m_Capabilities.SupportsMeshShaders ? "Metrics.SupportedTier1Plus" : "Metrics.NotSupported")).c_str());
    const char* raytracingKey = "Metrics.NotSupported";
    if (m_Capabilities.SupportsInlineRaytracing)
        raytracingKey = "Metrics.SupportedTier1_1Inline";
    else if (m_Capabilities.SupportsRaytracing)
        raytracingKey = "Metrics.SupportedTier1_0Plus";
    MetricsCardUI::Bullet("%s", LOCAL_FMT("Metrics.DXRRaytracing", LOCAL(raytracingKey)).c_str());
    MetricsCardUI::Bullet("%s", LOCAL_FMT("Metrics.DescriptorIndexing", LOCAL(m_Capabilities.SupportsDescriptorIndexing ? "Metrics.SupportedTier3" : "Metrics.Tier1Or2")).c_str());
}

void RenderMetricsWidget::UpdatePassStats()
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

void RenderMetricsWidget::UpdateDrawSnapshot(f32 dt)
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
