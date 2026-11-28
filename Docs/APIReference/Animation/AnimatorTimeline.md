# Animator / Action Timeline / Socket

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## Animator
스켈레톤 및 skinned mesh, animation clip/controller의 호환성을 먼저 확보한다. `LoadController(path) -> bool`, `ClearController`, `Play`, `Pause`, `Stop`, `SetTime(time)`, `SetFloat/Int/Bool/Trigger(name,value)`를 지원한다. 레이어는 `PlayLayer(name, ResourceHandle<Animation>, weight=1, isLooping=false)`, `StopLayer`, `SetLayerWeight`, `FindLayer`로 관리한다. 존재하지 않는 이름의 처리 방식은 구현을 추가 확인한다.

```cpp
#include "AnimatorComponent.h"
void UpdateLocomotion(Engine::AnimatorComponent& animator, float speed, bool jump) {
    animator.SetFloat("Speed", speed);
    if (jump) animator.SetTrigger("Jump");
}
```
전제: controller에 Speed/Jump가 해당 타입으로 정의되어 있다. parameter 이름은 프로젝트가 정의한다.

## 액션 타임라인
`ActionTimelineComponent::Play(path="")`, `Pause`, `Resume`, `Stop`, `RegisterNotifyCallback`, `RegisterShotCallback`. timelineAsset/timelinePath와 clip, hitbox window, camera override 실행 상태가 별개다. `SocketAttachmentComponent`는 boneName, offsetPosition, offsetRotation을 제공한다. 무기를 장착할 때 bone 이름·좌표계를 확인한다. 타임라인 notify로 공격 판정 시작/종료를 구현할 수 있지만 각 callback 시그니처 및 hitbox 연동은 구현 추가 검증 필요.

## 검증
state transition, layer weight, 루프 경계 notify, pause/stop/interrupt, camera 복원, root motion과 physics 이동 충돌, scene unload와 callback 해제를 확인한다. 이름 기반 parameter와 bone 참조의 실패 진단/rename 정책은 사용성 개선 후보(A13).

## 관련 소스
- [AnimatorComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Animation/Public/AnimatorComponent.h) — `Engine/Source/Animation/Public/AnimatorComponent.h`
- [AnimationSystem.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Animation/Public/AnimationSystem.h) — `Engine/Source/Animation/Public/AnimationSystem.h`
- [ActionTimelineComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Animation/Timeline/Public/ActionTimelineComponent.h) — `Engine/Source/Animation/Timeline/Public/ActionTimelineComponent.h`

## 검증 및 참고 예제
- [BlendTree2DCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/BlendTree2DCheck.cpp)
- [ActionTimelinePlaybackCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/ActionTimelinePlaybackCheck.cpp)
