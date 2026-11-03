#pragma once

#include "Engine_API.h"
#include "NetworkTypes.h"
#include "NetworkAddress.h"
#include <cstddef>
#include <memory>

namespace Engine::Network
{
    class ENGINE_API INetworkTransport
    {
    public:
        virtual ~INetworkTransport() = default;

        // 전송 계층 초기화 및 정리 (라이브러리 레벨 WSAStartup/WSACleanup 등)
        virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;

        // 소켓 바인드 (서버 또는 UDP 수신용)
        virtual ENetworkResult Bind(const NetworkAddress& address) = 0;

        // TCP 전용: 리스닝 및 클라이언트 접속 수락
        virtual ENetworkResult Listen(std::int32_t backlog = 16) = 0;
        virtual ENetworkResult Accept(std::unique_ptr<INetworkTransport>& outClientTransport, NetworkAddress& outClientAddress) = 0;

        // TCP 전용: 원격지 서버 연결 (논블로킹 Connect 지원)
        virtual ENetworkResult Connect(const NetworkAddress& address) = 0;

        // 연결 지향(TCP) 데이터 송수신
        virtual ENetworkResult Send(const std::uint8_t* data, std::size_t size, std::size_t& outBytesSent) = 0;
        virtual ENetworkResult Receive(std::uint8_t* buffer, std::size_t bufferSize, std::size_t& outBytesReceived) = 0;

        // 비연결형(UDP) 데이터 송수신
        virtual ENetworkResult SendTo(const std::uint8_t* data, std::size_t size, const NetworkAddress& targetAddress, std::size_t& outBytesSent) = 0;
        virtual ENetworkResult ReceiveFrom(std::uint8_t* buffer, std::size_t bufferSize, NetworkAddress& outSenderAddress, std::size_t& outBytesReceived) = 0;

        // 소켓 닫기 및 유효성 확인
        virtual void Close() = 0;
        virtual bool IsValid() const = 0;
        virtual bool IsConnected() const = 0;

        virtual ENetworkProtocol GetProtocol() const = 0;
        virtual NetworkAddress GetLocalAddress() const = 0;
        virtual NetworkAddress GetRemoteAddress() const = 0;
    };
}