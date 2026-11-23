# 터레인 시스템 확장 계획

- 작성일: 2026-11-23
- 상태: 계획 (코드 미반영). 현재 코드 상태는 2절의 정적 분석 결과이며 실행 검증 결과가 아니다.
- 상위 우선순위: [DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7절
- 관련 문서: [Shader/BINDING_LAYOUT.md](Shader/BINDING_LAYOUT.md), [MaterialShaderGraphIntegrationPlan.md](MaterialShaderGraphIntegrationPlan.md), [CrashHandlingAndRecoveryPlan.md](CrashHandlingAndRecoveryPlan.md), [DiskCacheDevelopmentPlan.md](DiskCacheDevelopmentPlan.md) 2.4절, [Utf8PathHandlingPlan.md](Utf8PathHandlingPlan.md), [EngineEditorClientGuidelines.md](EngineEditorClientGuidelines.md) 14·19절

## 1. 목표

1. 지형 높이·스플랫 데이터를 씬과 함께 저장·복원하고, 독립 Client에서도 같은 지형을 재생한다.
2. 하이트맵을 손실 없이(16비트 이상) 가져오고 내보내며, 자동 저장(복구)에 지형을 포함한다.
3. 스플랫맵으로 여러 레이어(흙·암석·물가·접근 불가 등)를 칠하고, 레이어 팔레트에서 선택한다.
4. 지형 표면 셰이딩을 고정 `terrainPS.hlsl`이 아닌 머티리얼 그래프로 조절한다.
5. 내비게이션이 지형의 높이·경사·영역 속성을 반영한다.

## 2. 현재 코드 상태 (2026-11-23 정적 분석)

| 영역 | 현재 코드 | 문제 | 심각도 |
|---|---|---|---|
| 저장 | `TerrainComponent`의 `HeightData`·`SplatData`가 `PROPERTY`가 아니다 | 씬 저장·복구 스냅샷·Client에서 편집 결과가 사라진다. Undo는 컴포넌트 `Copy`라 메모리 안에서만 동작한다 | 심각 |
| 하이트맵 입출력 | `TerrainUtility::Save/LoadHeightmapFromFile`이 8비트 PNG, `path::string()`(ACP) 사용 | 256단계 계단, 비-ACP 경로 실패. Import·Reset이 Undo·dirty를 기록하지 않는다 | 높음 |
| 렌더 | 고정 `terrainVS/PS.hlsl`, `TerrainMaterial`은 추적되지 않는 `Assets/`에만 있고 `EntityFactory`가 구형 `Resources/` 키를 하드코딩 | 그래프 머티리얼을 쓰면 높이 변위가 없다. 새 클론에서 지형이 그려지지 않을 수 있다(실행 확인 필요) | 높음 |
| 바인딩 | `Renderer`가 heightmap을 텍스처 슬롯 1, splat을 슬롯 4(space1)에 묶는다 | 셰이더 그래프 `TextureSample` 슬롯 0..7과 충돌한다 | 높음 |
| 스플랫 | RGBA 4레이어 고정, PS는 RGB만 사용, 틴트·타일링 하드코딩 | A 채널 무시, 레이어 확장 불가 | 보통 |
| 페인트 | `ApplyPaint`가 에디터에 연결되지 않았고 `worldMatrix` 인자가 없다 | 이동·회전한 지형에서 칠 위치가 어긋난다(잠재 결함) | 보통 |
| 내비게이션 | `NavGrid::BakeObstaclesFromScene`은 콜라이더 AABB만 막는다. 결과는 registry `ctx`에만 있다 | 지형 경사·영역 무시, 베이크 결과 미저장. 오버레이·베이크 툴팁이 영문 하드코딩 | 보통 |
| 팔레트 | 레이어 팔레트 UI 없음 | — | 기능 부족 |

## 3. 설계 결정

### 3.1 데이터 소유: `TerrainData` 에셋

- 높이·스플랫·속성 맵은 씬 JSON이 아니라 별도 바이너리 에셋(`.bamterrain`)이 소유한다. `TerrainComponent`는 에셋 핸들(또는 가상 URI)과 크기·머티리얼만 직렬화한다.
- 근거: 해상도 1025에서 높이만 약 4MB(float)다. 씬 JSON에 넣으면 파일 크기·diff·로드 시간이 나빠지고 `.bampak` 쿠킹 경로와도 다르다.
- 형식(초안): 헤더(매직·버전·해상도·높이 형식) + 높이(R16_UNORM 기본, R32 선택) + 스플랫 가중치(⌈레이어 수/4⌉장의 RGBA8) + 속성 맵(셀당 `uint8` 영역 ID).
- 기각한 대안: 씬 JSON에 base64로 저장. 구현은 가장 작지만 위 비용 때문에 채택하지 않는다.

### 3.2 저장 시점과 자동 저장

- 씬 저장 시 dirty `TerrainData`를 같은 저장 트랜잭션에서 원자적으로 쓴다(`.tmp` 후 교체). 실패하면 씬 저장도 실패로 보고한다.
- 자동 저장은 `RecoveryManager` 하트비트 스냅샷(`Temp/`)에 dirty 지형을 포함하는 방식을 기본으로 한다.
- **결정 필요**: "하이트맵 자동 저장"이 복구용 스냅샷인지, 원본 에셋을 주기적으로 덮어쓰는 것인지. 원본 덮어쓰기는 되돌릴 수 없는 손실 위험이 있어 기본값으로 두지 않는다.

### 3.3 레이어와 팔레트

- `TerrainLayer` 에셋: 알베도·노멀·ORM 텍스처, 월드 타일링, 그리고 게임플레이 속성(`Walkable`, `NavCost`, `SurfaceType`).
- 팔레트는 지형이 참조하는 레이어 목록 UI다. 2D `TilePalettePanel`과 같은 선택·브러시 패턴을 쓰고, 브러시 단축키는 `ShortcutManager`에 등록한다.
- 스플랫은 시각 정보다. 물·접근 불가 같은 게임 의미를 스플랫 색에서 추론하지 않는다.
  - 물: 시각 레이어(젖은 흙 등)와 실제 수면(기존 `WaterMaterial` 메시, Render S6)을 분리한다.
  - 접근 불가: 보이지 않는 `Blocked` 마스크 레이어를 속성 맵에 칠하고 NavGrid가 읽는다.
- **결정 필요**: 목표 레이어 수(4/8/16)와 최대 해상도.

### 3.4 렌더: 엔진 버텍스 팩토리 + 그래프 표면

- 높이 변위·법선·그림자 VS는 엔진 템플릿으로 유지한다. 머티리얼 그래프는 픽셀 표면만 생성한다.
- 새 머티리얼 도메인 `Terrain`과 노드 `Terrain.LayerWeights`, `Terrain.LayerSample(i)`, `Terrain.Slope`, `Terrain.Height`를 추가한다. 레이어 텍스처는 Texture2DArray로 묶는다.
- heightmap·splat 바인딩을 머티리얼 텍스처 슬롯 공간에서 분리하고 [BINDING_LAYOUT.md](Shader/BINDING_LAYOUT.md)를 갱신한다.
- 기존 `terrainPS.hlsl`의 동작(경사 자동 암석 등)은 기본 지형 그래프로 이관한다.
- 기각한 대안: 그래프가 VS까지 생성. 그림자·피킹·TAA 모션 벡터 경로마다 생성이 필요해 비용이 크다.

### 3.5 내비게이션

- 1차: 기존 `NavGrid`를 지형 대응 그리드로 확장한다(셀 높이, 경사 한계, 스텝 높이, 속성 맵의 비용·차단). 베이크 결과를 저장한다.
- **결정 필요**: 다리·다층 구조가 필요하면 Recast/Detour 기반 내브메쉬를 별도 범위로 검토한다. 현재 vcpkg 의존성에는 없다.

## 4. 단계와 완료 기준

| 단계 | 범위 | 선행 | 완료 기준 |
|---|---|---|---|
| T1 | `TerrainData` 에셋·저장/복원, Undo·dirty 연결(Import·Reset 포함), 16비트 PNG·RAW 입출력, UTF-8 경로, 복구 스냅샷 포함, 기본 머티리얼 키 정리 | — | 스컬프트 → 저장 → 재시작 후 높이 동일. 독립 Client 동일. Import 후 Undo 가능. 한글 경로 입출력 |
| T2 | `TerrainLayer`, N개 레이어 스플랫, 페인트 브러시(`worldMatrix` 반영), 팔레트 UI, 속성 맵(Blocked/Water) | T1 | 이동·회전한 지형에서 칠 위치가 정확하고 저장·Undo 유지 |
| T3 | `Terrain` 머티리얼 도메인, 바인딩 분리, 기본 지형 그래프 이관 | T2 | 그래프 수정이 지형에 반영되고 그림자·피킹 회귀 없음 |
| T4 | 지형 대응 NavGrid, 베이크 결과 저장, 로캘 키 정리 | T1(속성은 T2) | 경사 한계 초과·Blocked 영역을 경로가 회피하고 재로드 후 유지 |

보류: 지형 LOD·청크 스트리밍, 홀(구멍) 절단, 지형 모션 벡터(TAA), Recast 내브메쉬.

## 5. 검증 원칙

- AI는 빌드·테스트를 실행하지 않는다. 각 단계 완료 시 사용자가 실행할 명령과 확인 기준을 [ImplementationLog.md](ImplementationLog.md)에 기록한다.
- `TerrainDataCheck`에 저장 왕복·리샘플링·잘못된 크기 거부 사례를 추가한다.
