# 게임 UI

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

**문서 및 EntityFactory 코드 확인.** Canvas 아래 UI 엔티티 계층을 사용한다.

1. Hierarchy 또는 UI View의 Canvas 메뉴에서 Canvas를 생성한다.
2. Canvas 아래 Image, Button, Text, Text Input, Dropdown 등 필요한 요소를 추가한다.
3. RectTransform에서 위치와 크기, Canvas/레이아웃 설정을 조절한다.
4. 버튼의 interactable, 이미지의 raycastTarget 및 상태별 색상을 설정한다.
5. UI View의 Interact 미리보기로 Hover/Press/Click을 확인한다.
6. 실제 게임 콜백/그래프를 연결한 후 PIE에서 기능을 검사한다.

CanvasScaler, UILayoutGroup, UIScroll, UIClip, UIAlphaMask, UIEllipseMask, RuntimeSettingsMenu 등의 헤더가 존재한다. 각 속성의 편집 및 복합 동작은 최신 UI에서 확인한다.

메인 메뉴의 권장 계층:

```text
MainCanvas
├── Background
├── Title
├── MenuGroup
│   ├── StartButton
│   ├── SettingsButton
│   └── ExitButton
└── SettingsPanel
    ├── ResolutionDropdown
    └── BackButton
```

장식 이미지는 필요하면 raycastTarget을 끈다. 투명 이미지라도 raycastTarget이 켜져 있으면 뒤 버튼 입력을 막을 수 있다. Interact는 UI 동작 미리보기이며 전체 게임 로직 검증은 PIE로 한다. 드롭다운은 여러 해상도와 긴 한글 텍스트에서 겹침/클리핑을 검사한다.

## 관련 문서 및 소스

- [UIInteraction.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/UIInteraction.md)
- [`EntityFactory.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/Application/Private/EntityFactory.cpp) — `Editor/Source/Application/Private/EntityFactory.cpp`
- [전체 근거 및 검토 범위](Sources.md)

