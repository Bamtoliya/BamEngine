# Terrain / Spline

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## Terrain
`TerrainComponent`가 지형 데이터의 구성 경계다. TerrainSystem의 `OnUpdate`와 `OnSubmit`은 엔진 시스템 루프에서 실행된다. `GetTerrainChunkStats(const TerrainComponent&)` 및 `GetLastFrameTotalChunkStats()`는 진단 조회 경로다. `EnsureTerrainResources`, `EnsureTerrainChunks`, `UpdateTerrainHeightmap`, `UpdateTerrainSplatmap`, `UpdateTerrainHoleMask`는 리소스 및 청크 갱신용 내부 성격이 강하므로 gameplay가 직접 호출하기 전에 public/private 접근 위치와 호출 주체를 확인한다.

## Spline
`SplineComponent`와 `SplinePoint`의 제어점 데이터를 설정하고 `IsValid()`를 확인한다. SplineFollowerComponent/ SplineMeshComponent는 각각 경로 추종 및 메시 활용 경계다. 실제 거리→곡선 parameter 변환, frame orientation, 끝점/loop 처리 API는 SplineCurve와 distance/frame table의 전체 선언을 추가 확인한다.

## 적용 예 (개념)
레이싱 트랙 또는 철도: spline 점 배치 → 유효성 확인 → follower/mesh 설정 → 위치와 방향 검증. 지형: terrain 에셋 구성 → 높이·레이어 편집 → 충돌 및 nav 데이터 재생성 여부 확인. 지형 편집 결과가 자동으로 모든 파생 시스템에 전달된다는 보장은 없다.

## 테스트 기준
청크 경계, 음수 좌표, LOD 전환, hole/collision 불일치, spline 중복 점, 급회전, 일정 거리 속도, 편집 후 cache invalidation. 이 문서는 타입/경계 안내 수준이며 전체 terrain/spline 데이터 편집 API는 후속 조사가 필요하다.

## 관련 소스
- [TerrainSystem.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Terrain/Public/TerrainSystem.h) — `Engine/Source/Terrain/Public/TerrainSystem.h`
- [TerrainComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Terrain/Public/TerrainComponent.h) — `Engine/Source/Terrain/Public/TerrainComponent.h`
- [SplineComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Spline/Public/SplineComponent.h) — `Engine/Source/Spline/Public/SplineComponent.h`
- [SplineFollowerComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Spline/Public/SplineFollowerComponent.h) — `Engine/Source/Spline/Public/SplineFollowerComponent.h`
- [SplineMeshComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Spline/Public/SplineMeshComponent.h) — `Engine/Source/Spline/Public/SplineMeshComponent.h`

## 검증 및 참고 예제
- [TerrainDataCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/TerrainDataCheck.cpp)
- [SplineCurveCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/SplineCurveCheck.cpp)
