#pragma once

#include "Engine_API.h"
#include "NetworkTypes.h"
#include "PacketStream.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Engine::Network
{
    using uint8 = std::uint8_t;
    using uint32 = std::uint32_t;

    // RPC 타깃 대상 정의
    enum class ERpcTarget : uint8
    {
        Server = 0,    // 클라이언트 -> 서버 실행 요청
        Client = 1,    // 서버 -> 특정/로컬 클라이언트 실행 하달
        Multicast = 2  // 서버 -> 모든 연결 클라이언트 브로드캐스트
    };

    // RPC 패킷 메시지 데이터 구조체
    struct RpcMessage
    {
        uint32 netId = 0;              // 대상 엔티티 NetworkId (0이면 전역 RPC)
        uint32 rpcNameHash = 0;        // RPC 함수명 FNV-1a 32비트 해시
        ERpcTarget target = ERpcTarget::Server;
        uint32 senderClientId = 0;     // 발신 클라이언트 ID (0 = 서버)
        std::vector<std::uint8_t> payload; // RPC 직렬화 인자 바이트 버퍼
    };

    // FNV-1a 32-bit 문자열 해시 유틸리티
    constexpr uint32 HashRpcName(std::string_view name) noexcept
    {
        uint32 hash = 2166136261u;
        for (char c : name)
        {
            hash ^= static_cast<std::uint8_t>(c);
            hash *= 16777619u;
        }
        return hash;
    }

    // RPC 핸들러 콜백 함수 시그니처
    using RpcHandler = std::function<void(const RpcMessage&)>;

    // 등록된 RPC 엔트리
    struct RpcRegistration
    {
        std::string name;
        uint32 nameHash = 0;
        ERpcTarget target = ERpcTarget::Server;
        RpcHandler handler;
    };

    // 네트워크 RPC 레지스트리 및 인코딩/디코딩/디스패치 관리자
    class ENGINE_API NetworkRpcRegistry
    {
    public:
        NetworkRpcRegistry() = default;
        ~NetworkRpcRegistry() = default;

        void SetIsServer(bool isServer) { m_IsServer = isServer; }
        [[nodiscard]] bool IsServer() const { return m_IsServer; }

        // RPC 등록 및 해제
        void RegisterRpc(std::string_view name, ERpcTarget target, RpcHandler handler);
        void UnregisterRpc(std::string_view name);
        void UnregisterRpcByHash(uint32 nameHash);
        [[nodiscard]] bool IsRpcRegistered(std::string_view name) const;
        [[nodiscard]] bool IsRpcRegistered(uint32 nameHash) const;
        [[nodiscard]] const RpcRegistration* FindRegistration(uint32 nameHash) const;
        void Clear();

        // 패킷 스트림 인코딩 / 디코딩
        static void EncodeRpc(const RpcMessage& message, PacketWriter& writer);
        static bool DecodeRpc(PacketReader& reader, RpcMessage& outMessage);

        // 수신된 RPC 디스패치 (권한 검사 수행: Server 타깃은 서버에서만, Client 타깃은 클라이언트에서만 실행)
        bool DispatchIncoming(const RpcMessage& message);
        bool DispatchIncoming(const RpcMessage& message, bool isServer);

    private:
        bool m_IsServer = false;
        std::unordered_map<uint32, RpcRegistration> m_Registrations;
    };
}
