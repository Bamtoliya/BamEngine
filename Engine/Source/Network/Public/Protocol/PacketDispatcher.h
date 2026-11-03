#pragma once

#include "Engine_API.h"
#include "PacketHeader.h"
#include "PacketStream.h"
#include "NetworkAddress.h"
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace Engine::Network
{
    // 패킷 핸들러 콜백 함수 시그니처: (헤더, 페이로드 리더, 송신자 주소)
    using PacketHandlerFunc = std::function<void(const PacketHeader& header, PacketReader& reader, const NetworkAddress& sender)>;
    using UnhandledCallback = std::function<void(const PacketHeader& header, const NetworkAddress& sender)>;

    class ENGINE_API PacketDispatcher
    {
    public:
        PacketDispatcher() = default;

        // 특정 패킷 ID에 콜백 핸들러 등록
        void RegisterHandler(EPacketId packetId, PacketHandlerFunc handler);
        void RegisterHandler(std::uint16_t packetId, PacketHandlerFunc handler);
        void UnregisterHandler(EPacketId packetId);
        void UnregisterHandler(std::uint16_t packetId);

        // 등록되지 않은 알 수 없는 패킷 수신 시 콜백
        void SetUnhandledCallback(UnhandledCallback callback) { m_UnhandledCallback = std::move(callback); }

        // 스트림 기반 디스패치 (TCP/UDP 공용: 소비된 바이트 수 outBytesConsumed 반환)
        bool Dispatch(const std::uint8_t* data, std::size_t size, const NetworkAddress& sender, std::size_t& outBytesConsumed);

        // 단일 데이터그램 디스패치 (UDP 전용 편의 함수)
        bool DispatchPacket(const std::uint8_t* data, std::size_t size, const NetworkAddress& sender);

        void ClearHandlers() { m_Handlers.clear(); }

    private:
        std::unordered_map<std::uint16_t, PacketHandlerFunc> m_Handlers;
        UnhandledCallback m_UnhandledCallback;
    };
}