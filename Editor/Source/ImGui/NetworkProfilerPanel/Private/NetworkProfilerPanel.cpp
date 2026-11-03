#include "NetworkProfilerPanel.h"
#include "System.h"
#include "NetworkSystem.h"
#include "SystemManager.h"
#include "LocalizationManager.h"
#include <imgui.h>
#include <format>
#include <numeric>

BEGIN(Editor)

NetworkProfilerPanel::NetworkProfilerPanel()
{
    m_Name = L"Network Profiler";
    m_DisplayNameKey = "UI_NETWORK_PROFILER_PANEL";
    m_PingHistory.fill(0.0f);
}

void NetworkProfilerPanel::Update(f32 dt)
{
    m_StatAccumulator += dt;
    if (m_StatAccumulator >= 1.0f)
    {
        auto* sysMgr = &Engine::SystemManager::Get();
        auto* netSystem = sysMgr ? sysMgr->GetSystem<Engine::NetworkSystem>() : nullptr;

        if (netSystem)
        {
            const auto& driver = netSystem->GetDriver();
            std::uint64_t totalBytesSent = 0;
            std::uint64_t totalBytesRecv = 0;

            if (driver.IsClient())
            {
                float currentRtt = driver.GetSmoothedRTT();
                m_PingHistory[m_PingHistoryIndex] = currentRtt;
                m_PingHistoryIndex = (m_PingHistoryIndex + 1) % MaxPingHistory;

                const auto* conn = driver.GetConnection(Engine::Network::ServerClientId);
                if (conn)
                {
                    totalBytesSent = conn->GetBytesSent();
                    totalBytesRecv = conn->GetBytesReceived();
                }
            }

            m_BytesSentPerSec = static_cast<float>(totalBytesSent - m_PrevBytesSent) / m_StatAccumulator;
            m_BytesReceivedPerSec = static_cast<float>(totalBytesRecv - m_PrevBytesReceived) / m_StatAccumulator;
            m_PrevBytesSent = totalBytesSent;
            m_PrevBytesReceived = totalBytesRecv;
        }

        m_StatAccumulator = 0.0f;
    }
}

void NetworkProfilerPanel::Draw()
{
    if (!m_Open) return;

    ImGui::SetNextWindowSize(ImVec2(520, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Network Profiler##Window", &m_Open))
    {
        ImGui::End();
        return;
    }

    auto* sysMgr = &Engine::SystemManager::Get();
    auto* netSystem = sysMgr ? sysMgr->GetSystem<Engine::NetworkSystem>() : nullptr;

    if (!netSystem)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "NetworkSystem is not registered in SystemManager.");
        ImGui::End();
        return;
    }

    DrawSessionControl(netSystem);
    ImGui::Separator();

    DrawTrafficMetrics(netSystem);
    ImGui::Separator();

    if (netSystem->GetDriver().IsClient())
    {
        DrawPingGraph(netSystem);
        ImGui::Separator();
    }
    else if (netSystem->GetDriver().IsServer())
    {
        DrawConnectedClientsTable(netSystem);
        ImGui::Separator();
    }

    ImGui::End();
}

void NetworkProfilerPanel::DrawSessionControl(Engine::NetworkSystem* netSystem)
{
    auto& driver = netSystem->GetDriver();
    auto mode = driver.GetNetworkMode();

    ImGui::Text("Session Mode: ");
    ImGui::SameLine();
    if (mode == Engine::Network::ENetworkMode::Standalone)
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[ Standalone ]");
    else if (mode == Engine::Network::ENetworkMode::DedicatedServer)
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "[ Server Running ]");
    else if (mode == Engine::Network::ENetworkMode::Client)
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "[ Client Connected ]");

    ImGui::Spacing();

    if (mode == Engine::Network::ENetworkMode::Standalone)
    {
        ImGui::InputInt("Port##NetPort", &m_ServerPort);
        if (ImGui::Button("Host Server", ImVec2(120, 26)))
        {
            driver.StartServer(static_cast<std::uint16_t>(m_ServerPort));
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        ImGui::InputText("IP##NetIP", m_ServerAddressBuffer, sizeof(m_ServerAddressBuffer));

        ImGui::SameLine();
        if (ImGui::Button("Connect", ImVec2(100, 26)))
        {
            Engine::Network::NetworkAddress targetAddr(m_ServerAddressBuffer, static_cast<std::uint16_t>(m_ServerPort));
            driver.ConnectToServer(targetAddr);
        }
    }
    else
    {
        if (ImGui::Button("Disconnect / Stop", ImVec2(150, 26)))
        {
            driver.Stop(Engine::Network::EDisconnectReason::Requested);
        }
    }
}

void NetworkProfilerPanel::DrawTrafficMetrics(Engine::NetworkSystem* netSystem)
{
    ImGui::Text("Network Traffic:");
    ImGui::BulletText("Outbound: %.2f KB/s", m_BytesSentPerSec / 1024.0f);
    ImGui::SameLine(240);
    ImGui::BulletText("Inbound: %.2f KB/s", m_BytesReceivedPerSec / 1024.0f);
}

void NetworkProfilerPanel::DrawPingGraph(Engine::NetworkSystem* netSystem)
{
    float currentRTT = netSystem->GetDriver().GetSmoothedRTT();
    ImGui::Text("Latency (RTT): %.1f ms", currentRTT);

    float maxPing = *std::max_element(m_PingHistory.begin(), m_PingHistory.end());
    maxPing = std::max(100.0f, maxPing * 1.2f);

    std::string overlay = std::format("{:.1f} ms", currentRTT);
    ImGui::PlotLines("##PingPlot", m_PingHistory.data(), static_cast<int>(MaxPingHistory), static_cast<int>(m_PingHistoryIndex), overlay.c_str(), 0.0f, maxPing, ImVec2(-1, 80));
}

void NetworkProfilerPanel::DrawConnectedClientsTable(Engine::NetworkSystem* netSystem)
{
    ImGui::Text("Connected Clients:");

    if (ImGui::BeginTable("ClientsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Client ID", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Remote Address", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Ping (RTT)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableHeadersRow();

        // 1번 클라이언트부터 등록된 세션 순회
        for (Engine::Network::ClientId cid = 1; cid < 32; ++cid)
        {
            const auto* conn = netSystem->GetDriver().GetConnection(cid);
            if (!conn || conn->GetState() == Engine::Network::EConnectionState::Disconnected) continue;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("#%u", conn->GetClientId());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", conn->GetAddress().ToString().c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Connected");

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.1f ms", conn->GetSmoothedRTT());
        }

        ImGui::EndTable();
    }
}

END