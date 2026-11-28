# 입력 액션

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

`File → Settings`에서 Input Actions 관련 설정을 찾는다. `InputActionSettingsWidget`에는 카테고리/액션/바인딩 생성 및 변경 UI가 있다. 게임 맵과 에디터 단축키를 구별한다. 새 프로젝트에는 기본 저장소의 입력 설정이 자동으로 모두 복제된다고 가정하지 않는다.

저장소 루트 `Config/InputActions.json`의 Gameplay 예:

| 액션 | 바인딩 |
|---|---|
| MoveForward / MoveBackward / MoveLeft / MoveRight | W / S / A / D |
| Jump | Space |
| Sprint | LShift, RShift |
| OrbitCamera | Mouse:Right |
| Attack | Q |
| Restart | R |

GameUI 맵에는 Tab/Shift+Tab, Enter, Left/Right 바인딩이 있다. 프로젝트에 맞춰 ID와 바인딩을 구성하고 다음 PIE 세션에서 적용 여부를 확인한다.

실제 InputManager 헤더에서 확인한 호출 예(런타임 초기화와 액션 맵 평가가 완료된 문맥):

```cpp
#include "InputManager.h"

bool jumpRequested = Engine::InputManager::Get()
    .WasActionPressed("Gameplay", "Jump");
```

연속 이동과 한 번 발생하는 입력을 분리한다. Fixed Update에서 소비할 경우 `WasActionPressedInFixedUpdate` 등 해당 API를 확인한다. 맵이 평가되지 않는 상태에서는 액션 설정만으로 동작하지 않는다.

## 관련 문서 및 소스

- [`InputActions.json`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Config/InputActions.json) — `Config/InputActions.json`
- [`InputManager.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Input/Public/InputManager.h) — `Engine/Source/Input/Public/InputManager.h`
- [`InputActionSettingsWidget.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/SettingsPanel/Private/InputActionSettingsWidget.cpp) — `Editor/Source/ImGui/SettingsPanel/Private/InputActionSettingsWidget.cpp`
- [전체 근거 및 검토 범위](Sources.md)

