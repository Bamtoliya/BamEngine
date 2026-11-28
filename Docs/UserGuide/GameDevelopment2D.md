# 2D 게임 제작

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

## 스프라이트

1. PNG 등의 텍스처를 임포트하고 Sprite Editor에서 텍스처를 연다.
2. Slice 메뉴에서 Grid by Cell Size / Grid by Cell Count / Automatic 중 선택한다.
3. 셀 크기 또는 개수, Offset, Padding, Alpha Threshold, Pivot, Pixels Per Unit을 맞춘다.
4. 생성된 조각의 영역과 피벗을 검사하고 저장한다.
5. Sprite 엔티티의 SpriteRenderer에 할당한다. Orthographic 카메라로 결과를 본다.
6. Sprite Animation 메뉴에서 Single 또는 Row 방식으로 클립을 생성하고 FPS/재생 방식을 설정한다.

`SpriteEditor.cpp`에서 위 UI 경로 및 조각 Rect 편집을 확인했다. 별도의 드래그 선택 영역에만 전체 자동 슬라이스를 적용하는 기능은 조사한 경로에서 확정하지 못했다. 필요하다면 개별 조각 Rect 편집, 원본 이미지 별도 분할 또는 추가 구현을 검토한다.

## Tilemap과 4방향 Autotile

TilemapComponent를 사용할 엔티티를 구성하고 적절한 Mesh/Material, Cell Size, Sorting Layer, Order In Layer를 확인한다. Tile Palette에 스프라이트를 드래그한다. Tilemap 엔티티를 선택한 상태로 브러시를 사용한다.

Autotile 모드:

1. Tile Palette에서 Autotile 도구를 선택한다.
2. 마스크 슬라이더(0~15)와 선택 스프라이트를 사용하여 Assign한다.
3. 먼저 마스크 0(고립 타일)을 지정한다. `SetAutotile`은 이 스프라이트가 유효하지 않으면 실패한다.
4. 나머지 연결 조합을 모두 지정하고 인접 칸을 칠하거나 지워 갱신을 확인한다.

코드에서 확인한 좌표 비트: `+Y = 1`, `+X = 2`, `-Y = 4`, `-X = 8`. 아래는 +Y를 위로 보는 월드 기준이다(텍스처 이미지의 Y 방향과 혼동하지 않는다).

| 마스크 | 연결 방향 | 형태 힌트 |
|---:|---|---|
| 0 | 없음 | 고립 |
| 1 | 위 | 위로만 연결 |
| 2 | 오른쪽 | 오른쪽으로만 연결 |
| 3 | 위+오른쪽 | 코너 |
| 4 | 아래 | 아래로만 연결 |
| 5 | 위+아래 | 세로 통로 |
| 6 | 오른쪽+아래 | 코너 |
| 7 | 위+오른쪽+아래 | T자 |
| 8 | 왼쪽 | 왼쪽으로만 연결 |
| 9 | 위+왼쪽 | 코너 |
| 10 | 오른쪽+왼쪽 | 가로 통로 |
| 11 | 위+오른쪽+왼쪽 | T자 |
| 12 | 아래+왼쪽 | 코너 |
| 13 | 위+아래+왼쪽 | T자 |
| 14 | 오른쪽+아래+왼쪽 | T자 |
| 15 | 모든 방향 | 내부/교차 |

**주의:** 일반 타일이 아니라 `isAutotile`인 이웃만 연결 마스크에 포함된다. 16×16은 타일맵 내부 청크 크기이지 스프라이트의 픽셀 크기가 아니다. 4방향 규칙에는 대각선/내부 코너 전용 마스크가 없다. 실제 텍스처 도안은 지형의 연결 방향에 맞게 그린다. 저장/재열기와 타일 삭제 후 연결 복원도 검사한다.

## 2D 플랫폼 게임 예시

| 엔티티 | 권장 구성 |
|---|---|
| Player | SpriteRenderer, RigidBody2D, Collider2D, 이동 로직 |
| Ground | Tilemap 또는 Sprite + 별도로 배치한 Collider2D |
| Camera | Orthographic Camera, 필요하면 PixelPerfectCamera/Camera2DRoom(파일 확인) |
| Collectible | Sprite + 트리거용 충돌 설정 + 점수/수집 로직 |
| HUD | Canvas와 UIText 등 |

타일을 칠하기만 하면 물리 Collider가 자동 생성된다는 근거는 이번에 확인하지 못했다. 첫 테스트는 수동 Collider2D로 바닥과 벽을 구성한다. 점프/이동 구현 시 물리 속도를 중복 수정하지 않고 고정 스텝과 프레임 로직을 구분한다.

## 관련 문서 및 소스

- [`SpriteEditor.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/ResourceEditor/Private/SpriteEditor.cpp) — `Editor/Source/ImGui/ResourceEditor/Private/SpriteEditor.cpp`
- [`TilePalettePanel.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/TilemapEditor/Private/TilePalettePanel.cpp) — `Editor/Source/ImGui/TilemapEditor/Private/TilePalettePanel.cpp`
- [`TilemapComponent.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Tilemap/Public/TilemapComponent.h) — `Engine/Source/Tilemap/Public/TilemapComponent.h`
- [전체 근거 및 검토 범위](Sources.md)

