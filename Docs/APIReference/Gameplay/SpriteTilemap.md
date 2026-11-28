# Sprite / Sprite Animation / Tilemap

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## Sprite
`SpriteRendererComponent`의 `spriteHandle`, `materialHandle`, `size`, `color`, `sortingLayerID`, `orderInLayer`, `flipX/flipY`를 설정한다. sprite region과 pivot/PPU는 에셋 데이터다. `SpriteAnimationComponent`는 `animation`, `framesPerSecond`(12 기본), `playRate`, `Play`, `Pause`, `Stop`, `SetFrame`, `GetEffectiveFrameCount`, `GetSpriteAtFrame`을 제공한다. 자동 비활성화/삭제 옵션이 켜졌다면 종료 후 엔티티 참조를 계속 사용하지 않는다.

## Tilemap
`TilemapComponent::SetTile(x,y,sprite,color)`, `SetAutotile(x,y)`, `ClearTile(x,y)`, `GetTile(x,y)`를 사용한다. 청크는 16x16이다. savedTiles와 chunks는 저장/실행 데이터로 분리되며 `SyncSavedTiles`, `RestoreFromSavedTiles`가 연결한다. 저장 계약은 Scene archive 경로를 따른다.

```cpp
#include "TilemapComponent.h"
bool Paint(Engine::TilemapComponent& tilemap,
           Engine::ResourceHandle<Engine::Sprite> sprite) {
    return tilemap.SetTile(0, 0, sprite);
}
```
SetTile 반환 false는 동일 셀/no-op 등일 수 있다. invalid sprite는 ClearTile 경로로 이어지므로 잘못된 handle을 넘기지 않는다.

## 4방향 Autotile
bit 0=+Y(1), bit 1=+X(2), bit 2=-Y(4), bit 3=-X(8). 실제 화면의 위/아래는 카메라 좌표에 맞춰 해석한다. mask 0=고립, 2=+X, 8=-X, 10=좌우, 15=4방향. `autotileSprites[0]`이 유효해야 SetAutotile이 동작한다. 연결 대상은 isAutotile 셀에 한정한다. 해당 마스크 스프라이트가 누락되면 resolver의 fallback을 확인한다. 일반 타일과 서로 다른 종류의 자동 연결을 기대하지 않는다(A09). 규칙 편집 시 저장/reopen과 인접 셀 재계산을 확인한다.

## 관련 소스
- [SpriteRendererComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Sprite/Public/SpriteRendererComponent.h) — `Engine/Source/Sprite/Public/SpriteRendererComponent.h`
- [SpriteAnimationComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Sprite/Public/SpriteAnimationComponent.h) — `Engine/Source/Sprite/Public/SpriteAnimationComponent.h`
- [TilemapComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Tilemap/Public/TilemapComponent.h) — `Engine/Source/Tilemap/Public/TilemapComponent.h`

## 검증 및 참고 예제
- [SpriteAtlasCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/SpriteAtlasCheck.cpp)
- [TilemapCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/TilemapCheck.cpp)
- [PixelPerfectCameraCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/PixelPerfectCameraCheck.cpp)
- [SpriteSlicingCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/examples/SpriteSlicingCheck.cpp)
