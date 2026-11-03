#include "NetworkConnection.h"
#include <algorithm>

namespace Engine::Network
{
    NetworkConnection::NetworkConnection() = default;

    NetworkConnection::NetworkConnection(ClientId clientId, const NetworkAddress& address)
        : m_ClientId(clientId), m_Address(address), m_State(EConnectionState::Connecting)
    {
    }

    void NetworkConnection::OnPacketReceived(float currentTime, std::size_t bytes)
    {
        m_LastReceiveTime = currentTime;
        m_PacketsReceived++;
        m_BytesReceived += bytes;
    }

    void NetworkConnection::OnPacketSent(float currentTime, std::size_t bytes)
    {
        m_PacketsSent++;
        m_BytesSent += bytes;
    }

    void NetworkConnection::OnPingSent(float currentTime)
    {
        m_LastPingSentTime = currentTime;
    }

    void NetworkConnection::OnPongReceived(float currentTime, std::uint32_t pingTimestamp)
    {
        // 핑 패킷 송신 시점의 타임스탬프와 현재 시각의 차이로 RTT 산출 (초 -> 밀리초 변환)
        float rttMs = (currentTime - (static_cast<float>(pingTimestamp) / 1000.0f)) * 1000.0f;
        rttMs = std::max(0.0f, rttMs);

        m_RoundTripTime = rttMs;

        // 지수 이동 평균(EWMA, 가중치 10%)으로 급격한 핑 튐 완화 (RFC 6298 유사)
        if (m_SmoothedRTT <= 0.0f)
        {
            m_SmoothedRTT = rttMs;
        }
        else
        {
            m_SmoothedRTT = (0.9f * m_SmoothedRTT) + (0.1f * rttMs);
        }
    }

    bool NetworkConnection::IsTimedOut(float currentTime, float timeoutThreshold) const
    {
        if (m_State == EConnectionState::Disconnected) return false;
        if (m_LastReceiveTime <= 0.0f) return false; // 아직 패킷을 하나도 받지 않은 초기 연결 단계
        return (currentTime - m_LastReceiveTime) > timeoutThreshold;
    }
}