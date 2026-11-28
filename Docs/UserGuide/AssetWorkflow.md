# 에셋과 프리팹

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

## 임포트

`File → Import Asset`을 이용해 모델 및 텍스처를 가져온다. ToolBar 소스에서 FBX, OBJ, glTF, GLB 모델 분기와 PNG, JPG/JPEG 등 텍스처 분기를 확인했다. 나머지 포맷은 현재 파일 선택 필터 및 importer에서 확인한다.

1. 프로젝트 안에 Models, Textures, Materials 등의 작업 폴더를 만든다(권장 분류).
2. 원본 파일을 선택해 임포트하고 옵션 및 출력 경로를 검토한다.
3. 하단 작업 상태 및 알림에서 완료/실패를 확인한다.
4. Content Browser에서 생성된 에셋을 확인하고 모델·머티리얼·텍스처 편집기로 연다.
5. 대상 엔티티의 Inspector에서 리소스 선택기 또는 지원되는 Drag & Drop으로 연결한다.

콘텐츠 브라우저의 생성 메뉴에는 Material, Material Instance, Scene, Prefab, Graph, Timeline, Terrain Layer, Effect, Data Table 생성 진입점이 있다. 소스 원본과 엔진이 실행할 변환 리소스를 구분한다. 임포트된 결과를 파일 탐색기에서 일괄 이동하기보다 참조 영향 확인이 있는 에디터 작업을 우선한다.

다중 드래그 페이로드를 받는 코드가 Content Browser에 있으나 모든 드롭 대상과 컨텍스트 메뉴의 다중 작업 지원을 보장하지 않는다. 여러 파일 작업은 복사본에서 먼저 검증한다.

## 프리팹

**문서 확인, Hierarchy의 프리팹 저장 파일 필터와 배치 알림 경로 코드 확인.**

1. 플레이어 또는 아이템 등의 엔티티 계층과 컴포넌트를 구성한다.
2. Hierarchy에서 루트 선택 → Save As Prefab → `.bamprefab.json`으로 저장.
3. Content Browser의 프리팹을 씬에 배치한다.
4. 프리팹 원본을 열어 전용 편집 화면에서 수정하거나 씬 인스턴스를 개별 수정한다.
5. Apply All / Revert All / Unpack을 사용하기 전 원본과 씬을 저장한다.

현재 프리팹 워크플로 문서는 원본 경로를 가진 복제 스냅샷 방식을 설명한다. 기존 인스턴스 모두가 원본 저장 즉시 자동 갱신되는 Unity식 동작을 전제로 하지 않는다. 외부 엔티티 참조가 있는 Revert 작업은 안전 검사가 필요하며, 원본 및 인스턴스 동기화는 복사본 씬에서 검증한다. 최근 프리팹 열기 문제 및 드롭다운 겹침 문제가 최신 커밋에서 완전히 해소되었는지는 이번 검토에서 실행 확인하지 못했다.

## 관련 문서 및 소스

- [PrefabSceneWorkflowPlan.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/PrefabSceneWorkflowPlan.md)
- [`ToolBar.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/ToolBar/Private/ToolBar.cpp) — `Editor/Source/ImGui/ToolBar/Private/ToolBar.cpp`
- [`HierarchyPanel.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/HierarchyPanel/Private/HierarchyPanel.cpp) — `Editor/Source/ImGui/HierarchyPanel/Private/HierarchyPanel.cpp`
- [`ContentBrowserPanel.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserPanel.cpp) — `Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserPanel.cpp`
- [전체 근거 및 검토 범위](Sources.md)

