# Canvas / UIEventSystem / Widgets

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 구조
Canvas 아래 RectTransform을 사용하고 Image/Text/Button 등의 컴포넌트를 배치한다. UIEventSystem은 scene+canvas에 연결되는 객체이며 보통 엔진 runtime UI 소유자가 입력을 처리한다. 직접 커스텀 canvas를 제어할 때만 별도 instance를 관리한다. 기존 runtime 입력과 중복 소유하지 않는다.

| API | 의미 |
|---|---|
| `Update(const UIPointerInput&) -> bool` | pointer sample 전달 |
| `OnPointerEvent()` | multicast delegate (Enter/Exit/Down/Up/Click/Cancel/ValueChanged) |
| `ResolveTarget(event) -> Entity*` | 삭제/비활성 전후 null 처리 |
| `Navigate(direction,submit)` / `AdjustFocusedSlider(direction)` | 키보드 탐색 |
| `HandleKeyInput(key,mods,clipboardOut,clipboardIn)` | 키 입력 |
| `HandleTextInput(utf8Text)` / `HandleImeComposition(composition,commit)` | 텍스트 및 IME |
| `IsPointerOverUI`, `HasPointerCapture`, `HasFocusedTextInput`, `HasOpenDropdown` | 게임 입력 억제 판단에 활용 |

```cpp
#include "UIEventSystem.h"
Engine::DelegateHandle ObserveClick(Engine::UIEventSystem& ui) {
    return ui.OnPointerEvent().AddLambda([&ui](const Engine::UIPointerEvent& ev) {
        if (ev.type != Engine::EUIPointerEventType::Click) return;
        if (auto* target = ui.ResolveTarget(ev)) { (void)target; }
    });
}
```
소유자는 반환 handle을 보관하고 `ui.OnPointerEvent().Remove(handle)`로 정리한다. UIEventSystem 객체의 수명이 callback보다 길어야 한다.

## 제한 및 검증
장식 이미지의 raycastTarget과 캔버스 Z-order, 버튼 disabled, 포커스 상실/캡처 취소, DPI 및 viewport 변환을 확인한다. UI View Interact는 미리보기이며 실제 게임 콜백 실행과 동일하다고 가정하지 않는다. 드롭다운 텍스트 겹침과 프리팹 무응답은 이전 사용자 보고 사항으로 최신 커밋 재현 필요(A14). [UIInteraction](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/UIInteraction.md) 참조.

## 관련 소스
- [UIEventSystem.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/UI/Public/UIEventSystem.h) — `Engine/Source/UI/Public/UIEventSystem.h`
- [CanvasComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/UI/Public/CanvasComponent.h) — `Engine/Source/UI/Public/CanvasComponent.h`
- [RectTransformComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/UI/Public/RectTransformComponent.h) — `Engine/Source/UI/Public/RectTransformComponent.h`
- [UIButtonComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/UI/Public/UIButtonComponent.h) — `Engine/Source/UI/Public/UIButtonComponent.h`
- [UIDropdownComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/UI/Public/UIDropdownComponent.h) — `Engine/Source/UI/Public/UIDropdownComponent.h`
- [UITextInputComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/UI/Public/UITextInputComponent.h) — `Engine/Source/UI/Public/UITextInputComponent.h`

## 검증 및 참고 예제
- [UIInteractionCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/UIInteractionCheck.cpp)
- [UILayoutCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/UILayoutCheck.cpp)
- [CanvasViewCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/examples/CanvasViewCheck.cpp)
- [EditorUICheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/examples/EditorUICheck.cpp)
