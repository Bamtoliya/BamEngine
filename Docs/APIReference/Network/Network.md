# Network / RPC / Replication

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Experimental** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 구성 및 사용 경계
NetworkSystem이 Gameplay phase에서 동작한다. 초기화 성공 후 `GetDriver()`, `GetRpcRegistry()`를 사용한다. `AssignNetId(registry,entity,ownerClientId=ServerClientId)`로 identity를 할당할 수 있으며 `RegisterEntity(netId,entity)`, `UnregisterEntity(netId)`, `FindEntityByNetId(netId)`를 제공한다. `entt::entity`는 registry-local이며 네트워크 wire ID로 사용하지 않는다.

```cpp
#include "NetworkSystem.h"
bool Send(Engine::NetworkSystem& network,
          const Engine::Network::RpcMessage& message) {
    return network.SendRpc(message);
}
```
전제: driver 초기화 및 session 연결, RPC 등록 및 권한 설정 완료. `SendRpc(message,targetClientId=InvalidClientId)`의 기본 목적지 의미는 역할별 구현 확인 후 사용한다. GetDriver는 pointer dereference이므로 초기화 전 호출 금지. `RegisterNetworkHandlers`, `UpdateServerSync`, `UpdateClientInterpolation` 등은 시스템 실행 내부 경계다.

## 검증 범위
Protocol/RPC 검사 파일 존재만으로 실제 WAN, 인증, 암호화, anti-cheat 또는 서버 권한 모델이 완료된 것은 아니다. payload 크기 제한, schema version, invalid netId, 권한 없는 RPC, disconnect/reconnect, 지연 및 packet loss 테스트를 별도 수행한다. 매칭/relay/전용 서버 배포는 현재 문서에서 보증하지 않는다(A12).

## 관련 소스
- [NetworkSystem.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Network/Public/NetworkSystem.h) — `Engine/Source/Network/Public/NetworkSystem.h`
- [NetworkIdentityComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Network/Public/NetworkIdentityComponent.h) — `Engine/Source/Network/Public/NetworkIdentityComponent.h`
- [NetworkTransformComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Network/Public/NetworkTransformComponent.h) — `Engine/Source/Network/Public/NetworkTransformComponent.h`
- [NetworkRpc.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Network/Public/NetworkRpc.h) — `Engine/Source/Network/Public/NetworkRpc.h`

## 검증 및 참고 예제
- [NetworkProtocolCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/NetworkProtocolCheck.cpp)
- [NetworkRpcCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/NetworkRpcCheck.cpp)
