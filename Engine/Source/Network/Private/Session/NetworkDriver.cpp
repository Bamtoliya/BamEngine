#include "NetworkDriver.h"
#include "SocketTransport.h"
#include <algorithm>
#include <vector>

namespace Engine::Network
{
    NetworkDriver::NetworkDriver()
    {
        RegisterInternalHandlers();
    }

    NetworkDriver::~NetworkDriver()
    {
        Stop(EDisconnectReason::Requested);
    }

    void NetworkDriver::RegisterInternalHandlers()
    {
        m_Dispatcher.RegisterHandler(EPacketId::ConnectRequest, [this](const PacketHeader& h, PacketReader& r, const NetworkAddress& s) { HandleConnectRequest(h, r, s); });
        m_Dispatcher.RegisterHandler(EPacketId::ConnectAccept, [this](const PacketHeader& h, PacketReader& r, const NetworkAddress& s) { HandleConnectAccept(h, r, s); });
        m_Dispatcher.RegisterHandler(EPacketId::ConnectAck, [this](const PacketHeader& h, PacketReader& r, const NetworkAddress& s) { HandleConnectAck(h, r, s); });
        m_Dispatcher.RegisterHandler(EPacketId::HeartbeatPing, [this](const PacketHeader& h, PacketReader& r, const NetworkAddress& s) { HandleHeartbeatPing(h, r, s); });
        m_Dispatcher.RegisterHandler(EPacketId::HeartbeatPong, [this](const PacketHeader& h, PacketReader& r, const NetworkAddress& s) { HandleHeartbeatPong(h, r, s); });
        m_Dispatcher.RegisterHandler(EPacketId::Disconnect, [this](const PacketHeader& h, PacketReader& r, const NetworkAddress& s) { HandleDisconnect(h, r, s); });
    }

    bool NetworkDriver::StartServer(std::uint16_t port, const NetworkSessionConfig& config)
    {
        Stop();
        m_Config = config;
        m_Config.ServerPort = port;
        m_Mode = ENetworkMode::DedicatedServer;
        m_LocalClientId = ServerClientId;

        m_Transport = std::make_unique<SocketTransport>(ENetworkProtocol::UDP);
        if (!m_Transport->Initialize()) return false;

        NetworkAddress bindAddr = NetworkAddress::Any(port);
        if (m_Transport->Bind(bindAddr) != ENetworkResult::Success)
        {
            m_Transport->Shutdown();
            m_Transport.reset();
            return false;
        }

        return true;
    }

    bool NetworkDriver::ConnectToServer(const NetworkAddress& serverAddress, const NetworkSessionConfig& config)
    {
        Stop();
        m_Config = config;
        m_Mode = ENetworkMode::Client;
        m_LocalClientId = InvalidClientId;

        m_Transport = std::make_unique<SocketTransport>(ENetworkProtocol::UDP);
        if (!m_Transport->Initialize()) return false;

        m_ServerConnection = NetworkConnection(ServerClientId, serverAddress);
        m_ServerConnection.SetState(EConnectionState::Connecting);

        // ConnectRequest 패킷 전송
        PacketWriter writer(EPacketId::ConnectRequest);
        writer.Write<std::uint32_t>(m_Config.ProtocolVersion);
        writer.WriteString("BamClient");

        std::size_t totalSize = 0;
        const std::uint8_t* data = writer.FinalizePacket(totalSize);

        std::size_t sentBytes = 0;
        m_Transport->SendTo(data, totalSize, serverAddress, sentBytes);
        m_ServerConnection.OnPacketSent(m_CurrentTime, sentBytes);

        if (m_OnConnectionStateChanged) m_OnConnectionStateChanged(EConnectionState::Connecting, EDisconnectReason::None);
        return true;
    }

    void NetworkDriver::Stop(EDisconnectReason reason)
    {
        if (m_Mode == ENetworkMode::Standalone) return;

        if (IsServer())
        {
            // 모든 클라이언트에게 Disconnect 패킷 브로드캐스트
            PacketWriter writer(EPacketId::Disconnect);
            writer.Write<std::uint8_t>(static_cast<std::uint8_t>(reason));
            Broadcast(writer);

            m_Connections.clear();
            m_AddressToClient.clear();
        }
        else if (IsClient() && m_ServerConnection.GetState() == EConnectionState::Connected)
        {
            PacketWriter writer(EPacketId::Disconnect);
            writer.Write<std::uint8_t>(static_cast<std::uint8_t>(reason));
            SendToServer(writer);
            m_ServerConnection.SetState(EConnectionState::Disconnected);
        }

        if (m_Transport)
        {
            m_Transport->Close();
            m_Transport->Shutdown();
            m_Transport.reset();
        }

        m_Mode = ENetworkMode::Standalone;
        m_LocalClientId = InvalidClientId;
        m_CurrentTime = 0.0f;
    }

    void NetworkDriver::Update(float deltaTime)
    {
        if (m_Mode == ENetworkMode::Standalone || !m_Transport) return;

        m_CurrentTime += deltaTime;

        PollIncomingPackets();
        UpdateHeartbeat(deltaTime);
        CheckTimeouts();
    }

    void NetworkDriver::PollIncomingPackets()
    {
        std::uint8_t buffer[2048] = { 0 };
        NetworkAddress senderAddr;
        std::size_t bytesReceived = 0;

        // 논블로킹 수신 루프 (WouldBlock을 만날 때까지 모두 폴링)
        while (true)
        {
            ENetworkResult res = m_Transport->ReceiveFrom(buffer, sizeof(buffer), senderAddr, bytesReceived);
            if (res == ENetworkResult::Success && bytesReceived > 0)
            {
                if (IsServer())
                {
                    ClientId cid = FindClientIdByAddress(senderAddr);
                    if (cid != InvalidClientId)
                    {
                        m_Connections[cid].OnPacketReceived(m_CurrentTime, bytesReceived);
                    }
                }
                else if (IsClient() && senderAddr == m_ServerConnection.GetAddress())
                {
                    m_ServerConnection.OnPacketReceived(m_CurrentTime, bytesReceived);
                }

                m_Dispatcher.DispatchPacket(buffer, bytesReceived, senderAddr);
            }
            else
            {
                break; // 읽을 데이터 없음 (WouldBlock) 또는 소켓 에러
            }
        }
    }

    void NetworkDriver::UpdateHeartbeat(float deltaTime)
    {
        m_HeartbeatTimer += deltaTime;
        if (m_HeartbeatTimer < m_Config.HeartbeatInterval) return;
        m_HeartbeatTimer = 0.0f;

        std::uint32_t timestampMs = static_cast<std::uint32_t>(m_CurrentTime * 1000.0f);

        if (IsClient() && m_ServerConnection.GetState() == EConnectionState::Connected)
        {
            PacketWriter ping(EPacketId::HeartbeatPing);
            ping.Write<std::uint32_t>(timestampMs);
            SendToServer(ping);
            m_ServerConnection.OnPingSent(m_CurrentTime);
        }
    }

    void NetworkDriver::CheckTimeouts()
    {
        if (IsServer())
        {
            std::vector<ClientId> timedOutClients;
            for (const auto& [cid, conn] : m_Connections)
            {
                if (conn.IsTimedOut(m_CurrentTime, m_Config.TimeoutThreshold))
                {
                    timedOutClients.push_back(cid);
                }
            }

            for (ClientId cid : timedOutClients)
            {
                DisconnectClientInternal(cid, EDisconnectReason::TimedOut, true);
            }
        }
        else if (IsClient() && m_ServerConnection.GetState() == EConnectionState::Connected)
        {
            if (m_ServerConnection.IsTimedOut(m_CurrentTime, m_Config.TimeoutThreshold))
            {
                m_ServerConnection.SetState(EConnectionState::Disconnected);
                if (m_OnConnectionStateChanged) m_OnConnectionStateChanged(EConnectionState::Disconnected, EDisconnectReason::TimedOut);
            }
        }
    }

    bool NetworkDriver::SendToClient(ClientId clientId, PacketWriter& writer)
    {
        if (!IsServer() || !m_Transport) return false;
        auto it = m_Connections.find(clientId);
        if (it == m_Connections.end() || it->second.GetState() != EConnectionState::Connected) return false;

        std::size_t totalSize = 0;
        const std::uint8_t* data = writer.FinalizePacket(totalSize);

        std::size_t sent = 0;
        ENetworkResult res = m_Transport->SendTo(data, totalSize, it->second.GetAddress(), sent);
        if (res == ENetworkResult::Success)
        {
            it->second.OnPacketSent(m_CurrentTime, sent);
            return true;
        }
        return false;
    }

    void NetworkDriver::Broadcast(PacketWriter& writer, ClientId ignoreClient)
    {
        if (!IsServer() || !m_Transport) return;
        std::size_t totalSize = 0;
        const std::uint8_t* data = writer.FinalizePacket(totalSize);

        for (auto& [cid, conn] : m_Connections)
        {
            if (cid == ignoreClient || conn.GetState() != EConnectionState::Connected) continue;

            std::size_t sent = 0;
            if (m_Transport->SendTo(data, totalSize, conn.GetAddress(), sent) == ENetworkResult::Success)
            {
                conn.OnPacketSent(m_CurrentTime, sent);
            }
        }
    }

    bool NetworkDriver::SendToServer(PacketWriter& writer)
    {
        if (!IsClient() || !m_Transport) return false;
        std::size_t totalSize = 0;
        const std::uint8_t* data = writer.FinalizePacket(totalSize);

        std::size_t sent = 0;
        ENetworkResult res = m_Transport->SendTo(data, totalSize, m_ServerConnection.GetAddress(), sent);
        if (res == ENetworkResult::Success)
        {
            m_ServerConnection.OnPacketSent(m_CurrentTime, sent);
            return true;
        }
        return false;
    }

    bool NetworkDriver::IsConnected() const
    {
        if (IsServer()) return true;
        if (IsClient()) return m_ServerConnection.GetState() == EConnectionState::Connected;
        return false;
    }

    const NetworkConnection* NetworkDriver::GetConnection(ClientId clientId) const
    {
        auto it = m_Connections.find(clientId);
        return (it != m_Connections.end()) ? &it->second : nullptr;
    }

    float NetworkDriver::GetSmoothedRTT() const
    {
        return IsClient() ? m_ServerConnection.GetSmoothedRTT() : 0.0f;
    }

    ClientId NetworkDriver::FindClientIdByAddress(const NetworkAddress& address) const
    {
        auto it = m_AddressToClient.find(address.ToString());
        return (it != m_AddressToClient.end()) ? it->second : InvalidClientId;
    }

    void NetworkDriver::DisconnectClientInternal(ClientId clientId, EDisconnectReason reason, bool notifyRemote)
    {
        auto it = m_Connections.find(clientId);
        if (it == m_Connections.end()) return;

        if (notifyRemote && m_Transport)
        {
            PacketWriter writer(EPacketId::Disconnect);
            writer.Write<std::uint8_t>(static_cast<std::uint8_t>(reason));
            SendToClient(clientId, writer);
        }

        m_AddressToClient.erase(it->second.GetAddress().ToString());
        m_Connections.erase(it);

        if (m_OnClientDisconnected) m_OnClientDisconnected(clientId, reason);
    }

    // =========================================================================
    // 핸드셰이크 & 하트비트 내부 프로토콜 처리
    // =========================================================================
    void NetworkDriver::HandleConnectRequest(const PacketHeader& header, PacketReader& reader, const NetworkAddress& sender)
    {
        if (!IsServer()) return;

        std::uint32_t protocolVersion = 0;
        std::string clientToken;
        reader.Read<std::uint32_t>(protocolVersion);
        reader.ReadString(clientToken);

        // 버전 불일치 또는 정원 초과 검사
        if (protocolVersion != m_Config.ProtocolVersion)
        {
            PacketWriter reject(EPacketId::Disconnect);
            reject.Write<std::uint8_t>(static_cast<std::uint8_t>(EDisconnectReason::VersionMismatch));
            std::size_t sz = 0;
            const std::uint8_t* p = reject.FinalizePacket(sz);
            std::size_t sent = 0;
            m_Transport->SendTo(p, sz, sender, sent);
            return;
        }

        if (m_Connections.size() >= m_Config.MaxClients)
        {
            PacketWriter reject(EPacketId::Disconnect);
            reject.Write<std::uint8_t>(static_cast<std::uint8_t>(EDisconnectReason::ServerFull));
            std::size_t sz = 0;
            const std::uint8_t* p = reject.FinalizePacket(sz);
            std::size_t sent = 0;
            m_Transport->SendTo(p, sz, sender, sent);
            return;
        }

        // 새 ClientId 발급 및 세션 등록
        ClientId assignedId = m_NextClientId++;
        m_Connections[assignedId] = NetworkConnection(assignedId, sender);
        m_AddressToClient[sender.ToString()] = assignedId;

        // ConnectAccept 회신
        PacketWriter accept(EPacketId::ConnectAccept);
        accept.Write<ClientId>(assignedId);
        accept.Write<std::uint32_t>(60); // Server Tick Rate

        std::size_t sz = 0;
        const std::uint8_t* p = accept.FinalizePacket(sz);
        std::size_t sent = 0;
        m_Transport->SendTo(p, sz, sender, sent);
    }

    void NetworkDriver::HandleConnectAccept(const PacketHeader& header, PacketReader& reader, const NetworkAddress& sender)
    {
        if (!IsClient() || sender != m_ServerConnection.GetAddress()) return;

        ClientId assignedId = InvalidClientId;
        std::uint32_t serverTickRate = 0;
        reader.Read<ClientId>(assignedId);
        reader.Read<std::uint32_t>(serverTickRate);

        m_LocalClientId = assignedId;
        m_ServerConnection.SetState(EConnectionState::Connected);

        // ConnectAck 회신
        PacketWriter ack(EPacketId::ConnectAck);
        ack.Write<ClientId>(m_LocalClientId);
        SendToServer(ack);

        if (m_OnConnectionStateChanged) m_OnConnectionStateChanged(EConnectionState::Connected, EDisconnectReason::None);
    }

    void NetworkDriver::HandleConnectAck(const PacketHeader& header, PacketReader& reader, const NetworkAddress& sender)
    {
        if (!IsServer()) return;
        ClientId cid = FindClientIdByAddress(sender);
        if (cid != InvalidClientId)
        {
            m_Connections[cid].SetState(EConnectionState::Connected);
            if (m_OnClientConnected) m_OnClientConnected(cid, sender);
        }
    }

    void NetworkDriver::HandleHeartbeatPing(const PacketHeader& header, PacketReader& reader, const NetworkAddress& sender)
    {
        if (!IsServer()) return;
        std::uint32_t timestamp = 0;
        reader.Read<std::uint32_t>(timestamp);

        // Pong 패킷으로 동일 타임스탬프 반사
        PacketWriter pong(EPacketId::HeartbeatPong);
        pong.Write<std::uint32_t>(timestamp);

        ClientId cid = FindClientIdByAddress(sender);
        if (cid != InvalidClientId)
        {
            SendToClient(cid, pong);
        }
    }

    void NetworkDriver::HandleHeartbeatPong(const PacketHeader& header, PacketReader& reader, const NetworkAddress& sender)
    {
        if (!IsClient() || sender != m_ServerConnection.GetAddress()) return;
        std::uint32_t timestamp = 0;
        reader.Read<std::uint32_t>(timestamp);

        m_ServerConnection.OnPongReceived(m_CurrentTime, timestamp);
    }

    void NetworkDriver::HandleDisconnect(const PacketHeader& header, PacketReader& reader, const NetworkAddress& sender)
    {
        std::uint8_t reasonByte = 0;
        reader.Read<std::uint8_t>(reasonByte);
        EDisconnectReason reason = static_cast<EDisconnectReason>(reasonByte);

        if (IsServer())
        {
            ClientId cid = FindClientIdByAddress(sender);
            if (cid != InvalidClientId)
            {
                DisconnectClientInternal(cid, reason, false);
            }
        }
        else if (IsClient() && sender == m_ServerConnection.GetAddress())
        {
            m_ServerConnection.SetState(EConnectionState::Disconnected);
            if (m_OnConnectionStateChanged) m_OnConnectionStateChanged(EConnectionState::Disconnected, reason);
        }
    }
}