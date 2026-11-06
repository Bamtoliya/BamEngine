#include "NetworkMetricsWidget.h"
#include "NetworkSystem.h"
#include "SystemManager.h"

bool NetworkMetricsWidget::DrawCard(f32 cardWidth, bool isSelected) const
{
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
    const auto detailLabel = LOCAL(isSelected ? "Metrics.CloseDetail" : "Metrics.ViewDetails") + "###Details";
    const bool clicked = ImGui::SmallButton(detailLabel.c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor();
    return clicked;
}

void NetworkMetricsWidget::DrawDetails() const
{
    auto* sysMgr = &Engine::SystemManager::Get();
    auto* netSystem = sysMgr ? sysMgr->GetSystem<Engine::NetworkSystem>() : nullptr;
    if (!netSystem)
    {
        ImGui::TextDisabled(LOCAL_CSTR("Metrics.NetworkSystemUnavailable"));
        return;
    }

    auto& driver = netSystem->GetDriver();
    ImGui::Text(LOCAL_CSTR("Metrics.NetworkDriverStatus"), driver.IsServer() ? LOCAL_CSTR("NetworkDisplay.DedicatedServer") : (driver.IsClient() ? LOCAL_CSTR("NetworkDisplay.ClientConnected") : LOCAL_CSTR("NetworkDisplay.Standalone")));
    if (driver.IsClient())
    {
        ImGui::BulletText(LOCAL_CSTR("Metrics.SmoothedRttMs"), driver.GetSmoothedRTT());
    }
    else if (driver.IsServer())
    {
        ImGui::BulletText(LOCAL_CSTR("Metrics.HostingOnPort"), driver.GetConfig().ServerPort);
    }
}