#pragma once

#include "ImGuiInterface.h"
#include <array>
#include <cstdint>
#include <string>

namespace Engine
{
    class NetworkSystem;
}

BEGIN(Editor)

class NetworkProfilerPanel : public IImGui
{
public:
    NetworkProfilerPanel();
    virtual ~NetworkProfilerPanel() override = default;

    virtual void Draw() override;
    virtual void Update(f32 dt) override;
    EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Diagnostics; }
private:
    void DrawSessionControl(Engine::NetworkSystem* netSystem);
    void DrawTrafficMetrics(Engine::NetworkSystem* netSystem);
    void DrawPingGraph(Engine::NetworkSystem* netSystem);
    void DrawConnectedClientsTable(Engine::NetworkSystem* netSystem);

private:
    static constexpr std::size_t MaxPingHistory = 120;
    std::array<float, MaxPingHistory> m_PingHistory{};
    std::size_t m_PingHistoryIndex = 0;

    // 퀵 세션 접속용 입력 버퍼
    char m_ServerAddressBuffer[64] = "127.0.0.1";
    int m_ServerPort = 7777;

    // 초당 대역폭(BPS/PPS) 계산용 통계
    float m_StatAccumulator = 0.0f;
    std::uint64_t m_PrevBytesSent = 0;
    std::uint64_t m_PrevBytesReceived = 0;
    float m_BytesSentPerSec = 0.0f;
    float m_BytesReceivedPerSec = 0.0f;
};

END