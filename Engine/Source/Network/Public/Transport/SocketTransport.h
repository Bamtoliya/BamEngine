#pragma once

#include "Engine_API.h"
#include "NetworkTransportInterface.h"
#include <atomic>
#include <cstdint>

namespace Engine::Network
{
    class ENGINE_API SocketTransport : public INetworkTransport
    {
    public:
        explicit SocketTransport(ENetworkProtocol protocol = ENetworkProtocol::UDP);
        virtual ~SocketTransport() override;

        // 라이브러리 차원 WSAStartup / WSACleanup 참조 카운트 관리
        virtual bool Initialize() override;
        virtual void Shutdown() override;

        // 소켓 생성 및 주소 바인딩
        virtual ENetworkResult Bind(const NetworkAddress& address) override;

        // TCP 전용 리스닝 및 수락
        virtual ENetworkResult Listen(std::int32_t backlog = 16) override;
        virtual ENetworkResult Accept(std::unique_ptr<INetworkTransport>& outClientTransport, NetworkAddress& outClientAddress) override;

        // TCP 전용 연결
        virtual ENetworkResult Connect(const NetworkAddress& address) override;

        // 연결 지향(TCP) 송수신
        virtual ENetworkResult Send(const std::uint8_t* data, std::size_t size, std::size_t& outBytesSent) override;
        virtual ENetworkResult Receive(std::uint8_t* buffer, std::size_t bufferSize, std::size_t& outBytesReceived) override;

        // 비연결형(UDP) 송수신
        virtual ENetworkResult SendTo(const std::uint8_t* data, std::size_t size, const NetworkAddress& targetAddress, std::size_t& outBytesSent) override;
        virtual ENetworkResult ReceiveFrom(std::uint8_t* buffer, std::size_t bufferSize, NetworkAddress& outSenderAddress, std::size_t& outBytesReceived) override;

        // 소켓 제어
        virtual void Close() override;
        virtual bool IsValid() const override { return m_Socket != s_InvalidSocket; }
        virtual bool IsConnected() const override { return m_IsConnected; }

        virtual ENetworkProtocol GetProtocol() const override { return m_Protocol; }
        virtual NetworkAddress GetLocalAddress() const override { return m_LocalAddress; }
        virtual NetworkAddress GetRemoteAddress() const override { return m_RemoteAddress; }

        // 소켓 옵션 설정
        bool SetNonBlocking(bool nonBlocking);
        bool SetReuseAddress(bool reuse);
        bool SetBroadcast(bool broadcast);

    private:
        // Accept로 생성된 소켓을 감싸는 내부 전용 생성자
        SocketTransport(std::uintptr_t nativeSocket, ENetworkProtocol protocol, const NetworkAddress& localAddr, const NetworkAddress& remoteAddr);

        bool OpenSocket(EIPVersion ipVersion);
        ENetworkResult TranslateSocketError(std::int32_t errorCode) const;

    private:
        static constexpr std::uintptr_t s_InvalidSocket = ~static_cast<std::uintptr_t>(0);

        std::uintptr_t m_Socket = s_InvalidSocket;
        ENetworkProtocol m_Protocol = ENetworkProtocol::UDP;
        NetworkAddress m_LocalAddress;
        NetworkAddress m_RemoteAddress;
        bool m_IsConnected = false;
        bool m_IsNonBlocking = true;

        static inline std::atomic<std::int32_t> s_WsaInitRefCount{ 0 };
    };
}