# 추가 시스템

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

다음은 기능을 사용할 때 조사할 시작점이다. 헤더 및 패널 존재를 확인한 목록이며, 세부 사용법을 실행 검증한 것은 아니다.

| 시스템 | 제작 적용 예 | 시작 코드/패널 |
|---|---|---|
| Terrain | 지형 및 레이어 페인팅 | TerrainComponent, TerrainEditor, TerrainLayerEditor |
| Navigation | NPC 경로 탐색 | NavMesh, NavGrid, NavAgentComponent, NavigationSystem |
| Audio | BGM, 효과음, 공간 음원 | AudioSourceComponent, AudioListenerComponent, AudioMixer |
| Gameplay | 체력/피격/상호작용 | HealthComponent, HitboxComponent, InteractableComponent |
| Data Table / Save | 게임 데이터 및 진행 상태 | DataTableEditor, DataTable, SaveGameManager |
| Spline | 이동 경로, 도로, 레일 | SplineComponent, SplineFollowerComponent, SplineMeshComponent |
| Network | 연결 및 동기화 | NetworkSystem, NetworkIdentityComponent, NetworkTransformComponent, NetworkProfilerPanel |
| RL/Simulation | 강화학습 및 반복 시뮬레이션 | RLEnvironment, SceneRLEnvironment, RLServer |
| Sprite baking | 모델을 스프라이트 시트로 변환 | SpriteSheetBaker, ContentBrowser BakeSpriteSheet 경로 |
| Editor 확장 | 커스텀 패널/리소스 편집 | PluginManagerPanel, EditorPanelPluginApi, ResourcePanelPluginApi |

게임에 필요한 기능부터 하나씩 독립 씬에서 검증한다. 예를 들어 3인칭 NPC를 제작하려면 NavMesh Bake 결과, NavAgent 이동, 애니메이션 연결을 각각 확인한 뒤 통합한다. 네트워크 기능은 연결 성공과 권한/예측/동기화 및 실제 멀티플레이 안정성을 별개로 판단한다.

## 관련 문서 및 소스

- [전체 근거 및 검토 범위](Sources.md)

