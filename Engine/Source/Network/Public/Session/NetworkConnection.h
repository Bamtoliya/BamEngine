#pragma once

#include "Engine_API.h"
#include "NetworkSessionTypes.h"
#include "NetworkAddress.h"
#include <cstdint>

namespace Engine::Network
{
    class ENGINE_API NetworkConnection
    {
    public:
        NetworkConnection();
        NetworkConnection(ClientId clientId, const NetworkAddress& address);

        ClientId GetClientId() const { return m_ClientId; }
        const NetworkAddress& GetAddress() const { return m_Address; }
        EConnectionState GetState() const { return m_State; }
        void SetState(EConnectionState state) { m_State = state; }

        // 핑 / RTT 및 타임스탬프 갱신
        float GetRTT() const { return m_RoundTripTime; }
        float GetSmoothedRTT() const { return m_SmoothedRTT; }
        float GetLastReceiveTime() const { return m_LastReceiveTime; }
        float GetLastPingSentTime() const { return m_LastPingSentTime; }

        void OnPacketReceived(float currentTime, std::size_t bytes);
        void OnPacketSent(float currentTime, std::size_t bytes);
        void OnPingSent(float currentTime);
        void OnPongReceived(float currentTime, std::uint32_t pingTimestamp);

        bool IsTimedOut(float currentTime, float timeoutThreshold) const;

        // 트래픽 통계
        std::uint64_t GetPacketsSent() const { return m_PacketsSent; }
        std::uint64_t GetPacketsReceived() const { return m_PacketsReceived; }
        std::uint64_t GetBytesSent() const { return m_BytesSent; }
        std::uint64_t GetBytesReceived() const { return m_BytesReceived; }

    private:
        ClientId m_ClientId = InvalidClientId;
        NetworkAddress m_Address;
        EConnectionState m_State = EConnectionState::Disconnected;

        float m_LastReceiveTime = 0.0f;
        float m_LastPingSentTime = 0.0f;
        float m_RoundTripTime = 0.0f;       // 최근 측정된 왕복 시간 (밀리초)
        float m_SmoothedRTT = 0.0f;         // 지수 이동 평균 RTT (밀리초)

        std::uint64_t m_PacketsSent = 0;
        std::uint64_t m_PacketsReceived = 0;
        std::uint64_t m_BytesSent = 0;
        std::uint64_t m_BytesReceived = 0;
    };
}