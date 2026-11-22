#include "NetworkRpc.h"
#include <cstring>
#include <algorithm>

namespace Engine::Network
{
    void NetworkRpcRegistry::RegisterRpc(std::string_view name, ERpcTarget target, RpcHandler handler)
    {
        uint32 hash = HashRpcName(name);
        RpcRegistration reg;
        reg.name = std::string(name);
        reg.nameHash = hash;
        reg.target = target;
        reg.handler = std::move(handler);

        m_Registrations[hash] = std::move(reg);
    }

    void NetworkRpcRegistry::UnregisterRpc(std::string_view name)
    {
        uint32 hash = HashRpcName(name);
        UnregisterRpcByHash(hash);
    }

    void NetworkRpcRegistry::UnregisterRpcByHash(uint32 nameHash)
    {
        m_Registrations.erase(nameHash);
    }

    bool NetworkRpcRegistry::IsRpcRegistered(std::string_view name) const
    {
        return IsRpcRegistered(HashRpcName(name));
    }

    bool NetworkRpcRegistry::IsRpcRegistered(uint32 nameHash) const
    {
        return m_Registrations.contains(nameHash);
    }

    const RpcRegistration* NetworkRpcRegistry::FindRegistration(uint32 nameHash) const
    {
        auto it = m_Registrations.find(nameHash);
        return (it != m_Registrations.end()) ? &it->second : nullptr;
    }

    void NetworkRpcRegistry::Clear()
    {
        m_Registrations.clear();
    }

    void NetworkRpcRegistry::EncodeRpc(const RpcMessage& message, PacketWriter& writer)
    {
        writer.Write<uint32>(message.netId);
        writer.Write<uint32>(message.rpcNameHash);
        writer.Write<uint8>(static_cast<uint8>(message.target));
        writer.Write<uint32>(message.senderClientId);

        uint32 payloadSize = static_cast<uint32>(message.payload.size());
        writer.Write<uint32>(payloadSize);
        if (payloadSize > 0)
        {
            writer.WriteBytes(message.payload.data(), payloadSize);
        }
    }

    bool NetworkRpcRegistry::DecodeRpc(PacketReader& reader, RpcMessage& outMessage)
    {
        if (!reader.Read<uint32>(outMessage.netId)) return false;
        if (!reader.Read<uint32>(outMessage.rpcNameHash)) return false;

        uint8 targetRaw = 0;
        if (!reader.Read<uint8>(targetRaw)) return false;
        outMessage.target = static_cast<ERpcTarget>(targetRaw);

        if (!reader.Read<uint32>(outMessage.senderClientId)) return false;

        uint32 payloadSize = 0;
        if (!reader.Read<uint32>(payloadSize)) return false;

        if (payloadSize > reader.GetRemainingBytes())
        {
            return false;
        }

        outMessage.payload.resize(payloadSize);
        if (payloadSize > 0)
        {
            if (!reader.ReadBytes(outMessage.payload.data(), payloadSize))
            {
                return false;
            }
        }

        return true;
    }

    bool NetworkRpcRegistry::DispatchIncoming(const RpcMessage& message)
    {
        return DispatchIncoming(message, m_IsServer);
    }

    bool NetworkRpcRegistry::DispatchIncoming(const RpcMessage& message, bool isServer)
    {
        auto it = m_Registrations.find(message.rpcNameHash);
        if (it == m_Registrations.end())
        {
            return false;
        }

        const auto& reg = it->second;

        // RPC 타깃 불일치 검사
        if (reg.target != message.target)
        {
            return false;
        }

        // 권한 검사:
        // 1. Server RPC: 반드시 서버에서만 실행되어야 함 (클라이언트 수신 시 무시)
        if (reg.target == ERpcTarget::Server && !isServer)
        {
            return false;
        }

        // 2. Client RPC: 클라이언트(또는 리슨 서버의 로컬 클라이언트)에서만 실행 (순수 서버는 타 클라이언트용 RPC 로컬 미실행)
        if (reg.target == ERpcTarget::Client && isServer)
        {
            return false;
        }

        // 3. Multicast RPC: 서버 및 클라이언트 모두 실행 가능

        if (reg.handler)
        {
            reg.handler(message);
            return true;
        }

        return false;
    }
}
