#pragma once

#include "Engine_API.h"
#include "NetworkTypes.h"
#include <string>
#include <string_view>
#include <cstdint>

namespace Engine::Network
{
    class ENGINE_API NetworkAddress
    {
    public:
        NetworkAddress();
        NetworkAddress(std::string_view ipString, std::uint16_t port);
        NetworkAddress(std::uint32_t ipv4HostOrder, std::uint16_t port);

        bool IsValid() const { return m_IsValid; }
        EIPVersion GetIPVersion() const { return m_IPVersion; }
        std::string GetIP() const;
        std::uint16_t GetPort() const;
        std::string ToString() const;

        bool operator==(const NetworkAddress& other) const;
        bool operator!=(const NetworkAddress& other) const { return !(*this == other); }
        bool operator<(const NetworkAddress& other) const;

        // 소켓 구현체 전용 내부 버퍼 접근자
        const void* GetNativeSockAddr() const { return m_Storage; }
        void* GetNativeSockAddr() { return m_Storage; }
        std::int32_t GetNativeSockAddrLength() const;
        void SetFromNative(const void* sockaddrPtr, std::int32_t sockaddrLength);

        // 팩토리 유틸리티
        static NetworkAddress Any(std::uint16_t port, EIPVersion version = EIPVersion::IPv4);
        static NetworkAddress Loopback(std::uint16_t port, EIPVersion version = EIPVersion::IPv4);

    private:
        // sockaddr_storage (128 bytes) 격리용 불투명 정렬 버퍼
        alignas(8) std::uint8_t m_Storage[128] = { 0 };
        EIPVersion m_IPVersion = EIPVersion::IPv4;
        bool m_IsValid = false;
    };
}