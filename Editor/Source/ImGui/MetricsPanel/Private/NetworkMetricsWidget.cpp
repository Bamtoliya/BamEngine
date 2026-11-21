#include "NetworkMetricsWidget.h"
#include "NetworkSystem.h"
#include "SystemManager.h"
#include "MetricsCardUI.h"

bool NetworkMetricsWidget::DrawCard(f32 cardWidth, bool isSelected) const
{
    auto* netSystem = Engine::SystemManager::Get().GetSystem<Engine::NetworkSystem>();
    const bool online = netSystem && netSystem->GetDriver().GetNetworkMode() != Engine::Network::ENetworkMode::Standalone;
    const char* status = online ? "Metrics.Online" : "Metrics.Offline";
    const auto color = online ? ImVec4(0.2f, 0.9f, 0.2f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    return MetricsCardUI::Draw("Card_Network", cardWidth, isSelected, LOCAL_CSTR("Metrics.Network"),
        ImVec4(0.2f, 0.85f, 0.85f, 1.0f), LOCAL_CSTR(status), color, [&]
        {
            if (!online)
            {
                ImGui::TextUnformatted(LOCAL_CSTR("Metrics.ModeStandalone"));
                ImGui::TextDisabled("%s", LOCAL_CSTR("Metrics.TrafficKbS"));
                return;
            }
            if (netSystem->GetDriver().IsClient())
                ImGui::Text(LOCAL_CSTR("Metrics.PingMs"), netSystem->GetDriver().GetSmoothedRTT());
            else
                ImGui::Text(LOCAL_CSTR("Metrics.ServerPort"), netSystem->GetDriver().GetConfig().ServerPort);
            ImGui::TextDisabled("%s", LOCAL_CSTR("Metrics.StatusConnected"));
        });
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
        MetricsCardUI::Bullet(LOCAL_CSTR("Metrics.SmoothedRttMs"), driver.GetSmoothedRTT());
    }
    else if (driver.IsServer())
    {
        MetricsCardUI::Bullet(LOCAL_CSTR("Metrics.HostingOnPort"), driver.GetConfig().ServerPort);
    }
}
