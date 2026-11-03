#pragma once

#include "NetworkTypes.h"
#include <cstdint>

namespace Engine::Network
{
    // 네트워크 실행 모드
    enum class ENetworkMode : std::uint8_t
    {
        Standalone = 0,     // 단독 오프라인 모드 (네트워크 비활성)
        DedicatedServer = 1,// 그래픽/오디오 없는 순수 호스팅 서버
        ListenServer = 2,   // 서버 호스트 겸 로컬 플레이어 동시 구동
        Client = 3          // 원격 서버에 접속하는 클라이언트
    };

    // 개별 클라이언트 세션 연결 상태
    enum class EConnectionState : std::uint8_t
    {
        Disconnected = 0,   // 연결 해제됨
        Connecting = 1,     // 3-Way 핸드셰이크 진행 중
        Connected = 2,      // 정상 연결 및 상호 통신 가능 상태
        Disconnecting = 3   // 연결 종료 진행 중
    };

    // 연결 종료 사유
    enum class EDisconnectReason : std::uint8_t
    {
        None = 0,
        Requested = 1,      // 사용자가 정상적으로 접속 종료 요청
        TimedOut = 2,       // 하트비트 응답 없음 (네트워크 단선/타임아웃)
        ServerFull = 3,     // 서버 최대 인원 초과
        VersionMismatch = 4,// 프로토콜 또는 게임 버전 불일치
        Kicked = 5,         // 서버 관리자에 의해 퇴장됨
        ConnectionLost = 6  // 소켓 리셋 등 비정상 소켓 오류
    };

    // 네트워크 세션 런타임 설정
    struct NetworkSessionConfig
    {
        float HeartbeatInterval = 1.0f;     // 하트비트 Ping 전송 주기 (초)
        float TimeoutThreshold = 10.0f;     // 타임아웃 판정 임계치 (초)
        std::uint32_t MaxClients = 32;      // 최대 동시 접속 허용 클라이언트 수
        std::uint32_t ProtocolVersion = 1;  // 프로토콜 호환성 버전
        std::uint16_t ServerPort = 7777;    // 기본 수신 포트
    };
}