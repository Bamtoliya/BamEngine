# 터레인 시스템 확장 계획

- 작성일: 2026-11-23
- 상태: T1·T2(a~e)·T3(a~c) 코드 반영(6.1~6.13절, 빌드·실행 미검증), T4 계획. 현재 코드 상태는 2절의 정적 분석 결과이며 실행 검증 결과가 아니다.
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
- **결정(2026-11-24, 기본안 적용)**: "하이트맵 자동 저장"은 복구용 스냅샷이다. 원본 `.bamterrain`은 사용자가 씬을 저장할 때만 바뀐다. 사용자가 명시적으로 선택하지 않아 계획의 기본안을 적용했으며, 원본 주기 덮어쓰기가 필요하면 별도 설정으로 다시 논의한다.

### 3.3 레이어와 팔레트

- `TerrainLayer` 에셋: 알베도·노멀·ORM 텍스처, 월드 타일링, 그리고 게임플레이 속성(`Walkable`, `NavCost`, `SurfaceType`).
  - **결정(2026-11-24, 사용자)**: 독립 에셋(`TerrainLayerAsset`, `.bamterrainlayer.json`)으로 한다. T2-c에서 잠시 컴포넌트 내장 목록으로 구현했다가, 지형 사이 공유를 위해 에셋으로 바꿨다(6.8절). 지형은 칸마다 에셋을 참조한다. 노멀·ORM 텍스처는 레이어 렌더링 단계에서 추가한다.
- 팔레트는 지형이 참조하는 레이어 목록 UI다. 2D `TilePalettePanel`과 같은 선택·브러시 패턴을 쓰고, 브러시 단축키는 `ShortcutManager`에 등록한다.
- 스플랫은 시각 정보다. 물·접근 불가 같은 게임 의미를 스플랫 색에서 추론하지 않는다.
  - 물: 시각 레이어(젖은 흙 등)와 실제 수면(기존 `WaterMaterial` 메시, Render S6)을 분리한다.
  - 접근 불가: 보이지 않는 `Blocked` 마스크 레이어를 속성 맵에 칠하고 NavGrid가 읽는다.
- **결정(2026-11-24, 사용자)**: 레이어 최대 8개, 최대 해상도 분할 1024(정점 1025).

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

## 6. 진행 기록

### 6.1 T1-a: 데이터 파일 저장·복원 (2026-11-24, 코드 반영 / 빌드·실행 미검증)

- **파일:** `TerrainDataFile`(`Engine/Source/Terrain`)이 `.bamterrain` v1을 쓰고 읽는다. 헤더 24바이트(매직 `BAMTERR `, 버전, 해상도, 높이 형식 1 = R16_UNORM, 스플랫 채널 4) 뒤에 높이 uint16, 스플랫 RGBA8이 온다. 임시 파일에 쓴 뒤 교체하며, 매직·버전·형식·파일 크기가 하나라도 맞지 않으면 읽지 않는다. 높이는 0~1로 잘라 저장하므로 왕복 오차는 1/65535 이하다.
- **컴포넌트:** `TerrainComponent::DataPath`(가상 URI, 읽기 전용 표시, `ALLOW_MISSING`)를 직렬화한다. 이전 씬에는 이 값이 없으므로 처음 갱신 때 기본 형태를 만들던 기존 동작을 그대로 따른다.
- **저장:** `SceneDocument::SaveTo`가 씬 JSON을 쓰기 전에 `TerrainDataFile::SaveSceneTerrains`를 부른다. 데이터가 만들어진 모든 지형을 쓰고, 경로가 없으면 씬 옆 `<씬 이름>_Terrain_<엔티티 ID 16진수>.bamterrain`을 정한다. 하나라도 실패하면 씬 저장도 실패한다. 편집 표시(`IsDataDirty`)로 건너뛰지 않는 이유는 Undo가 컴포넌트 사본을 되돌리며 표시도 편집 전 값으로 돌아가기 때문이다.
- **복원:** `TerrainSystem::OnUpdate`가 `DataPath`가 바뀐 지형을 `TerrainDataFile::LoadIfChanged`로 읽는다. 해상도가 다르면 기존 리샘플링을 거친다. 읽지 못하면 경고하고 현재 높이를 유지한다.
- **편집 표시:** 스컬프트(`ApplyBrush`), 하이트맵 가져오기, 스플랫 페인트, 초기화가 `IsDataDirty`를 켠다(후속 단계의 미저장 표시·복구용).
- **검사:** `TerrainDataCheck`에 한글 파일명 왕복, R16 오차, 크기 불일치 쓰기 거부, 잘린·손상·없는 파일 거부, 기본 경로 이름 규칙을 추가했다.
- **이 단계에서 하지 않은 것(T1 잔여):**
  - T1-b: PIE 시작·종료와 복구 스냅샷이 씬 아카이브만 복사하므로 저장하지 않은 지형 편집이 PIE 후·복구 후 사라질 수 있다(기존 결함, 실행 확인 필요). "하이트맵 자동 저장"의 의미 결정(3.2절)이 이 단계에서 필요하다.
  - T1-c: 하이트맵 가져오기·초기화의 Undo, 16비트 PNG·RAW 입출력, 하이트맵 경로의 UTF-8 처리.
  - T1-d: 기본 머티리얼 키 정리, Client 패키징에 `.bamterrain` 포함 확인.
  - 다른 이름으로 저장하면 새 씬도 이전 씬의 데이터 파일을 가리킨다(두 씬이 같은 파일을 공유한다). 저장 중 정한 `DataPath`는 편집 이력 밖에서 바뀐다.

### 6.2 T1-b: 저장하지 않은 지형 편집 보존 (2026-11-24, 코드 반영 / 빌드·실행 미검증)

- **결함(정적 분석).** PIE는 시작 전 씬 아카이브를 잡고 종료 시 그것으로 씬을 다시 만든다. 복구 스냅샷도 씬 아카이브만 쓴다. 아카이브에는 지형 데이터 경로만 있으므로, 저장하지 않은 스컬프트·페인트가 PIE 종료나 복구 후 마지막 저장 파일(또는 기본 형태)로 돌아갔다.
- **PIE.** `PIEManager::SnapshotScene`이 `TerrainDataFile::CaptureMemory`로 지형 높이·스플랫과 편집 표시를 엔티티 ID(`IDComponent`)별로 복사한다. `RestoreScene`이 씬을 열기 전에 `RestoreMemory`로 되돌리고 `LoadedDataPath`를 맞춰 저장 파일로 덮어쓰지 않는다. ID가 없는 엔티티는 대상이 아니다.
- **복구 스냅샷.** `RecoveryManager::WriteSnapshotInternal`이 `ScopedRecoveryCopies`로 지형마다 스냅샷 옆 `<스냅샷 이름>_Terrain_<ID>.bamterrain`을 쓰고, 스냅샷을 쓰는 동안만 `TerrainComponent::RecoveryDataPath`를 채운다. 복원하면 `LoadIfChanged`가 그 사본을 읽고 편집된 상태로 표시하며, `DataPath`(원본)는 그대로라 다음 씬 저장 때 원본에 쓴다. 사본을 쓰지 못한 지형은 복구 시 마지막 저장 파일을 쓴다(스냅샷 자체는 남긴다).
- **검사.** `TerrainDataCheck`의 `CheckUnsavedTerrainPreservation`: PIE 사본 복원, 복구 사본 기록·경로 정리, 복구 사본 읽기·편집 표시·원본 경로 유지.
- **남은 한계.** 복구 임시 사본은 스냅샷처럼 같은 이름으로 덮어쓰며 별도로 지우지 않는다. 긴급 스냅샷(충돌 처리 중)도 지형 파일을 쓰므로 그 시점의 메모리 상태에 따라 실패할 수 있다.

### 6.3 T1-c: 하이트맵 16비트 입출력과 가져오기·초기화 Undo (2026-11-24, 코드 반영 / 빌드·실행 미검증)

- **결함(정적 분석).** `LoadHeightmapFromFile`·`SaveHeightmapToFile`이 8비트 PNG만 다뤄 256단계 계단이 생겼고, `path::string()`(ACP)을 stb에 넘겨 한글 경로에서 실패했다. 가져오기 실패 시 높이 배열 크기를 먼저 바꿨다. 지형 편집기의 가져오기·초기화는 편집 이력에 남지 않아 Ctrl+Z로 되돌릴 수 없었고, 실패를 알리지 않았다.
- **입력.** 파일을 넓은 문자 경로로 메모리에 읽어 `stbi_load_16_from_memory`로 디코딩한다(8비트 이미지도 16비트로 넓혀 같은 경로). `.raw`·`.r16`은 머리말 없는 리틀 엔디언 uint16 정사각형으로 읽고 한 변을 파일 크기에서 구한다. 크기가 정사각형이 아니면 거부한다. 결과는 새 배열에 만든 뒤 성공할 때만 바꾼다.
- **출력.** 확장자가 `.raw`·`.r16`이면 RAW, 그 외에는 16비트 그레이스케일 PNG다. `stb_image_write`는 8비트만 쓰므로 같은 파일의 `stbi_zlib_compress`·CRC로 PNG 청크를 직접 조립한다(새 의존성 없음, 모든 플랫폼).
- **편집기.** 가져오기·초기화를 스컬프트와 같은 `SceneEditSnapshot` 이력(`TerrainEditor.HistoryImport`·`HistoryReset`)으로 기록한다. 가져오기 실패와 내보내기 성공·실패를 알림으로 보인다. 파일 대화상자에 RAW 필터를 추가했다.
- **로캘.** `Editor.TerrainEditor.csv`에 8행.
- **검사.** `TerrainDataCheck`의 `CheckHeightmapImportExport`: 한글 경로 16비트 PNG·RAW 왕복(오차 1/65535 이하), 잘못된 RAW·없는 파일 거부 시 높이 유지.
- **남은 한계.** RAW는 정사각형만 받고 빅 엔디언 RAW는 받지 않는다. PNG의 감마·색 프로필은 무시한다. 내보낸 PNG의 압축 수준은 고정(8)이다.

### 6.4 T1-d: 기본 머티리얼과 패키징 (2026-11-24, 코드 반영 / 빌드·실행 미검증)

- **결함(정적 분석).** `EntityFactory::CreateTerrainEntity`가 `GetResourceHandle<Material>(L"Resources/Material/TerrainMaterial")`로 키만 조회했다. 이 함수는 파일을 불러오지 않으므로, 같은 키로 미리 등록된 리소스가 없으면 빈 핸들이 되고 `TerrainSystem::OnSubmit`이 머티리얼 없음으로 지형을 건너뛴다. 머티리얼 파일은 추적되지 않는 `Assets/Material/`에만 있어 새 클론에는 없다.
- **수정.**
  - `Engine/Resources/Material/TerrainMaterial.bammat.json`을 추적되는 엔진 리소스로 추가했다(기존 `Assets` 파일 기반, 셰이더·텍스처 경로를 `DefaultMaterial`과 같은 `Engine://` 형식으로 맞춤).
  - `TerrainComponent::DefaultMaterialUri`를 두고, 새 지형은 `LoadFile`로 이 파일을 불러 쓴다.
  - 머티리얼이 비었거나 찾지 못한 지형(구형 키를 쓰는 이전 씬, 새 클론)은 그릴 때 기본 머티리얼을 쓴다. 컴포넌트 값은 바꾸지 않으며, 불러오기는 한 번만 시도한다.
  - 사용자 PC의 `Assets/Material/TerrainMaterial.bammat.json`은 키가 달라 충돌하지 않고, 그 파일을 참조하는 기존 씬은 계속 그 파일을 쓴다. 지우지 않았다.
- **패키징(확인 결과, 코드 변경 없음).** `ClientPackage`의 `IsRuntimeResource`가 확장자가 `.bam`으로 시작하는 파일을 모두 복사하므로 `.bamterrain`은 일반 파일 패키지에 포함된다.
- **남은 한계(지형 밖 기존 한계).** Client는 `.bampak`을 마운트하지만 엔진 리소스 로더 중 pak에서 바이트를 읽는 곳이 없다(`PakMountManager::TryReadBytes` 호출부 없음). pak 패키지에서는 지형 데이터뿐 아니라 다른 리소스도 디스크 경로로만 찾는다. 별도 작업으로 다룬다.

### 6.5 T2-a: 8레이어 데이터·속성 맵·페인트 위치 (2026-11-24, 코드 반영 / 빌드·실행 미검증)

- **데이터.** `TerrainComponent`에 `MaxLayers = 8`, `LayersPerPlane = 4`, `MaxResolution = 1024`를 두었다. 레이어 0~3은 기존 `SplatData`, 4~7은 새 `SplatDataExtra`(같은 RGBA8 배치), 게임 의미는 `AttributeData`(정점당 `ETerrainAttribute`: 0 없음, 1 Blocked, 2 Water)에 담는다. 시각 레이어와 게임 의미를 분리한다(3.3절).
- **해상도 상한.** `Resolution`에 `RANGE(1, 1024)`를 붙이고 `TerrainUtility::TryGetGridLayout`이 1024 초과를 거부한다. 파일 읽기·쓰기와 리소스 생성이 이 검사를 거친다. 이전에 1024를 넘는 해상도로 저장한 씬은 지형이 그려지지 않는다(기본값은 128).
- **크기 맞춤.** `TerrainUtility::EnsureLayerData`가 매 갱신에서 두 레이어 평면과 속성 맵을 해상도 크기로 맞춘다(비었으면 레이어 0 = 255, 해상도가 바뀌면 최근접 리샘플링).
- **파일 v2.** `.bamterrain` 쓰기는 v2(채널 8: 레이어 0~3, 4~7, 속성 맵)이고, 읽기는 v1(레이어 4~7·속성 0)과 v2를 받는다. `TerrainDataFile::Write`는 `TerrainDataContent` 하나를 받는다. PIE 메모리 사본·복구 사본·씬 저장 모두 새 배열을 함께 다룬다.
- **페인트.** `ApplyPaint(terrain, 월드 좌표, worldMatrix, 레이어 0~7, ...)`가 역행렬로 로컬 좌표를 구해 이동·회전한 지형에서도 위치가 맞는다(2절의 잠재 결함). 8개 가중치 합을 255로 정규화한다. `ApplyAttributePaint`는 반경 안 정점에 속성 값을 그대로 칠한다. 둘 다 바뀐 정점이 있을 때만 true와 `IsDataDirty`.
- **검사.** `TerrainDataCheck`의 `CheckLayersAndAttributes`: 해상도 상한, v2 왕복, v1 호환, 이동·회전 지형의 레이어 6 페인트 위치와 정규화, 속성 페인트, 해상도 변경 시 크기 맞춤.
- **아직 없는 것(T2-b 이후).**
  - 렌더링: GPU에는 레이어 0~3만 올라간다. 레이어 4~7을 칠해도 화면에는 보이지 않는다. 두 번째 스플랫 텍스처와 셰이더(`terrainPS.hlsl`은 RGB 3개만 사용)는 바인딩 분리와 함께 다룬다.
  - `TerrainLayer` 에셋(텍스처·타일링·`Walkable`·`NavCost`·`SurfaceType`), 팔레트 UI, 에디터의 페인트·속성 브러시 연결, 브러시 단축키(`ShortcutManager`). 현재 에디터에는 페인트 입력이 없다.
  - 페인트 반경은 로컬 단위라 지형 배율이 균일하지 않으면 원이 아니다.

### 6.6 T2-b: 에디터 브러시 도구·팔레트·단축키 (2026-11-24, 코드 반영 / 빌드·실행 미검증)

- **도구.** 지형 편집기에 `조형`·`레이어 페인트`·`속성 페인트` 도구(`ETerrainTool`)를 두었다. 큰 켜기/끄기 버튼은 도구와 무관한 "브러시" 스위치가 되었고(문구 변경), 켜져 있을 때만 뷰포트 좌클릭이 지형 편집이 된다.
- **팔레트.** 레이어 페인트는 8칸(4×2) 팔레트에서 레이어를 고른다. 색 견본은 구분용 고정 색이며 레이어 텍스처는 `TerrainLayer` 에셋 단계에서 연결한다. 5~8번을 고르면 "아직 그려지지 않음" 경고를 보인다. 속성 페인트는 `지우기`·`이동 불가`·`물` 중 하나를 칠한다.
- **뷰포트.** `SceneViewportPanel::HandleTerrainSculpting`이 도구에 따라 `ApplyBrush`·`ApplyPaint`(세기 × dt)·`ApplyAttributePaint`를 부른다. 한 번의 드래그는 기존처럼 하나의 편집 이력이 되며, 이름이 도구별(조형·레이어 페인트·속성 페인트)이다. 브러시 링 색은 조형 초록, 페인트 파랑, 속성 주황이다.
- **단축키(`ShortcutManager`, 뷰포트 문맥).** `Terrain.Layer.1`~`8`(숫자 1~8, 브러시가 켜져 있고 페인트 도구일 때), `Terrain.Brush.Smaller`·`Larger`(`[`·`]`, 반복 입력, 브러시가 켜져 있을 때). 분류는 `Shortcuts.CategoryTerrain`. 팔레트 툴팁과 반경 툴팁이 `GetShortcutHint`로 현재 키를 보여준다.
- **로캘.** `Editor.TerrainEditor.csv` 13행 추가·2행 문구 변경, `Editor.Action.csv` 10행, `Editor.Shortcuts.csv` 1행.
- **검사.** 이 단계는 패널·뷰포트 입력이라 자동 검사를 추가하지 않았다. 페인트 계산 자체는 T2-a의 `CheckLayersAndAttributes`가 검사한다.
- **남은 것(T2-c 이후).** `TerrainLayer` 에셋(텍스처·타일링·`Walkable`·`NavCost`·`SurfaceType`)과 지형별 레이어 목록, 레이어 4~7 렌더링, 속성 맵의 뷰포트 시각화(지금은 칠해도 보이지 않는다).

### 6.7 T2-c: 레이어 정의와 속성 맵 표시 (2026-11-24, 코드 반영 / 빌드·실행 미검증)

- **레이어 정의.** (6.8절에서 에셋 참조로 바뀜) `TerrainLayerSlot`(이름, 알베도 텍스처, 반복 길이, 이동 가능, 이동 비용, 표면 종류)을 `TerrainComponent::Layers`에 담았다. i번째 항목이 스플랫 레이어 i이고 8개를 넘는 항목은 쓰지 않는다. 인스펙터의 지형 컴포넌트에서 항목을 추가·삭제·편집하며, 씬과 함께 저장되고 편집 이력에 남는다.
- **팔레트.** 레이어 칸에 정의된 이름을 보이고(없으면 "레이어 N"), 선택한 레이어의 이동 가능·이동 비용·표면 종류를 요약한다. 정의가 없으면 추가 방법을 안내한다.
- **속성 맵 표시.** 속성 페인트 도구일 때 `SceneViewportPanel::DrawTerrainAttributeOverlay`가 속성이 있는 정점마다 점을 그린다(이동 불가 빨강, 물 파랑). 커서가 지형 밖이어도 그린다. 매 프레임 모든 정점을 검사하므로 해상도 1024(약 105만 정점)에서는 비용이 크다(측정 전).
- **아직 없는 것.** 레이어 텍스처·반복 길이의 실제 렌더링과 레이어 4~7 렌더링(셰이더·두 번째 스플랫 텍스처·바인딩 분리), `Walkable`·`NavCost`를 읽는 내비게이션(T4).
- **검사.** 리플렉션 직렬화·UI라 자동 검사를 추가하지 않았다. 저장·복원은 수동 확인 항목으로 둔다.

### 6.8 T2-d: 지형 레이어 에셋 (2026-11-24, 코드 반영 / 빌드·실행 미검증)

- **결정.** 사용자가 컴포넌트 내장 목록(T2-c) 대신 독립 에셋을 택했다. 같은 레이어를 여러 지형이 공유하고 한 곳에서 고친다.
- **에셋.** `TerrainLayerAsset`(`Engine/Source/Terrain`), 리소스 종류 `EResourceType::TerrainLayer`(열거 끝에 추가), 로더 확장자 `.bamterrainlayer`(파일은 `.bamterrainlayer.json`). 설정은 `TerrainLayerSettings`(알베도 텍스처, 반복 길이, 이동 가능, 이동 비용, 표면 종류) 하나에 모아 편집기가 구조체 단위로 비교·되돌리기를 한다. 파일을 읽을 때 파일에 적힌 `m_Key`·`m_Path`로 등록 식별자를 바꾸지 않는다(`Resource::Reload`와 같은 규칙). 표시 이름은 파일 이름이다.
- **지형.** `TerrainLayerSlot`은 `ResourceHandle<TerrainLayerAsset> Layer` 하나만 가진다. 인스펙터의 지형 컴포넌트 레이어 목록에서 칸을 추가하고 에셋 피커(아이콘·"TERRAIN LAYER" 배지)로 고른다. T2-c 형식으로 저장한 씬의 레이어 값(이름 등)은 읽지 않고 버린다(커밋 전 형식이라 변환하지 않음).
- **편집기.** `TerrainLayerEditor`(리소스 편집기): 설정 표(리플렉션), 저장 버튼·Ctrl+S(`TerrainLayerEditor.Save`), 실행 취소·다시 실행(`RegisterDocumentHistory`, 전용 단축키 문맥 `EShortcutContext::TerrainLayerEditor`), 미저장 닫기 확인. 콘텐츠 브라우저에서 더블클릭으로 연다.
- **생성·표시.** 콘텐츠 브라우저 만들기 메뉴 "지형 레이어", 격자 아이콘·배지.
- **반영.** 저장하면 에셋 핫 리로드(`ProcessModifiedTerrainLayer`)가 이미 불러온 런타임 에셋을 다시 읽어, 그 에셋을 참조하는 모든 지형의 팔레트·속성에 반영한다. Client 패키지는 `.bam*` 규칙으로 포함된다.
- **팔레트.** 칸 이름은 참조한 에셋의 파일 이름, 요약은 에셋 설정을 쓴다.
- **검사.** `TerrainDataCheck`의 `CheckTerrainLayerAsset`: 한글 파일명 설정 저장 왕복, 로더가 정한 키 유지, 표시 이름, 확장자 판정.
- **남은 것.** 레이어 텍스처의 실제 렌더링, 노멀·ORM 텍스처, 이동·이름 변경 시 지형 참조 갱신은 기존 리소스 핸들 경로를 따르며 따로 검사하지 않았다.

### 6.9 T2-e: 8레이어 렌더링 (2026-11-24, 코드 반영 / 빌드·실행 미검증)

- **범위.** 머티리얼 그래프 도메인(T3) 전에, 고정 `terrainPS.hlsl`이 레이어 에셋과 8개 레이어를 그린다. 결과는 T3에서 기본 지형 그래프로 옮길 기준이 된다.
- **바인딩.** 레이어 4~7 스플랫을 `t5`, 레이어 0~7 알베도를 `t8`~`t15, space1`에 묶는다. 레이어 반복 길이·색 배율은 지형 드로우만 `SceneUBO` 뒤에 붙인 `TerrainObjectData`(256바이트, `b0, space2`)로 보낸다. 지형 VS·그림자·모션 벡터 셰이더는 앞 96바이트만 읽어 바뀌지 않는다. 세부는 [Shader/BINDING_LAYOUT.md](Shader/BINDING_LAYOUT.md) "지형 바인딩".
- **데이터.** `TerrainSystem`이 `SplatmapExtraTexture`를 만들고 `IsSplatDirty`일 때 두 스플랫을 함께 올린다. 그리기 전에 `FillTerrainLayerDrawData`가 칸별 에셋에서 알베도 텍스처·반복 길이를 채운다. 에셋이나 알베도가 없으면 흰 텍스처와 기본 색(레이어 0~2는 이전 셰이더의 잔디·암석·흙 색), 반복 5m(이전 0.2배율)를 쓴다. 그래서 레이어 에셋이 없는 기존 지형은 이전과 같은 모양이어야 한다.
- **셰이더.** 두 스플랫 가중치 합으로 다시 정규화하고, 가중치가 있는 레이어만 읽는다. 분기 안에서 읽으므로 분기 밖에서 구한 월드 XZ 기울기로 `SampleGrad`를 쓴다. 경사 자동 암석은 이전처럼 레이어 1이다. 팔레트의 "레이어 5~8은 그려지지 않음" 경고를 없앴다.
- **위험·미확인.**
  - D3D12 루트 CBV 외 백엔드(Vulkan·SDL GPU)에서 슬롯 1에 256바이트 상수를 받는지 확인하지 않았다. 지형은 현재 D3D12 경로로만 확인해 왔다.
  - 레이어 텍스처 크기·형식은 제한하지 않는다(텍스처마다 별도 SRV). 밉맵이 없는 텍스처는 멀리서 반짝일 수 있다.
  - 노멀·ORM 레이어 텍스처, 높이 기반 블렌딩은 없다. `t1`·`t4`의 머티리얼 슬롯 충돌은 T3에서 다룬다.
- **검사.** 셰이더·GPU 경로라 자동 검사를 추가하지 않았다. 수동 확인 항목으로 둔다.

### 6.10 T3-a: 지형 바인딩을 머티리얼 범위에서 분리 (2026-11-24, 코드 반영 / 빌드·실행 미검증)

- **이유.** 높이맵(`t1`)과 스플랫(`t4`·`t5`)이 머티리얼 텍스처 범위 `t0`~`t7, space1`에 있어, 셰이더 그래프의 `TextureSample` 슬롯 1·4·5와 충돌했다(2절). 그래프가 지형 표면을 만들게 하려면(T3-b) 먼저 분리해야 한다.
- **배치.** 높이맵 `t8, space3`, 스플랫 `t9`·`t10, space3`, 레이어 알베도는 그대로 `t8`~`t15, space1`. `space3` 샘플러가 `s0`~`s7`뿐이라 슬롯 16 이상은 `BindTexture`로 텍스처만 묶고, 높이맵은 `Load`, 스플랫은 `s0, space1`로 읽는다. 슬롯 번호는 `RenderCommands.h`의 `TerrainHeightmapSlot`·`TerrainSplatSlot`·`TerrainSplatExtraSlot`. 세부는 [Shader/BINDING_LAYOUT.md](Shader/BINDING_LAYOUT.md) "지형 바인딩".
- **바꾼 셰이더.** `terrainVS`, `terrainShadowDepthVS`, `motionVectorTerrainVS`, `terrainPS`. C++은 `Renderer.cpp`의 지형 드로우와 모션 벡터 지형 드로우.
- **충돌 확인(정적 분석).** 지형이 그려지는 G-Buffer·그림자·모션 벡터 패스에서 `space3`의 `t8`~`t10`을 묶는 곳이 없다(`MAX_MATERIAL_TEXTURE_SLOTS +` 검색). 포워드 패스는 `t10`~`t12`를 쓰지만 지형은 불투명이라 포워드 패스에 제출되지 않는다.
- **미확인.** D3D12 외 백엔드의 `space3` 슬롯 24~26 바인딩.
- **다음(T3-b·T3-c).** `Terrain` 머티리얼 도메인과 노드(`Terrain.LayerWeights`, `Terrain.LayerSample(i)`, `Terrain.Slope`, `Terrain.Height`), 기본 지형 그래프로 `terrainPS` 동작 이관.

### 6.11 T3-b1: 셰이더 그래프 Terrain 도메인 컴파일러 (2026-11-24, 코드 반영 / 빌드·실행 미검증)

- **범위.** T3-b를 둘로 나눴다. 이번 T3-b1은 엔진 컴파일러와 노드, T3-b2는 머티리얼 편집기 연결(지형 머티리얼에서 이 생성기 선택, 정점 셰이더를 `terrainVS`로 고정, 실시간 반영)이다.
- **공용 헤더.** `Engine/Resources/Shader/hlsl/terrainLayers.hlsli`에 지형 드로우 상수(`terrainObject`), 높이맵·스플랫·레이어 알베도, `TerrainBeginPixel`(정규화 가중치·경사·높이·월드 기울기), `TerrainLayerWeight`, `TerrainSampleLayer`, `TerrainBlendLayers`를 모았다. `terrainPS.hlsl`도 이 헤더로 다시 썼다(동작 같음). 샘플러는 `s0, space1` 하나이며, 그래프가 같은 레지스터에 `materialSampler`를 선언하면 `#define TERRAIN_SAMPLER materialSampler`로 같은 이름을 쓴다.
- **노드(셰이더 도메인).** `Terrain.LayerWeight(layer)` → float, `Terrain.LayerSample(layer)` → rgba/rgb, `Terrain.LayerBlend` → rgba/rgb, `Terrain.Slope` → float(1 평지, 0 수직), `Terrain.Height` → float(0~1, 높이맵을 지형 UV로 읽어 이동·회전과 무관). `layer`는 0~7 상수 핀(연결 불가).
- **생성기.** `ShaderGraphCompiler::GenerateTerrainMaterialHlsl`: 픽셀 입력은 `terrainVS` 출력과 같다(두 번째 UV 없음). `MaterialOutput`의 픽셀 핀만 쓰며 정점 핀(`worldPositionOffset`, `customUV0/1`)은 무시한다. `baseColor`를 연결하지 않으면 `TerrainBlendLayers`, 나머지 PBR 기본값은 다른 머티리얼처럼 `gMaterial` 상수. 셰이딩 플래그는 `terrainPS`와 같다.
- **거부 규칙.** 지형 노드는 이 생성기 밖(G-Buffer·포워드·언릿·VS)에서 `ShaderGraph.TerrainDomainOnly`, 정점 단계에서 `TerrainPixelOnly`, 범위 밖 레이어는 `TerrainLayerRange`, 지형 도메인의 `Shader.TexCoord1`은 `TerrainNoUV1`.
- **검사.** `ShaderGraphCheck`에 지형 도메인 사례: 기본 레이어 합성·헤더 포함·두 번째 UV 없음, 레이어 3 샘플·경사 식, G-Buffer 생성기의 거부와 원본 유지, 레이어 9 거부, UV1 거부, 슬롯 0 사용 시 샘플러 별칭 순서.
- **아직 없는 것.** 편집기에서 지형 머티리얼을 그래프로 만들고 컴파일·적용하는 경로(T3-b2), 기본 지형 그래프 에셋(T3-c). 생성된 HLSL의 실제 DXC 컴파일은 T3-b2에서 확인한다(이번 검사는 문자열만 본다).

### 6.12 T3-b2: 머티리얼 편집기의 Terrain 도메인 연결 (2026-11-24, 코드 반영 / 빌드·실행 미검증)

- **도메인.** `MaterialInterface`에 `EMaterialDomain { Surface, Terrain }`(`m_Domain`, `ALLOW_MISSING`, 인스펙터 파이프라인 분류)를 추가했다. 값이 없던 머티리얼은 Surface다. `MaterialInstance`는 기본 머티리얼의 도메인을 따른다. 엔진 기본 지형 머티리얼(`TerrainMaterial.bammat.json`)을 Terrain으로 표시했다.
- **편집기.** 지형 도메인 머티리얼의 그래프는 `GenerateTerrainMaterialHlsl`로 픽셀 셰이더만 만들고, 정점 셰이더는 엔진 `terrainVS`(`MaterialEditor::GetTerrainVertexShader`, 공유 리소스라 실패해도 지우지 않음)와 짝짓는다. 주 패스는 지오메트리여야 하며 아니면 `MaterialEditor.TerrainNeedsGeometry`. 그림자 패스 셰이더(`terrainShadowDepthVS`·`shadowDepthPS`)는 그래프 셰이더가 아니라 그대로 남는다.
- **사용법.** 지형 머티리얼(엔진 `TerrainMaterial`을 복제하거나 새 머티리얼의 도메인을 Terrain으로 바꾼 것)에 셰이더 그래프를 연결하고 컴파일 → 지형 컴포넌트의 머티리얼로 지정.
- **한계.**
  - 머티리얼 편집기 미리보기(구)는 `terrainVS`를 높이맵·스플랫 없이 그리므로 지형 머티리얼 모양을 보여주지 못한다(실제 지형에서 확인).
  - `terrainVS`는 DirectX12 바이너리만 찾으므로 다른 RHI에서는 지형 그래프 머티리얼을 적용할 수 없다.
  - 기본 지형 그래프 에셋(현재 `terrainPS`와 같은 결과)은 T3-c.

### 6.13 T3-c: 기본 지형 그래프 (2026-11-24, 코드 반영 / 빌드·실행 미검증)

- **방식.** 기본 그래프를 손으로 쓴 JSON 에셋 대신 코드(`ShaderGraphCompiler::BuildDefaultTerrainGraph`)로 만든다. 그래프 파일 형식이 바뀌어도 어긋나지 않고, 검사가 같은 그래프를 컴파일해 확인할 수 있다.
- **내용.** `terrainPS.hlsl`과 같은 식: 절벽 가중치 = 1 − smoothstep(0.55, 0.78, `Terrain.Slope`), 색 = lerp(`Terrain.LayerBlend`, `Terrain.LayerSample(1)`, 절벽), 거칠기 = lerp(0.85, 0.65, 절벽), 금속성 0·캐비티 0.5·AO 1은 상수 노드(연결하지 않으면 머티리얼 상수를 읽어 달라지므로).
- **사용.** 머티리얼 편집기가 지형 도메인 머티리얼에 새 셰이더 그래프를 만들 때(`CreateDefaultMaterialShaderGraph`) 이 그래프로 시작한다. 표면 머티리얼은 이전처럼 출력 노드만 있는 그래프다.
- **엔진 머티리얼은 연결하지 않음.** 엔진 `TerrainMaterial`은 그래프에 연결하지 않고 `terrainPS.cso`를 그대로 쓴다. 엔진 공유 리소스에 그래프를 연결하면 편집기가 임시 컴파일 결과(`Temp/MaterialShader`)로 셰이더를 바꿀 수 있고, Client는 그래프를 컴파일하지 않기 때문이다. 그래프로 바꾸려면 머티리얼을 프로젝트로 복제한다. 계획 3.4절의 "기본 지형 그래프로 이관"은 이 범위로 정리한다(고정 셰이더 제거는 하지 않음).
- **검사.** `ShaderGraphCheck`에 기본 지형 그래프 사례: 지형 생성기 컴파일 성공, 레이어 합성·레이어 1 절벽·경사·smoothstep 포함, PBR 값이 머티리얼 상수가 아닌 고정값, 표면 머티리얼로는 거부.
- **T3 완료 기준 대비.** "그래프 수정이 지형에 반영되고 그림자·피킹 회귀 없음"은 실행 확인 대기(11.7.1절 T3-b2·T3-c 항목).
