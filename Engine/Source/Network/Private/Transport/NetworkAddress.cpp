#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include "NetworkAddress.h"
#include <cstring>
#include <format>

namespace Engine::Network
{
    NetworkAddress::NetworkAddress()
    {
        std::memset(m_Storage, 0, sizeof(m_Storage));
    }

    NetworkAddress::NetworkAddress(std::string_view ipString, std::uint16_t port)
    {
        std::memset(m_Storage, 0, sizeof(m_Storage));
        std::string ipStr(ipString);

        sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(m_Storage);
        if (::inet_pton(AF_INET, ipStr.c_str(), &addr4->sin_addr) == 1)
        {
            addr4->sin_family = AF_INET;
            addr4->sin_port = ::htons(port);
            m_IPVersion = EIPVersion::IPv4;
            m_IsValid = true;
            return;
        }

        sockaddr_in6* addr6 = reinterpret_cast<sockaddr_in6*>(m_Storage);
        if (::inet_pton(AF_INET6, ipStr.c_str(), &addr6->sin6_addr) == 1)
        {
            addr6->sin6_family = AF_INET6;
            addr6->sin6_port = ::htons(port);
            m_IPVersion = EIPVersion::IPv6;
            m_IsValid = true;
            return;
        }

        m_IsValid = false;
    }

    NetworkAddress::NetworkAddress(std::uint32_t ipv4HostOrder, std::uint16_t port)
    {
        std::memset(m_Storage, 0, sizeof(m_Storage));
        sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(m_Storage);
        addr4->sin_family = AF_INET;
        addr4->sin_addr.s_addr = ::htonl(ipv4HostOrder);
        addr4->sin_port = ::htons(port);
        m_IPVersion = EIPVersion::IPv4;
        m_IsValid = true;
    }

    std::string NetworkAddress::GetIP() const
    {
        if (!m_IsValid) return "0.0.0.0";

        char buffer[INET6_ADDRSTRLEN] = { 0 };
        if (m_IPVersion == EIPVersion::IPv4)
        {
            const sockaddr_in* addr4 = reinterpret_cast<const sockaddr_in*>(m_Storage);
            ::inet_ntop(AF_INET, &(addr4->sin_addr), buffer, sizeof(buffer));
        }
        else
        {
            const sockaddr_in6* addr6 = reinterpret_cast<const sockaddr_in6*>(m_Storage);
            ::inet_ntop(AF_INET6, &(addr6->sin6_addr), buffer, sizeof(buffer));
        }
        return std::string(buffer);
    }

    std::uint16_t NetworkAddress::GetPort() const
    {
        if (!m_IsValid) return 0;
        const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(m_Storage);
        return ::ntohs(addr->sin_port);
    }

    std::string NetworkAddress::ToString() const
    {
        if (m_IPVersion == EIPVersion::IPv6)
        {
            return std::format("[{}]:{}", GetIP(), GetPort());
        }
        return std::format("{}:{}", GetIP(), GetPort());
    }

    bool NetworkAddress::operator==(const NetworkAddress& other) const
    {
        if (m_IsValid != other.m_IsValid || m_IPVersion != other.m_IPVersion) return false;
        if (!m_IsValid) return true;
        return std::memcmp(m_Storage, other.m_Storage, GetNativeSockAddrLength()) == 0;
    }

    bool NetworkAddress::operator<(const NetworkAddress& other) const
    {
        if (m_IsValid != other.m_IsValid) return m_IsValid < other.m_IsValid;
        if (m_IPVersion != other.m_IPVersion) return m_IPVersion < other.m_IPVersion;
        return std::memcmp(m_Storage, other.m_Storage, sizeof(m_Storage)) < 0;
    }

    std::int32_t NetworkAddress::GetNativeSockAddrLength() const
    {
        return (m_IPVersion == EIPVersion::IPv4) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
    }

    void NetworkAddress::SetFromNative(const void* sockaddrPtr, std::int32_t sockaddrLength)
    {
        if (!sockaddrPtr || sockaddrLength <= 0)
        {
            m_IsValid = false;
            return;
        }

        const sockaddr* genericAddr = static_cast<const sockaddr*>(sockaddrPtr);
        if (genericAddr->sa_family == AF_INET && sockaddrLength >= static_cast<std::int32_t>(sizeof(sockaddr_in)))
        {
            std::memcpy(m_Storage, sockaddrPtr, sizeof(sockaddr_in));
            m_IPVersion = EIPVersion::IPv4;
            m_IsValid = true;
        }
        else if (genericAddr->sa_family == AF_INET6 && sockaddrLength >= static_cast<std::int32_t>(sizeof(sockaddr_in6)))
        {
            std::memcpy(m_Storage, sockaddrPtr, sizeof(sockaddr_in6));
            m_IPVersion = EIPVersion::IPv6;
            m_IsValid = true;
        }
        else
        {
            m_IsValid = false;
        }
    }

    NetworkAddress NetworkAddress::Any(std::uint16_t port, EIPVersion version)
    {
        if (version == EIPVersion::IPv4) return NetworkAddress("0.0.0.0", port);
        return NetworkAddress("::", port);
    }

    NetworkAddress NetworkAddress::Loopback(std::uint16_t port, EIPVersion version)
    {
        if (version == EIPVersion::IPv4) return NetworkAddress("127.0.0.1", port);
        return NetworkAddress("::1", port);
    }
}