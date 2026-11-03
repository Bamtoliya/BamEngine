#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "SocketTransport.h"
#include <cstring>

namespace Engine::Network
{
    SocketTransport::SocketTransport(ENetworkProtocol protocol)
        : m_Protocol(protocol)
    {
    }

    SocketTransport::SocketTransport(std::uintptr_t nativeSocket, ENetworkProtocol protocol, const NetworkAddress& localAddr, const NetworkAddress& remoteAddr)
        : m_Socket(nativeSocket), m_Protocol(protocol), m_LocalAddress(localAddr), m_RemoteAddress(remoteAddr), m_IsConnected(true)
    {
        SetNonBlocking(true);
    }

    SocketTransport::~SocketTransport()
    {
        Close();
    }

    bool SocketTransport::Initialize()
    {
        if (s_WsaInitRefCount.fetch_add(1) == 0)
        {
            WSADATA wsaData;
            if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            {
                s_WsaInitRefCount.fetch_sub(1);
                return false;
            }
        }
        return true;
    }

    void SocketTransport::Shutdown()
    {
        if (s_WsaInitRefCount.load() > 0 && s_WsaInitRefCount.fetch_sub(1) == 1)
        {
            ::WSACleanup();
        }
    }

    bool SocketTransport::OpenSocket(EIPVersion ipVersion)
    {
        if (IsValid()) return true;

        int af = (ipVersion == EIPVersion::IPv6) ? AF_INET6 : AF_INET;
        int type = (m_Protocol == ENetworkProtocol::TCP) ? SOCK_STREAM : SOCK_DGRAM;
        int proto = (m_Protocol == ENetworkProtocol::TCP) ? IPPROTO_TCP : IPPROTO_UDP;

        SOCKET sock = ::socket(af, type, proto);
        if (sock == INVALID_SOCKET) return false;

        m_Socket = static_cast<std::uintptr_t>(sock);
        SetNonBlocking(m_IsNonBlocking);
        return true;
    }

    ENetworkResult SocketTransport::Bind(const NetworkAddress& address)
    {
        if (!address.IsValid()) return ENetworkResult::InvalidAddress;
        if (!IsValid() && !OpenSocket(address.GetIPVersion())) return ENetworkResult::Error;

        SOCKET sock = static_cast<SOCKET>(m_Socket);
        const sockaddr* addrPtr = static_cast<const sockaddr*>(address.GetNativeSockAddr());
        int addrLen = address.GetNativeSockAddrLength();

        if (::bind(sock, addrPtr, addrLen) == SOCKET_ERROR)
        {
            return TranslateSocketError(::WSAGetLastError());
        }

        m_LocalAddress = address;
        return ENetworkResult::Success;
    }

    ENetworkResult SocketTransport::Listen(std::int32_t backlog)
    {
        if (m_Protocol != ENetworkProtocol::TCP || !IsValid()) return ENetworkResult::Error;

        if (::listen(static_cast<SOCKET>(m_Socket), backlog) == SOCKET_ERROR)
        {
            return TranslateSocketError(::WSAGetLastError());
        }
        return ENetworkResult::Success;
    }

    ENetworkResult SocketTransport::Accept(std::unique_ptr<INetworkTransport>& outClientTransport, NetworkAddress& outClientAddress)
    {
        if (m_Protocol != ENetworkProtocol::TCP || !IsValid()) return ENetworkResult::Error;

        sockaddr_storage clientStorage;
        int clientLen = sizeof(clientStorage);
        SOCKET clientSock = ::accept(static_cast<SOCKET>(m_Socket), reinterpret_cast<sockaddr*>(&clientStorage), &clientLen);

        if (clientSock == INVALID_SOCKET)
        {
            return TranslateSocketError(::WSAGetLastError());
        }

        outClientAddress.SetFromNative(&clientStorage, clientLen);
        outClientTransport = std::unique_ptr<SocketTransport>(
            new SocketTransport(static_cast<std::uintptr_t>(clientSock), ENetworkProtocol::TCP, m_LocalAddress, outClientAddress)
        );
        return ENetworkResult::Success;
    }

    ENetworkResult SocketTransport::Connect(const NetworkAddress& address)
    {
        if (m_Protocol != ENetworkProtocol::TCP) return ENetworkResult::Error;
        if (!address.IsValid()) return ENetworkResult::InvalidAddress;
        if (!IsValid() && !OpenSocket(address.GetIPVersion())) return ENetworkResult::Error;

        SOCKET sock = static_cast<SOCKET>(m_Socket);
        const sockaddr* addrPtr = static_cast<const sockaddr*>(address.GetNativeSockAddr());
        int addrLen = address.GetNativeSockAddrLength();

        int result = ::connect(sock, addrPtr, addrLen);
        if (result == 0)
        {
            m_RemoteAddress = address;
            m_IsConnected = true;
            return ENetworkResult::Success;
        }

        int err = ::WSAGetLastError();
        if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS)
        {
            m_RemoteAddress = address;
            m_IsConnected = false; // 논블로킹 연결 진행 중
            return ENetworkResult::WouldBlock;
        }

        return TranslateSocketError(err);
    }

    ENetworkResult SocketTransport::Send(const std::uint8_t* data, std::size_t size, std::size_t& outBytesSent)
    {
        outBytesSent = 0;
        if (!IsValid() || !data || size == 0) return ENetworkResult::Error;

        int sent = ::send(static_cast<SOCKET>(m_Socket), reinterpret_cast<const char*>(data), static_cast<int>(size), 0);
        if (sent == SOCKET_ERROR)
        {
            return TranslateSocketError(::WSAGetLastError());
        }

        outBytesSent = static_cast<std::size_t>(sent);
        return ENetworkResult::Success;
    }

    ENetworkResult SocketTransport::Receive(std::uint8_t* buffer, std::size_t bufferSize, std::size_t& outBytesReceived)
    {
        outBytesReceived = 0;
        if (!IsValid() || !buffer || bufferSize == 0) return ENetworkResult::Error;

        int recvd = ::recv(static_cast<SOCKET>(m_Socket), reinterpret_cast<char*>(buffer), static_cast<int>(bufferSize), 0);
        if (recvd == 0)
        {
            m_IsConnected = false;
            return ENetworkResult::ConnectionClosed;
        }
        if (recvd == SOCKET_ERROR)
        {
            return TranslateSocketError(::WSAGetLastError());
        }

        outBytesReceived = static_cast<std::size_t>(recvd);
        return ENetworkResult::Success;
    }

    ENetworkResult SocketTransport::SendTo(const std::uint8_t* data, std::size_t size, const NetworkAddress& targetAddress, std::size_t& outBytesSent)
    {
        outBytesSent = 0;
        if (!targetAddress.IsValid() || !data || size == 0) return ENetworkResult::InvalidAddress;
        if (!IsValid() && !OpenSocket(targetAddress.GetIPVersion())) return ENetworkResult::Error;

        const sockaddr* addrPtr = static_cast<const sockaddr*>(targetAddress.GetNativeSockAddr());
        int addrLen = targetAddress.GetNativeSockAddrLength();

        int sent = ::sendto(static_cast<SOCKET>(m_Socket), reinterpret_cast<const char*>(data), static_cast<int>(size), 0, addrPtr, addrLen);
        if (sent == SOCKET_ERROR)
        {
            return TranslateSocketError(::WSAGetLastError());
        }

        outBytesSent = static_cast<std::size_t>(sent);
        return ENetworkResult::Success;
    }

    ENetworkResult SocketTransport::ReceiveFrom(std::uint8_t* buffer, std::size_t bufferSize, NetworkAddress& outSenderAddress, std::size_t& outBytesReceived)
    {
        outBytesReceived = 0;
        if (!IsValid() || !buffer || bufferSize == 0) return ENetworkResult::Error;

        sockaddr_storage senderStorage;
        int senderLen = sizeof(senderStorage);

        int recvd = ::recvfrom(static_cast<SOCKET>(m_Socket), reinterpret_cast<char*>(buffer), static_cast<int>(bufferSize), 0, reinterpret_cast<sockaddr*>(&senderStorage), &senderLen);
        if (recvd == SOCKET_ERROR)
        {
            return TranslateSocketError(::WSAGetLastError());
        }

        outSenderAddress.SetFromNative(&senderStorage, senderLen);
        outBytesReceived = static_cast<std::size_t>(recvd);
        return ENetworkResult::Success;
    }

    void SocketTransport::Close()
    {
        if (IsValid())
        {
            ::closesocket(static_cast<SOCKET>(m_Socket));
            m_Socket = s_InvalidSocket;
        }
        m_IsConnected = false;
    }

    bool SocketTransport::SetNonBlocking(bool nonBlocking)
    {
        m_IsNonBlocking = nonBlocking;
        if (!IsValid()) return true;

        u_long mode = nonBlocking ? 1 : 0;
        return (::ioctlsocket(static_cast<SOCKET>(m_Socket), FIONBIO, &mode) == 0);
    }

    bool SocketTransport::SetReuseAddress(bool reuse)
    {
        if (!IsValid()) return false;
        int opt = reuse ? 1 : 0;
        return (::setsockopt(static_cast<SOCKET>(m_Socket), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt)) == 0);
    }

    bool SocketTransport::SetBroadcast(bool broadcast)
    {
        if (!IsValid()) return false;
        int opt = broadcast ? 1 : 0;
        return (::setsockopt(static_cast<SOCKET>(m_Socket), SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&opt), sizeof(opt)) == 0);
    }

    ENetworkResult SocketTransport::TranslateSocketError(std::int32_t errorCode) const
    {
        switch (errorCode)
        {
        case 0: return ENetworkResult::Success;
        case WSAEWOULDBLOCK:
        case WSAEINPROGRESS: return ENetworkResult::WouldBlock;
        case WSAECONNRESET: return ENetworkResult::ConnectionReset;
        case WSAECONNABORTED:
        case WSAESHUTDOWN: return ENetworkResult::ConnectionClosed;
        case WSAETIMEDOUT: return ENetworkResult::TimedOut;
        case WSAEADDRINUSE: return ENetworkResult::AddressInUse;
        case WSAEAFNOSUPPORT:
        case WSAEDESTADDRREQ: return ENetworkResult::InvalidAddress;
        case WSAEMSGSIZE: return ENetworkResult::BufferOverflow;
        case WSAENOTCONN: return ENetworkResult::NotConnected;
        default: return ENetworkResult::Error;
        }
    }
}