#include "PacketDispatcher.h"

namespace Engine::Network
{
    void PacketDispatcher::RegisterHandler(EPacketId packetId, PacketHandlerFunc handler)
    {
        RegisterHandler(static_cast<std::uint16_t>(packetId), std::move(handler));
    }

    void PacketDispatcher::RegisterHandler(std::uint16_t packetId, PacketHandlerFunc handler)
    {
        m_Handlers[packetId] = std::move(handler);
    }

    void PacketDispatcher::UnregisterHandler(EPacketId packetId)
    {
        UnregisterHandler(static_cast<std::uint16_t>(packetId));
    }

    void PacketDispatcher::UnregisterHandler(std::uint16_t packetId)
    {
        m_Handlers.erase(packetId);
    }

    bool PacketDispatcher::Dispatch(const std::uint8_t* data, std::size_t size, const NetworkAddress& sender, std::size_t& outBytesConsumed)
    {
        outBytesConsumed = 0;
        if (!data || size < sizeof(PacketHeader))
        {
            return false; // 헤더조차 다 도착하지 않음 (불완전 패킷)
        }

        const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);
        if (!header->IsValid())
        {
            // 매직 넘버 불일치 또는 비정상 페이로드 크기: 스트림 오염 방지를 위해 1바이트 전진 재동기화
            outBytesConsumed = 1;
            return false;
        }

        std::size_t totalPacketSize = sizeof(PacketHeader) + header->PayloadSize;
        if (size < totalPacketSize)
        {
            return false; // 페이로드가 아직 덜 수신됨
        }

        // 온전한 패킷 확보 완료
        PacketReader reader(data + sizeof(PacketHeader), header->PayloadSize);
        auto it = m_Handlers.find(header->PacketId);
        if (it != m_Handlers.end() && it->second)
        {
            it->second(*header, reader, sender);
        }
        else if (m_UnhandledCallback)
        {
            m_UnhandledCallback(*header, sender);
        }

        outBytesConsumed = totalPacketSize;
        return true;
    }

    bool PacketDispatcher::DispatchPacket(const std::uint8_t* data, std::size_t size, const NetworkAddress& sender)
    {
        std::size_t consumed = 0;
        return Dispatch(data, size, sender, consumed) && (consumed == size);
    }
}