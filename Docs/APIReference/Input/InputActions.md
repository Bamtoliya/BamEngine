# InputManager / Input Actions

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 설정 및 조회
프로젝트 `Config/InputActions.json`에서 map, action, key binding을 관리한다. 저장소 기본 파일에는 Gameplay.MoveForward(W), Jump(Space), Sprint(Shift), Attack(Q), GameUI 등이 있다. 새 프로젝트에서도 자동으로 동일한 입력이 생성된다고 가정하지 않는다.

```cpp
#include "InputManager.h"
void ReadGameplay() {
    auto& input = Engine::InputManager::Get();
    const bool forward = input.IsActionHeld("Gameplay", "MoveForward");
    const bool jump = input.WasActionPressed("Gameplay", "Jump");
    (void)forward; (void)jump;
}
```
`WasActionReleased`, `WasActionPressedInFixedUpdate`, `WasActionReleasedInFixedUpdate`를 용도에 맞게 사용한다. Runtime이 fixed 소비를 담당한다. Update와 fixed 양쪽에서 같은 Jump를 실행하면 중복 처리될 수 있으므로 한 소유 경로를 선택한다.

## 재바인딩
`RebindAction(mapId,actionId,newBindingKey)` 또는 bindingIndex overload, `GetActionBindings`, `ResetActionBindingsToDefault`, `ResetAllBindingsToDefault`, `SaveBindingsToFile`, `LoadBindingsFromFile` 사용. bool 실패를 UI에 반영한다. `EvaluateActionMap`은 map 활성/마우스 입력 평가 경계로 runtime/editor에서 관리할 대상이다.

## 게임패드 및 UI
`IsGamepadConnected`, `GetGamepadLeftStick`, `GetGamepadRightTrigger`, `SetGamepadDeadzone`, `SetGamepadRumble` 지원 선언; 최대 4개 상수. 장치 미연결과 포커스 상실을 처리한다. `GetMousePosition`은 window local, `GetScreenMousePosition`은 desktop, `GetLogicalMousePosition`은 viewport와 target resolution으로 변환한다. 서로 섞으면 고DPI/분리 viewport 오류가 생길 수 있다. UI 입력 소비와 Gameplay action gating을 함께 검증한다.

## 관련 소스
- [InputManager.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Input/Public/InputManager.h) — `Engine/Source/Input/Public/InputManager.h`
- [InputActionSettings.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Input/Public/InputActionSettings.h) — `Engine/Source/Input/Public/InputActionSettings.h`
- [InputActions.json](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Config/InputActions.json) — `Config/InputActions.json`
- [Runtime.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Runtime/Private/Runtime.cpp) — `Engine/Source/Runtime/Private/Runtime.cpp`

## 검증 및 참고 예제
- [InputManagerCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/InputManagerCheck.cpp)
