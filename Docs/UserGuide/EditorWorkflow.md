# 에디터 기본 작업

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

`Window → Panels`를 이용해 필요한 패널을 다시 열 수 있다. 아래 목록은 주요 소스/헤더 및 문서에 근거한다.

| 패널 | 사용 목적 |
|---|---|
| Hierarchy | 씬 엔티티와 부모/자식 계층, 생성 및 프리팹 작업 |
| Inspector | 선택 엔티티의 컴포넌트와 속성 변경 |
| Scene View | 월드 배치, 기즈모, 편집 카메라 |
| Game View / Camera Viewport | 게임 화면 및 카메라 결과 확인 |
| UI View | Canvas 및 화면 UI 배치, 상호작용 미리보기 |
| Content Browser | 프로젝트/엔진/마운트 에셋 탐색, 리소스 생성 및 열기 |
| Graph | 로직 및 셰이더 그래프 편집 |
| Tile Palette | 스프라이트 타일과 오토타일 규칙 편집 |
| Animator Graph / Timeline | 애니메이션 상태 및 시간 기반 편집 기능(파일 확인) |
| Post Process | 후처리 편집 기능(파일 확인) |
| Console / Metrics / History | 로그, 성능, 편집 이력 |
| Asset Reference / Build Profile | 참조 확인, 빌드 대상 및 패키징 설정 |

## Scene 카메라

`EditorCameraController`에서 확인한 Perspective 조작:

| 조작 | 기능 |
|---|---|
| Alt + 왼쪽 버튼 드래그 | 피벗 중심 Orbit |
| 가운데 버튼 드래그 | Pan |
| Alt + 오른쪽 버튼 드래그 | 줌 드래그 |
| 휠 | 거리 또는 FOV 줌(설정에 따라), Orthographic에서는 크기 조절 |
| 오른쪽 버튼을 누른 상태에서 휠 | 카메라 이동 속도 조절 |
| 오른쪽 버튼과 WASD/QE | 자유 이동 계열 조작 |

뷰포트에 포커스/호버가 필요할 수 있다. Scene View의 편집 카메라를 움직이는 것과 실제 Camera 엔티티를 움직이는 것은 다르다.

## 저장 및 단축키

- `Scene` 메뉴: New, Save, Save As, Load에 대응하는 작업.
- Undo: Ctrl+Z / Redo: Ctrl+Y 또는 Ctrl+Shift+Z (씬 편집 문맥 기준).
- 텍스트 입력 중에는 텍스트 이력이 우선한다. 단축키는 Settings의 Shortcuts에서 현재 매핑을 확인한다.
- 창 배치, 선택, 편집 카메라 이동 등은 씬 데이터 Undo 대상이 아니다.
- 렌더 타겟을 보고 있다면 Game View의 정상 최종 출력과 디버그 타겟을 구분한다.

## 씬과 엔티티

## 씬 생성

`Scene` 메뉴에서 새 씬을 만든 뒤 `Assets/Scenes/` 하위에 `.bamscene.json`으로 저장한다. 콘텐츠 브라우저의 Create Scene 진입점도 존재한다. 시작 씬은 프로젝트 설정과 `Config/Project.json`의 `StartupScenePath`를 확인한다. 기본 생성 경로는 `Project://Scenes/Main.bamscene.json`이다.

## 엔티티 및 컴포넌트

1. Hierarchy 빈 영역의 우클릭 생성 메뉴에서 Empty, Cube, Sphere, Plane, Cylinder, Capsule, 조명, Sprite, Canvas, Terrain, Camera 등 필요한 항목을 만든다.
2. 엔티티를 선택하고 Inspector 또는 Scene 기즈모에서 위치, 회전, 크기를 조절한다.
3. Inspector에서 필요한 컴포넌트를 추가하고 리소스 핸들·대상 엔티티를 연결한다.
4. 컴포넌트의 Enable 및 엔티티 활성 상태를 확인한다.
5. 씬 저장 후 다시 열어 데이터와 참조가 유지되는지 확인한다.

ECS에서 엔티티의 외형, 물리, 게임 동작은 별도 컴포넌트다. 메시가 존재한다고 충돌이 자동으로 생기는 것은 아니다. 부모/자식 계층의 Transform과 로컬/월드 좌표도 구분한다.

## 관련 문서 및 소스

- [EditorWorkflow.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/UserGuide/EditorWorkflow.md)
- [`ToolBar.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/ToolBar/Private/ToolBar.cpp) — `Editor/Source/ImGui/ToolBar/Private/ToolBar.cpp`
- [`EntityFactory.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/Application/Private/EntityFactory.cpp) — `Editor/Source/Application/Private/EntityFactory.cpp`
- [`EditorCameraController.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/Application/Camera/Private/EditorCameraController.cpp) — `Editor/Source/Application/Camera/Private/EditorCameraController.cpp`
- [`HierarchyPanel.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/HierarchyPanel/Private/HierarchyPanel.cpp) — `Editor/Source/ImGui/HierarchyPanel/Private/HierarchyPanel.cpp`
- [`Scene.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Scene/Public/Scene.h) — `Engine/Source/World/Scene/Public/Scene.h`
- [전체 근거 및 검토 범위](Sources.md)

