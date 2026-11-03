#pragma once

#include "Engine_API.h"
#include "NetworkSessionTypes.h"
#include "NetworkConnection.h"
#include "NetworkTransportInterface.h"
#include "PacketDispatcher.h"
#include "PacketStream.h"
#include <memory>
#include <unordered_map>
#include <functional>
#include <string>

namespace Engine::Network
{
    class ENGINE_API NetworkDriver
    {
    public:
        using ClientConnectedCallback = std::function<void(ClientId clientId, const NetworkAddress& address)>;
        using ClientDisconnectedCallback = std::function<void(ClientId clientId, EDisconnectReason reason)>;
        using ConnectionStateCallback = std::function<void(EConnectionState newState, EDisconnectReason reason)>;

        NetworkDriver();
        ~NetworkDriver();

        // 서버 모드 시작 (DedicatedServer / ListenServer)
        bool StartServer(std::uint16_t port, const NetworkSessionConfig& config = {});

        // 클라이언트 모드 시작 (원격 서버 접속)
        bool ConnectToServer(const NetworkAddress& serverAddress, const NetworkSessionConfig& config = {});

        // 네트워크 세션 종료 및 모든 소켓 정리
        void Stop(EDisconnectReason reason = EDisconnectReason::Requested);

        // 매 프레임 호출되는 네트워크 업데이트 틱 (수신 폴링, 타임아웃 검출, 핑 전송)
        void Update(float deltaTime);

        // 패킷 송신 API
        bool SendToClient(ClientId clientId, PacketWriter& writer);
        void Broadcast(PacketWriter& writer, ClientId ignoreClient = InvalidClientId);
        bool SendToServer(PacketWriter& writer);

        // 상태 및 조회 API
        ENetworkMode GetNetworkMode() const { return m_Mode; }
        bool IsServer() const { return m_Mode == ENetworkMode::DedicatedServer || m_Mode == ENetworkMode::ListenServer; }
        bool IsClient() const { return m_Mode == ENetworkMode::Client; }
        bool IsConnected() const;

        ClientId GetLocalClientId() const { return m_LocalClientId; }
        const NetworkConnection* GetConnection(ClientId clientId) const;
        float GetSmoothedRTT() const; // 클라이언트 모드 시 서버와의 평균 RTT 반환

        PacketDispatcher& GetDispatcher() { return m_Dispatcher; }
        const NetworkSessionConfig& GetConfig() const { return m_Config; }

        // 이벤트 콜백 등록
        void SetOnClientConnected(ClientConnectedCallback cb) { m_OnClientConnected = std::move(cb); }
        void SetOnClientDisconnected(ClientDisconnectedCallback cb) { m_OnClientDisconnected = std::move(cb); }
        void SetOnConnectionStateChanged(ConnectionStateCallback cb) { m_OnConnectionStateChanged = std::move(cb); }

    private:
        void RegisterInternalHandlers();
        void PollIncomingPackets();
        void UpdateHeartbeat(float deltaTime);
        void CheckTimeouts();

        // 내부 핸드셰이크 및 세션 프로토콜 핸들러
        void HandleConnectRequest(const PacketHeader& header, PacketReader& reader, const NetworkAddress& sender);
        void HandleConnectAccept(const PacketHeader& header, PacketReader& reader, const NetworkAddress& sender);
        void HandleConnectAck(const PacketHeader& header, PacketReader& reader, const NetworkAddress& sender);
        void HandleHeartbeatPing(const PacketHeader& header, PacketReader& reader, const NetworkAddress& sender);
        void HandleHeartbeatPong(const PacketHeader& header, PacketReader& reader, const NetworkAddress& sender);
        void HandleDisconnect(const PacketHeader& header, PacketReader& reader, const NetworkAddress& sender);

        ClientId FindClientIdByAddress(const NetworkAddress& address) const;
        void DisconnectClientInternal(ClientId clientId, EDisconnectReason reason, bool notifyRemote);

    private:
        ENetworkMode m_Mode = ENetworkMode::Standalone;
        NetworkSessionConfig m_Config;
        std::unique_ptr<INetworkTransport> m_Transport;
        PacketDispatcher m_Dispatcher;

        float m_CurrentTime = 0.0f;
        float m_HeartbeatTimer = 0.0f;
        ClientId m_NextClientId = 1;      // 서버 모드: 클라이언트 발급용 (0은 서버)
        ClientId m_LocalClientId = InvalidClientId; // 클라이언트 모드: 서버로부터 발급받은 ID

        // 서버 모드: 클라이언트 접속 목록
        std::unordered_map<ClientId, NetworkConnection> m_Connections;
        std::unordered_map<std::string, ClientId> m_AddressToClient;

        // 클라이언트 모드: 서버와의 단일 연결
        NetworkConnection m_ServerConnection;

        ClientConnectedCallback m_OnClientConnected;
        ClientDisconnectedCallback m_OnClientDisconnected;
        ConnectionStateCallback m_OnConnectionStateChanged;
    };
}