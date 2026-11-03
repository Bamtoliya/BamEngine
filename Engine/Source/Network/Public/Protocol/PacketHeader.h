#pragma once

#include <cstdint>

namespace Engine::Network
{
    // 패킷 헤더 (정확히 8바이트 고정, 1바이트 패킹)
#pragma pack(push, 1)
    struct PacketHeader
    {
        static constexpr std::uint16_t ExpectedMagic = 0xBA4D; // 'BM' BamEngine Magic Number
        static constexpr std::uint16_t HeaderSize = 8;
        static constexpr std::uint16_t MaxPayloadSize = 65535 - HeaderSize; // 단일 패킷 최대 크기 64KB 미만

        std::uint16_t Magic = ExpectedMagic; // 매직 넘버 검증
        std::uint16_t PacketId = 0;          // 패킷 메시지 종류 (EPacketId)
        std::uint16_t PayloadSize = 0;       // 헤더 뒤에 이어지는 순수 데이터 크기 (바이트)
        std::uint16_t Sequence = 0;          // 일련번호 (유실 및 순서 역전 감지)

        bool IsValid() const { return Magic == ExpectedMagic && PayloadSize <= MaxPayloadSize; }
    };
#pragma pack(pop)
    static_assert(sizeof(PacketHeader) == 8, "PacketHeader must be exactly 8 bytes!");

    // 엔진 표준 패킷 ID 목록
    enum class EPacketId : std::uint16_t
    {
        None = 0,

        // [1 ~ 9] 연결 세션 라이프사이클
        ConnectRequest = 1,
        ConnectChallenge = 2,
        ConnectAccept = 3,
        ConnectAck = 4,
        HeartbeatPing = 5,
        HeartbeatPong = 6,
        Disconnect = 7,

        // [10 ~ 19] ECS 월드 엔티티 동기화
        SpawnEntity = 10,
        DestroyEntity = 11,
        SyncTransform = 12,
        RPCMessage = 13,

        // [100 ~] 사용자 정의 게임플레이 메시지 시작 번호
        CustomStart = 100
    };
}