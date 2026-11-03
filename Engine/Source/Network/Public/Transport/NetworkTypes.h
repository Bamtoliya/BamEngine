#pragma once

#include <cstdint>
#include <string>

namespace Engine::Network
{
    using NetId = std::uint32_t;
    using ClientId = std::uint32_t;

    constexpr NetId InvalidNetId = 0;
    constexpr ClientId ServerClientId = 0;
    constexpr ClientId InvalidClientId = 0xFFFFFFFF;

    // 프로토콜 전송 모드
    enum class ENetworkProtocol : std::uint8_t
    {
        UDP = 0,
        TCP = 1
    };

    // 소켓 및 패킷 송수신 상태 결과 코드
    enum class ENetworkResult : std::int32_t
    {
        Success = 0,
        WouldBlock = 1,         // 논블로킹 소켓에서 즉시 읽을/쓸 데이터가 없음
        ConnectionClosed = 2,    // 상대방이 정상적으로 연결을 종료함 (TCP FIN)
        ConnectionReset = 3,     // 연결이 강제 리셋됨 (RST)
        TimedOut = 4,            // 타임아웃
        AddressInUse = 5,        // 포트 이미 사용 중
        InvalidAddress = 6,      // 잘못된 IP/포트 주소
        BufferOverflow = 7,      // 버퍼 크기 부족
        NotConnected = 8,        // 연결되지 않은 상태에서 송수신 시도
        Error = -1               // 일반 시스템 소켓 오류
    };

    // 주소 계열 (IPv4 / IPv6)
    enum class EIPVersion : std::uint8_t
    {
        IPv4 = 0,
        IPv6 = 1
    };
}