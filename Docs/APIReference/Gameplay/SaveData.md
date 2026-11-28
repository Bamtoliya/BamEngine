# SaveGame / DataTable / Gameplay

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 세이브
`SaveGameManager(path)` 또는 `Get()`로 접근한다. `IsValidSlotName`, `WriteSlot(slot,data)`, `ReadSlot(slot,outData)`, `ReadSlotMetadata`, `HasSlot`, `DeleteSlot`, `ListSlots`를 지원한다. `RegisterMigration(fromVersion,toVersion,fn)` 및 `MigrateSaveData(...)`로 포맷 진화를 처리한다. 씬 archive와 게임 진행 데이터는 동일하지 않다.

```cpp
#include "SaveGameManager.h"
bool Save(Engine::SaveGameManager& mgr, const Engine::SaveGameData& data) {
    if (!Engine::SaveGameManager::IsValidSlotName("Slot01")) return false;
    return mgr.WriteSlot("Slot01", data);
}
```
파일/권한 실패를 UI에 표시하고 덮어쓰기 시 백업 정책을 정한다. SaveGameCheck는 path traversal, roundtrip, atomic overwrite, slot CRUD, corrupted defense, backward compatibility 등의 검사 코드를 포함한다. 이번에 실행하지 않았다.

## DataTable
CSV 로딩 `LoadFromCsvString`, `LoadFromCsvFile`; `SetKeyColumn(key,outError)`, `ValidateRequiredColumns`, `FindRowIndexByKey`, `GetString/GetInt/GetFloat/GetBool` 제공. CSV 열/키 정의 및 중복·누락·변환 실패를 로딩 시 검증한다. `SetCell(row,col,value)`, `AddEmptyRow`, `RemoveRow`, `SaveToCsvFile`는 편집 경로이다. getter의 전체 parameter 및 fallback 정책은 선언/구현 추가 확인 후 호출한다. DataTable Editor와 runtime CSV를 별도 사용하지 않도록 schema를 공유한다.

## Gameplay helpers
Health/Hitbox/Interactable 등의 구체 API는 이번에 개별 본문을 확인하지 않았으므로 레퍼런스 범위에서 제외한다. 공통 전투 정책과 개별 게임 규칙을 분리한다.

## 관련 소스
- [SaveGameManager.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Gameplay/Public/SaveGameManager.h) — `Engine/Source/Gameplay/Public/SaveGameManager.h`
- [SaveGameTypes.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Gameplay/Public/SaveGameTypes.h) — `Engine/Source/Gameplay/Public/SaveGameTypes.h`
- [DataTable.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Gameplay/Public/DataTable.h) — `Engine/Source/Gameplay/Public/DataTable.h`

## 검증 및 참고 예제
- [SaveGameCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/SaveGameCheck.cpp)
- [GameplayCommonCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/GameplayCommonCheck.cpp)
