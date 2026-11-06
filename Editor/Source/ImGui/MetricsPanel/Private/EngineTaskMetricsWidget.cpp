#include "EngineTaskMetricsWidget.h"
#include "AssetManager.h"
#include "RenderPassManager.h"
#include "SceneManager.h"

void EngineTaskMetricsWidget::Update()
{
    m_AssetTaskMetrics = AssetManager::Get().GetTaskMetrics();
    m_ActiveSceneCount = Engine::SceneManager::Get().GetActiveScenes().size();
    m_RegisteredPassCount = Engine::RenderPassManager::Get().GetAllRenderPasses().size();
}

void EngineTaskMetricsWidget::DrawDetails() const
{
    DrawEngineStatus();
    ImGui::Separator();
    DrawAssetTaskMetrics();
}

bool EngineTaskMetricsWidget::DrawCard(f32 cardWidth, bool isSelected) const
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, isSelected ? ImVec4(0.15f, 0.25f, 0.35f, 0.8f) : ImVec4(0.14f, 0.14f, 0.16f, 0.7f));
    ImGui::BeginChild("Card_EngineTasks", ImVec2(cardWidth, 125.0f), true, ImGuiWindowFlags_NoScrollbar);

    ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), LOCAL_CSTR("Metrics.TasksEngine"));
    ImGui::SameLine(cardWidth - 75.0f);
    if (m_AssetTaskMetrics.Outstanding > 0) ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), LOCAL_CSTR("Metrics.Busy"));
    else ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), LOCAL_CSTR("Metrics.Idle"));

    ImGui::Text(LOCAL_CSTR("Metrics.PassesScenes"), m_RegisteredPassCount, m_ActiveSceneCount);
    ImGui::TextDisabled(LOCAL_CSTR("Metrics.TasksActiveDone"), m_AssetTaskMetrics.Outstanding, m_AssetTaskMetrics.GetCompleted());

    ImGui::Spacing();
    const auto detailLabel = LOCAL(isSelected ? "Metrics.CloseDetail" : "Metrics.ViewDetails") + "###Details";
    const bool clicked = ImGui::SmallButton(detailLabel.c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor();
    return clicked;
}

void EngineTaskMetricsWidget::DrawEngineStatus() const
{
    ImGui::TextUnformatted(LOCAL_CSTR("Metrics.EngineState"));
    const auto text = LOCAL_FMT("Metrics.ActiveScenes", m_ActiveSceneCount);
    ImGui::TextUnformatted(text.c_str());
    const auto text2 = LOCAL_FMT("Metrics.RegisteredPasses", m_RegisteredPassCount);
    ImGui::TextUnformatted(text2.c_str());
}

void EngineTaskMetricsWidget::DrawAssetTaskMetrics() const
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