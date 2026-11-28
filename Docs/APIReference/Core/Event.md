# EventBus / Delegate

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 기본 사용
```cpp
#include "EventBus.h"
struct DamageEvent { int amount; };
void Example() {
    Engine::EventBus bus;
    int total = 0;
    auto handle = bus.Subscribe<DamageEvent>([&](const DamageEvent& e) {
        total += e.amount;
    });
    bus.Publish(DamageEvent{10});
    bus.Unsubscribe<DamageEvent>(handle);
}
```
`Publish`는 동기 호출이다. payload 참조는 콜백의 호출 범위에서만 사용한다. `Delegate<Ret, Args...>::Execute`는 미등록 시 runtime_error를 던지고 `ExecuteIfBound`는 반환값을 전달하지 않는다. `MulticastDelegate::Add/AddLambda`는 handle을 반환하고 `Remove`, `Clear`, `Broadcast`를 제공한다.

## 재진입과 소유권
아래 제약은 기준 커밋의 동작이다. 2026-11-28 변경에서는 발행 중 `shared_ptr`로 이벤트 저장소를 유지하고 구독 핸들을 버스에서 발급한다. 재진입 사례는 `Engine/examples/EventBusCheck.cpp`에서 확인할 수 있다.

Broadcast는 listener vector를 snapshot으로 복사하며 호출 직전 현재 연결 여부를 다시 검사한다. 실행 도중 새 구독은 다음 발행부터, 삭제된 구독은 이후 호출에서 제외된다. 단, callback이 버스 자체의 `Reset()` 또는 소유자 파괴를 수행하면 snapshot이 살아 있어도 delegate 객체는 사라질 수 있다. 이를 금지하거나 지연한다(A02).
구독 handle은 event 타입 및 버스에 종속되며 숫자 단독으로 전역 유일하지 않다. DLL unload 이전에 콜백 및 type_index 보관 상태를 제거한다. 스레드 동기화는 공개 구현에 없으므로 동일 소유 스레드에서 사용한다.

## 비용
각 발행에서 snapshot 및 listener별 선형 검색이 발생한다. 다수 리스너의 고빈도 broadcast는 O(N²) 연결 검사 비용이 발생할 수 있다(A08). 측정 전에는 전체 게임의 병목으로 단정하지 않는다.

## 관련 소스
- [EventBus.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Event/Public/EventBus.h) — `Engine/Source/Core/Event/Public/EventBus.h`
- [Delegate.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Event/Public/Delegate.h) — `Engine/Source/Core/Event/Public/Delegate.h`

## 검증 및 참고 예제
- 이번 조사에서 이 API에 직접 대응하는 테스트를 확인하지 못함.
