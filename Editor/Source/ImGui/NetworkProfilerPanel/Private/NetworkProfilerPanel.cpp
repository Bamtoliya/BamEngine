#include "NetworkProfilerPanel.h"
#include "EditorUI.h"
#include "System.h"
#include "NetworkSystem.h"
#include "SystemManager.h"
#include "LocalizationManager.h"
#include <imgui.h>
#include <cstring>
#include <format>
#include <numeric>

struct NetworkProfilerPanel::PluginState
{
    std::array<float, MaxPingHistory> PingHistory;
    std::size_t PingHistoryIndex;
    char ServerAddressBuffer[64];
    int ServerPort;
    float StatAccumulator;
    std::uint64_t PrevBytesSent;
    std::uint64_t PrevBytesReceived;
    float BytesSentPerSec;
    float BytesReceivedPerSec;
};

std::vector<std::byte> NetworkProfilerPanel::CapturePluginState() const
{
    PluginState state{};
    state.PingHistory = m_PingHistory;
    state.PingHistoryIndex = m_PingHistoryIndex;
    std::memcpy(state.ServerAddressBuffer, m_ServerAddressBuffer, sizeof(m_ServerAddressBuffer));
    state.ServerPort = m_ServerPort;
    state.StatAccumulator = m_StatAccumulator;
    state.PrevBytesSent = m_PrevBytesSent;
    state.PrevBytesReceived = m_PrevBytesReceived;
    state.BytesSentPerSec = m_BytesSentPerSec;
    state.BytesReceivedPerSec = m_BytesReceivedPerSec;
    std::vector<std::byte> bytes(sizeof(state));
    std::memcpy(bytes.data(), &state, sizeof(state));
    return bytes;
}

bool NetworkProfilerPanel::RestorePluginState(const void* state, std::size_t stateSize)
{
    if (!state || stateSize != sizeof(PluginState))
        return false;

    PluginState snapshot{};
    std::memcpy(&snapshot, state, sizeof(snapshot));
    if (snapshot.PingHistoryIndex >= MaxPingHistory ||
        std::memchr(snapshot.ServerAddressBuffer, '\0', sizeof(snapshot.ServerAddressBuffer)) == nullptr)
        return false;

    m_PingHistory = snapshot.PingHistory;
    m_PingHistoryIndex = snapshot.PingHistoryIndex;
    std::memcpy(m_ServerAddressBuffer, snapshot.ServerAddressBuffer, sizeof(m_ServerAddressBuffer));
    m_ServerPort = snapshot.ServerPort;
    m_StatAccumulator = snapshot.StatAccumulator;
    m_PrevBytesSent = snapshot.PrevBytesSent;
    m_PrevBytesReceived = snapshot.PrevBytesReceived;
    m_BytesSentPerSec = snapshot.BytesSentPerSec;
    m_BytesReceivedPerSec = snapshot.BytesReceivedPerSec;
    return true;
}

void NetworkProfilerPanel::Update(float dt)
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
    auto* sysMgr = &Engine::SystemManager::Get();
    auto* netSystem = sysMgr ? sysMgr->GetSystem<Engine::NetworkSystem>() : nullptr;

    if (!netSystem)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), LOCAL_CSTR("NetworkProfiler.SystemUnavailable"));
        return;
    }

    DrawSessionControl(netSystem);
    ImGui::Separator();

    DrawTrafficMetrics(netSystem);
    ImGui::Separator();

    DrawSimulationControls(netSystem);
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

}

void NetworkProfilerPanel::DrawSessionControl(Engine::NetworkSystem* netSystem)
{
    auto& driver = netSystem->GetDriver();
    auto mode = driver.GetNetworkMode();

    ImGui::Text(LOCAL_CSTR("NetworkProfiler.SessionMode"));
    ImGui::SameLine();
    if (mode == Engine::Network::ENetworkMode::Standalone)
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), LOCAL_CSTR("NetworkProfiler.Standalone"));
    else if (mode == Engine::Network::ENetworkMode::DedicatedServer)
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), LOCAL_CSTR("NetworkProfiler.ServerRunning"));
    else if (mode == Engine::Network::ENetworkMode::Client)
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), LOCAL_CSTR("NetworkProfiler.ClientConnected"));

    ImGui::Spacing();

    if (mode == Engine::Network::ENetworkMode::Standalone)
    {
        ImGui::InputInt(LOCAL_LABEL("NetworkProfiler.Port").c_str(), &m_ServerPort);
        if (EditorUI::TextButton("HostServer", "NetworkProfiler.HostServer"))
        {
            driver.StartServer(static_cast<std::uint16_t>(m_ServerPort));
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        ImGui::InputText(LOCAL_LABEL("NetworkProfiler.IPAddress").c_str(), m_ServerAddressBuffer, sizeof(m_ServerAddressBuffer));

        EditorUI::TextButtonOptions connectOpt;
        connectOpt.SameLine = true;
        if (EditorUI::TextButton("Connect", "NetworkProfiler.Connect", connectOpt))
        {
            Engine::Network::NetworkAddress targetAddr(m_ServerAddressBuffer, static_cast<std::uint16_t>(m_ServerPort));
            driver.ConnectToServer(targetAddr);
        }
    }
    else
    {
        EditorUI::TextButtonOptions disconnectOpt;
        disconnectOpt.Style = EditorUI::EButtonStyle::Danger;
        if (EditorUI::TextButton("Disconnect", "NetworkProfiler.Disconnect", disconnectOpt))
        {
            driver.Stop(Engine::Network::EDisconnectReason::Requested);
        }
    }
}

void NetworkProfilerPanel::DrawTrafficMetrics(Engine::NetworkSystem* netSystem)
{
    ImGui::Text(LOCAL_CSTR("NetworkProfiler.NetworkTraffic"));
    ImGui::BulletText(LOCAL_CSTR("NetworkProfiler.OutboundKbS"), m_BytesSentPerSec / 1024.0f);
    ImGui::SameLine(240);
    ImGui::BulletText(LOCAL_CSTR("NetworkProfiler.InboundKbS"), m_BytesReceivedPerSec / 1024.0f);
}

void NetworkProfilerPanel::DrawSimulationControls(Engine::NetworkSystem* netSystem)
{
    auto& driver = netSystem->GetDriver();
    auto& sim = driver.GetSimSettings();

    ImGui::TextUnformatted(LOCAL_CSTR("NetworkProfiler.SimulationTitle"));

    ImGui::SetNextItemWidth(180.0f);
    ImGui::SliderFloat(LOCAL_LABEL("NetworkProfiler.SimulatedLatency").c_str(), &sim.simulatedLatencyMs, 0.0f, 500.0f, "%.1f ms");

    ImGui::SetNextItemWidth(180.0f);
    ImGui::SliderFloat(LOCAL_LABEL("NetworkProfiler.SimulatedJitter").c_str(), &sim.simulatedJitterMs, 0.0f, 100.0f, "%.1f ms");

    ImGui::SetNextItemWidth(180.0f);
    ImGui::SliderFloat(LOCAL_LABEL("NetworkProfiler.PacketLoss").c_str(), &sim.simulatedPacketLossPercent, 0.0f, 50.0f, "%.1f %%");

    EditorUI::TextButtonOptions resetOpt;
    resetOpt.SameLine = true;
    if (EditorUI::TextButton("NetSimReset", "EditorCommon.Reset", resetOpt))
    {
        sim.Reset();
    }
}

void NetworkProfilerPanel::DrawPingGraph(Engine::NetworkSystem* netSystem)
{
    float currentRTT = netSystem->GetDriver().GetSmoothedRTT();
    ImGui::Text(LOCAL_CSTR("NetworkProfiler.LatencyRttMs"), currentRTT);

    float maxPing = *std::max_element(m_PingHistory.begin(), m_PingHistory.end());
    maxPing = std::max(100.0f, maxPing * 1.2f);

    std::string overlay = std::format("{:.1f} ms", currentRTT);
    ImGui::PlotLines("##PingPlot", m_PingHistory.data(), static_cast<int>(MaxPingHistory), static_cast<int>(m_PingHistoryIndex), overlay.c_str(), 0.0f, maxPing, ImVec2(-1, 80));
}

void NetworkProfilerPanel::DrawConnectedClientsTable(Engine::NetworkSystem* netSystem)
{
    ImGui::Text(LOCAL_CSTR("NetworkProfiler.ConnectedClients"));
    // ImGuiTableFlags_Resizable 추가
    if (ImGui::BeginTable("ClientsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(LOCAL_LABEL("NetworkProfiler.ClientID").c_str(), ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn(LOCAL_LABEL("NetworkProfiler.RemoteAddress").c_str(), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(LOCAL_LABEL("NetworkProfiler.State").c_str(), ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn(LOCAL_LABEL("NetworkProfiler.PingRtt").c_str(), ImGuiTableColumnFlags_WidthFixed, 90.0f);
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
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), LOCAL_CSTR("NetworkProfiler.Connected"));

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.1f ms", conn->GetSmoothedRTT());
        }

        ImGui::EndTable();
    }
}