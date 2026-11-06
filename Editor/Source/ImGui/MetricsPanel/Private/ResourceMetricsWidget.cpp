#include "ResourceMetricsWidget.h"
#include "ResourceManager.h"

#include <cmath>
#include <fmt/format.h>

bool ResourceMetricsWidget::DrawCard(float cardWidth, bool isSelected) const
{
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
    const auto detailLabel = LOCAL(isSelected ? "Metrics.CloseDetail" : "Metrics.ViewDetails") + "###Details";
    const bool clicked = ImGui::SmallButton(detailLabel.c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor();
    return clicked;
}

void ResourceMetricsWidget::Update(f32 dt)
{
    if (!std::isfinite(dt) || dt <= 0.0f) return;
    m_ResourceRefreshElapsed += dt;
    if (m_ResourceRefreshElapsed < 0.5f) return;
    m_ResourceRefreshElapsed = 0.0f;
    m_ResourceMetrics = Engine::ResourceManager::Get().GetMetrics();
}

void ResourceMetricsWidget::DrawDetails() const
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