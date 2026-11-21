# 대화형 3D 모델 임포트 프리뷰 시스템 개발 계획 (Interactive Model Import Preview Plan)

- 작성일 2026-11-04 · 최종 갱신 2026-11-17
- Phase 1·2에 이어 배치 회전, 재임포트 머티리얼 보존, 원본 URI 추적, 생성물별 폴더 분류를 반영했다. 사용자 실행 결과와 남은 검증은 Phase 6에 기록한다.
- 연계 문서:
  - [통합 개발 우선순위 (DevelopmentPriorities.md)](DevelopmentPriorities.md)
  - [엔진·에디터·클라이언트 개발 지침 (EngineEditorClientGuidelines.md)](EngineEditorClientGuidelines.md) 16절(3D 뷰포트 일원화), 17절(가변 스플리터)
  - [리소스 및 직렬화 가이드라인 (ReflectionArchiveGuidelines.md)](ReflectionArchiveGuidelines.md)

---

## 1. 배경 및 문제 정의

### 1.1 외부 3D 에셋의 이종 좌표계 및 파편화 문제
- Blender, Maya, 3ds Max, Blender glTF Exporter 등 3D 저작 툴과 포맷마다 좌표계(Right-Handed vs Left-Handed), 업 벡터(+Y Up vs +Z Up), 스케일 단위(cm, m, inch 등 0.01x ~ 100x), 정점 와인딩 순서(CW vs CCW)가 상이하다.
- glTF/GLB는 오른손 좌표계에서 `+Y`를 위, `+Z`를 정면으로 사용한다. Assimp의 왼손 변환은 `Z`를 반사하므로 이 변환만 적용하면 모델 정면이 `-Z`로 바뀐다.

### 1.2 사전 시각적 피드백의 부재
- 기존 시스템에서는 외부 모델을 드롭하거나 임포트할 때 플래그(`MakeLeftHanded`, `FlipUVs`, `UniformScale`, `RotationOffset`)를 지정하더라도, 실제 변환 결과가 어떻게 나올지 미리 볼 수 없다.
- 결과적으로 아티스트나 개발자가 에셋을 임포트 $\rightarrow$ 씬/에디터에 배치 $\rightarrow$ 뒤집힘 확인 $\rightarrow$ 에셋 삭제 $\rightarrow$ 플래그를 바꿔 다시 임포트하는 비효율적인 시행착오를 반복해야 한다.
- 또한 원본 `.glb`, `.gltf`, `.fbx` 파일은 엔진 내부 리소스 포맷(`.bammodel`, `.bammesh`)이 아니므로 콘텐츠 브라우저에서 기본 큐브 아이콘으로만 표시되어 식별이 어렵다.

---

## 2. 목표 및 핵심 설계 원칙

### 2.1 대화형 3D 모델 임포트 프리뷰 모달 구축
- 외부 3D 파일을 드래그 앤 드롭하거나 임포트할 때 표시되는 [Model Import Options] 모달 창 내에 **실시간 대화형 3D 프리뷰어**를 내장한다.
- 좌측 3D 뷰포트에서 모델을 마우스로 회전/패닝/줌(`Alt + LMB/MMB/RMB`)하며 관찰하고, 우측 옵션 패널에서 스케일, 수동 회전 오프셋, 상하·헤딩 180° 토글과 좌표계 플래그를 조절한다.

### 2.2 핵심 설계 원칙

1. **3D 뷰포트 및 렌더링 일원화 (`EngineEditorClientGuidelines.md` 16절 준수)**:
   - 독자적인 간이 렌더러 대신 공통 컴포넌트인 `PreviewViewportWidget`을 활용한다.
   - `ViewportDeferredPipeline`을 통해 G-Buffer 생성, PBR 디퍼드 라이팅, SkyAtmosphere/IBL, ACES Filmic 톤매핑, 바닥 기준 그리드(`SubmitGrid`)를 메인 뷰포트와 완전히 동일한 품질로 제공한다.
   - `EditorCameraController`를 통해 Orbit, Pan, Zoom 조작계를 표준화하고, 뷰포트 호버 시에만 휠 스크롤을 소비하여 모달 스크롤과의 간섭을 차단한다.

2. **반응형 가변 스플리터 (`EngineEditorClientGuidelines.md` 17절 준수)**:
   - 좌측 3D 뷰포트와 우측 임포트 옵션 인스펙터를 하드코딩된 크기로 고정하지 않고, 수평 리사이저블 스플리터(`Splitter`)로 분할하여 사용자가 원하는 비율로 자유롭게 조절할 수 있도록 한다.

3. **무지연(Zero-Lag) 실시간 인터랙션 아키텍처**:
   - `UniformScale` 슬라이더나 `RotationOffset` 조작 시 무거운 파일 재파싱(Assimp Re-import)을 거치지 않는다.
   - 모달 초기 오픈 시 원본 모델을 프리뷰 전용 격리 씬(`Engine::Scene`)에 임시 인스턴스화하고, 슬라이더 변경 시 해당 엔티티의 `TransformComponent`에 스케일과 회전을 즉각 적용(60 FPS 실시간 반영)한다.
   - [Import] 버튼을 누를 때 최종 결정된 파라미터가 비동기 임포터(`AssetManager::ImportAsync`)로 전달되어 디스크 파일(`.bammodel`, `.bammesh`, `.bammat.json`)로 영속화된다.

4. **로캘 원본 및 UI 지침 준수 (`EngineEditorClientGuidelines.md` 14절 준수)**:
   - 모든 텍스트 라벨과 툴팁은 `Resources/Data/Localization/Editor.csv`에 등록된 키(`LOCAL_CSTR`, `LOCAL_LABEL`)를 사용한다.
   - JSON 번역본 직접 수정을 금지하고 CSV 원본을 유지한다.

---

## 3. 단계별 상세 개발 계획

### Phase 1: 뷰포트 및 변환 인프라 기반 구축 (완료)
- [x] **`EditorCameraController` 고도화**:
  - `Alt + LMB` 피벗 중심 궤도 회전(Orbit) 및 `m_PivotPoint` 지원.
  - `Alt + MMB` / `MMB` 시점 평면 패닝(Pan).
  - `Alt + RMB` 및 마우스 휠 줌(Zoom).
  - 뷰포트 호버 여부에 따른 휠 스크롤 제한(`allowScroll`)으로 UI 스크롤 오염 방지.
- [x] **`PreviewViewportWidget` 안정화**:
  - 포스트프로세스 체인(`ExecuteChain`) 이후 디스플레이 RT에 바닥 그리드(`m_Grid.SubmitGrid`)를 제출하도록 렌더링 순서 교정.
  - 정지 상태에서 본 행렬을 `Identity`로 초기화하여 원본 T-포즈(바인드 포즈) 정상 보장.
  - `InvisibleButton`과 오버레이 툴바 `BeginChild`의 Z-order 분리로 툴바 버튼(그리드, 포커스, 재생) 클릭 먹통 해결.
- [x] **`ModelEditor` 프리뷰어 장착**:
  - 기존 2D 프로퍼티 전용 창에서 `PreviewViewportWidget`을 결합한 복합 에디터로 확장.
  - 수평 가변 스플리터(`Splitter`) 적용.
  - Content Browser에서 `.bammesh` 또는 `.bammesh.json`을 열면 같은 창에서 해당 메시를 읽기 전용으로 표시한다. 기본 머티리얼로 형상을 렌더링하고 정점·인덱스 수를 표시한다. 스킨 데이터가 있는 메시도 단독 보기에서는 정적 형상으로 표시한다.
- [x] **`ModelConverter` & `ModelImporter` 옵션 확장**:
  - `ModelConversion::Options` 및 `ModelImportDesc`에 `UniformScale`(float), `RotationOffset`(array<float, 3>) 추가.
  - 임포트 배율은 메시 정점, 모든 노드 및 본의 이동 성분, 본 오프셋의 이동 성분, 애니메이션 위치 키에 베이크한다. 노드·본·애니메이션의 스케일 값은 원본 값을 유지한다. 엔티티 Scale이 `1`인 메시 직접 바인딩과 모델 배치에 같은 기본 배율이 적용된다.
  - 왼손 좌표계의 `+Y`를 위, `+Z`를 정면으로 사용한다. GLB와 glTF는 기본적으로 왼손 변환과 와인딩 반전을 적용하고, Assimp의 Z 반사 뒤 Y축 180°를 적용해 원본 `+Z` 정면을 유지한다. 모달에서 왼손 변환을 변경하면 와인딩 반전도 함께 바뀌며, 와인딩은 개별 조정할 수 있다.
  - 상하·헤딩 180°는 메시와 노드, 스켈레톤 및 애니메이션 좌표에 함께 반영한다. 수동 XYZ 회전 오프셋은 모델 배치용 루트에 적용한다.
  - `Editor.csv`에 `ModelImport.*` 로캘 키 추가.

---

### Phase 2: 임포트 모달 내 대화형 3D 프리뷰어 통합 및 핫 리로드 (완료)
- [x] **모달 클래스 독립 분리 (`ModelImportModal`)**:
  - `ContentBrowserPanel`에 비대하게 위치하던 모달 코드를 독립 패널/모달 클래스(`Editor/Source/AssetManager/Importer/Public|Private/ModelImportModal.h|.cpp`)로 분리.
  - `ImGuiManager`의 기본 패널로 등록 및 싱글턴 인스턴스(`ModelImportModal::Get()`) 연동.
- [x] **인플레이스 핫 리로드(Hot Reload) 및 재임포트 크래시 방어**:
  - 기존 모델/메쉬/스켈레톤/애니메이션/머티리얼인스턴스가 이미 로드되어 있을 때 `RegisterResource` 중복 키 등록으로 인한 `MaterialInstance registration failed` 및 `0xC0000005` 충돌 원천 차단.
  - 기존 핸들이 유효할 경우 `existingResource->Reload(key)`를 호출하는 안전한 인플레이스 갱신 파이프라인 구축.
  - 부모 머티리얼 에셋 누락 방어 및 안전한 역직렬화 보장.
- [x] **`ContentBrowserPanel` 3D 프리뷰 위젯 임베딩**:
  - `PreviewViewportWidget m_ImportPreviewWidget;` 멤버 추가 및 초기화.
  - `Update(dt)`에서 모달 활성화 시 렌더 파이프라인 연동.
- [x] **실시간 파라미터 조절 UI**:
  - 모달은 X/Y축 180° 보정 뒤 수동 XYZ 회전을 적용한 결과를 실시간으로 보여준다. 초기화는 수동 각도와 두 토글을 함께 지운다. 임포트 프리뷰와 모델·메시 썸네일은 같은 `+Z` 정면 대각선 각도를 사용한다.
  - 임포트 모달과 모델 에디터는 모델을 열거나 포커스를 요청할 때 메시 바운드에 현재 노드 변환을 적용하고 합친다. 모델·메시 썸네일과 같은 화면 맞춤 공식을 사용하며 가로·세로 중 좁은 시야각에 맞춰 거리를 계산한다.

#### 재임포트 안정화 중간 기록 (2026-11-07)

- 동일 출력 디렉터리의 동시 임포트를 막고, 부모 머티리얼과 출력 경로를 사전 검사한다. 기존 리소스를 덮어쓸 때 핸들을 유지하며 저장 모드에 따라 파일 또는 메모리에서 갱신한다.
- 메시 파일은 모두 임시 경로에 저장한 뒤 게시한다. 커밋 전에 실패하면 게시한 파일과 이미 갱신한 메모리 메시를 복구한다. 프레임 사이에 에디터가 정상 종료되어 작업이 폐기되는 경우에도 상태 소멸자가 게시한 메시 파일을 복구한다.
- 임베디드 텍스처의 저장·재로드 실패를 임포트 결과에 전달한다. 메모리 전용 임포트는 임베디드 이미지를 인코딩된 메모리에서 읽으며, 파일 저장 경로는 기존 썸네일 캐시 무효화를 요청한다.
- 묶음 복구의 현재 범위는 메시다. 모델·스켈레톤·애니메이션·머티리얼·임베디드 텍스처까지 포함하는 다중 파일 커밋은 후속 범위다.
- 다음 세션은 `ModelImporter.cpp`에서 사용하는 `AssetCache.h`와 `<cstdio>`의 직접 포함을 확인하고 Editor를 빌드하는 작업부터 시작한다. 그 뒤 폐기 가능한 모델로 `Overwrite`와 `SaveToFile` 조합, 기존 핸들 유지, 중간 저장 실패와 부모 머티리얼 누락, 일부 메시 게시 직후 정상 종료를 검증한다. 실패·종료 후 이전 메시 파일 해시와 로드된 메시 상태를 비교한다.

---

### Phase 3: 외부 3D 에셋 썸네일 생성 및 캐싱 (후속)
- [ ] **오프라인 썸네일 베이커**:
  - 콘텐츠 브라우저에서 `.glb`, `.gltf`, `.fbx`, `.obj` 파일 발견 시 백그라운드 태스크로 경량 렌더링 수행.
  - 64x64 또는 128x128 RGBA 텍스처를 생성하여 에디터 썸네일 캐시 폴더(`.bam/thumbnails/`)에 저장.
  - `ContentBrowserGridItem`에서 해당 썸네일을 표시하여 탐색기 수준의 시각적 식별성 확보.

---

### Phase 4: 프리셋 시스템 및 종합 검증
- [ ] **원클릭 프리셋 드롭다운**:
  - `Blender (+Z Up, 0.01x Scale)`
  - `glTF / GLB Standard (No Invert)`
  - `glTF / GLB Inverted Fix (Pitch 180)`
  - `Unreal Engine Exported FBX`
- [ ] **검증 시나리오**:
  - `CesiumMan.glb` 드래그 앤 드롭 $\rightarrow$ 모달 오픈 시 3D 모델 표시 $\rightarrow$ "Pitch 180" 클릭 시 정방향 확인 $\rightarrow$ 임포트 실행 $\rightarrow$ `.bammodel`이 올바른 방향으로 생성되는지 검증.
  - 복잡한 스켈레탈 애니메이션 모델 및 대형 씬 모델의 바운딩 볼륨 자동 피팅 검증.

---

### Phase 5: 모델 임포트 시 자동 메쉬 LOD 생성 파이프라인 (Auto LOD Generation)

- **도입 배경**:
  - 런타임 렌더링 레이어에는 `LODGroupComponent`와 `MeshRenderSystem`의 화면 커버리지(Screen Coverage) + 히스테리시스 기반 LOD 전환 파이프라인이 완성되어 있으나, 외부 모델 임포트 시 수동으로 LOD를 제작하지 않으면 이 시스템을 활용할 수 없음.
  - 모델 임포트 시점에 원본(LOD0)으로부터 간소화된 메쉬를 알고리즘으로 자동 생성하여 에셋 파이프라인의 완성도를 높임.
- **핵심 기술 및 라이브러리**:
  - 게임 엔진 업계 표준인 **`meshoptimizer` (`meshopt_simplify`)** 도입.
  - 정점 락킹(UV Seam 및 본 가중치 경계선 보존)을 지원하는 Quadric Error Metrics(QEM) 알고리즘 기반 메쉬 간소화.
- **임포트 옵션 확장 (`ModelImportDesc` / `ModelImportModal`)**:
  - `bool GenerateLODs = true;`: 자동 LOD 생성 활성화 여부
  - `uint32 LODCount = 3;`: 생성할 LOD 단계 수 (최대 4단계)
  - `std::vector<float> LODReductionRates = { 0.5f, 0.25f, 0.12f };`: 각 단계별 목표 삼각형 수 비율
  - `float TargetError = 0.01f;`: 허용 가능한 최대 기하 오차(Error Metric)
- **에셋 및 엔티티 연동**:
  - 임포트 시 원본 메쉬(`ModelName_Mesh.bammesh`) 외에 파생 메쉬 파일(`ModelName_Mesh_LOD1.bammesh`, `_LOD2.bammesh`)을 자동 디스크 영속화.
  - `.bammodel` 및 씬 인스턴스화 시 엔티티에 `LODGroupComponent`를 자동 부착하고 화면 비율 임계값(`screenCoverageThresholds`)을 자동 초기화.


---

### Phase 6: 하이브리드 모델 에셋 정리 파이프라인 (Hybrid Source-to-Asset Pipeline)

선택 모델 기준의 재임포트, 생성물 전체 복구와 이동 후 참조 검증은 [모델 임포트·재임포트·에셋 이동 개선 계획](AssetReimportRelocationWorkflowPlan.md)에 정리했다.

새로 임포트한 모델은 콘텐츠 브라우저에서 선택한 출력 폴더에 대표 모델과 스켈레톤을 저장한다. 생성물이 있을 때 메시(LOD 포함), 머티리얼 인스턴스, 임베디드 텍스처, 애니메이션 클립을 각각 하위 폴더에 저장한다.

```text
Assets/
├─ Source/Models/Characters/Barbarian.glb
└─ Models/Characters/Barbarian/
   ├─ Barbarian.bammodel
   ├─ Barbarian_Skeleton.bamskel
   ├─ Meshes/       # *.bammesh, LOD
   ├─ Materials/    # *.bammatinst
   ├─ Textures/     # 추출한 임베디드 이미지
   └─ Animations/   # *.bamanim
```

그림의 원본 위치와 모델 출력 폴더는 사용자가 정한다. 임포트는 원본 파일을 읽고, 외부 텍스처는 기존 경로 해석 규칙을 따른다. 모델 전용 출력 폴더 자동 생성과 원본·동반 파일의 자동 보관은 후속이다.

- `.bammodel`은 프로젝트 자산 루트 안 원본의 `Project://` URI와 리소스 종류별 하위 폴더 사용 여부를 저장한다. 기존 모델 파일에 해당 필드가 없으면 원본 URI는 비어 있고 각 출력 설정은 평탄한 폴더를 기본으로 한다.
- 콘텐츠 브라우저에서 `.bammodel`의 **옵션과 함께 다시 임포트**를 선택하면 저장된 원본을 열고, 생성물은 모델 파일의 상위 폴더에 갱신한다. 원본 URI가 비어 있는 모델에는 같은 폴더의 동일 이름 원본 탐색을 사용한다. 저장된 원본이 사라지거나 이름이 일치하지 않으면 재임포트 실패를 표시한다.
- 재임포트는 기존 모델의 리소스 종류별 폴더 설정을 유지한다. 이미 씬·컨트롤러가 참조하는 생성물을 이동하려면 참조 갱신을 포함하는 별도 이관이 필요하다.
- 기존 머티리얼 인스턴스는 기본으로 재사용한다. 원본 값으로 다시 만들려면 모달의 **기존 머티리얼 덮어쓰기**를 선택한다. 이 옵션으로 갱신한 머티리얼까지 포함하는 다중 파일 실패 복구는 후속이다.
- 수동 XYZ 회전은 모델 파일의 배치 회전으로 저장한다. 새 모델 인스턴스의 최상위 엔티티와 모델 썸네일에 적용하고, 본의 기준 자세와 클립 키에는 별도로 굽지 않는다. 씬에 이미 배치된 엔티티의 회전은 씬에 저장된 값을 따른다. 모델 변환 캐시의 버전을 올려 이전 방식으로 회전이 포함된 변환 데이터를 구분했다.

#### 2026-11-17 검증 기록

- 사용자 실행에서 도끼의 머티리얼 보존과 선택적 덮어쓰기를 확인했다. 원본과 모델을 서로 다른 폴더에 둔 상태에서 모델 메뉴로 재임포트하는 동작도 사용자 확인을 받았다.
- 사용자가 활의 `Meshes/`와 `Materials/` 분류를 화면에서 확인했다. Fox 테스트 출력에는 대표 모델과 스켈레톤, 기본 메시와 두 LOD, 머티리얼, 임베디드 PNG, `Walk`·`Run`·`Survey` 세 클립이 지정된 폴더에 있음을 디스크와 임포트 로그에서 확인했다.
- Rogue 기준 자세와 구형 KayKit 클립의 다리 본 좌표가 달랐다. 클립을 현재 임포터에서 Rogue와 일치하는 좌표 변환 설정으로 다시 생성한 뒤 정상 재생을 사용자에게 확인했다.
- Fox의 수동 Y 회전과 재생 방향은 사용자 예비 확인 상태다. 고정 카메라에서 재임포트·재개방·신규 배치까지 비교하는 절차와 커밋 대상에서의 빌드·CTest는 별도 검증으로 남긴다.

후속 범위는 모델 전용 폴더와 원본 보관 자동화, 외부 참조가 있는 원본 묶음의 복사·배포 제외 검증, 기존 에셋 이동과 참조 갱신, 대표 에셋 필터다.

---

## 4. 완료 기준 (Acceptance Criteria)

1. **시각적 정확성**:
   - [Model Import Options] 모달 창을 열었을 때, 원본 모델이 좌측 3D 뷰포트에 정상 렌더링되어야 한다.
   - PBR 조명과 바닥 그리드가 함께 표시되어 모델의 크기와 방향을 직관적으로 가늠할 수 있어야 한다.
2. **실시간 조작성**:
   - `Alt + 마우스 버튼`으로 모델 주위를 자유롭게 회전/패닝/줌할 수 있어야 한다.
   - 스케일 및 회전 슬라이더 조작 시 60 FPS로 끊김 없이 모델의 크기와 방향이 즉각 회전/스케일링되어야 한다.
3. **가변 레이아웃**:
   - 가변 스플리터를 통해 좌측 뷰포트와 우측 옵션 창의 너비를 마우스 드래그로 조절할 수 있어야 한다.
   - 좁은 옵션 영역에서 파일명과 경로는 줄바꿈되고, 스케일 프리셋과 X/Y/Z 입력은 필요한 만큼 다음 줄로 내려간다. 한국어와 영어에서 확인한다.
4. **일관성 및 안정성**:
   - [Cancel]을 누르거나 모달을 닫았을 때 임시 프리뷰 씬 자원이 누수 없이 안전하게 해제되어야 한다.
   - 최종 [Import] 시 모달에서 조절한 스케일과 회전 오프셋이 실제 에셋 변환 결과에 100% 동일하게 반영되어야 한다.
   - 같은 스킨드 원본을 `1.0`과 `0.01`로 임포트해 메시 정점과 Bounds, 본 오프셋·로컬 이동, 애니메이션 위치 키가 같은 배율로 바뀌는지 확인한다. 회전·스케일 키는 원본 값을 유지한다.
   - `RaytracingScene3`의 Fox 메시 직접 바인딩과 `.bammodel` 신규 배치를 각각 엔티티 Scale `1`로 확인한다. 재생 전후 배율이 유지되고, 모델 배치에서 임포트 배율이 중복 적용되지 않아야 한다. 기존 에셋은 원본에서 같은 배율로 재임포트한다.
   - `Fox.glb`를 기본 좌표계 옵션으로 다시 임포트한다. 임포트 모달과 모델·메시 썸네일에서 얼굴이 보이는지 확인하고, 씬에서 메시를 직접 바인딩해 이동 방향과 애니메이션 포즈를 확인한다. 상하·헤딩 토글을 각각 켜면 같은 회전이 프리뷰, 모델 배치, 메시 직접 바인딩에 적용되어야 한다.
   - Fox를 `1`과 `0.1`로 미리 보고 포커스를 다시 잡는다. 임포트 모달과 모델 에디터를 세로로 좁혀도 포커스 시 모델 전체가 들어가야 하며, 썸네일은 두 배율에서 비슷한 화면 비율을 유지해야 한다.
5. **하이브리드 디렉터리 정리 및 소스 추적**:
   - 하이브리드 레이아웃 임포트 시 대상 위치 아래 모델 전용 폴더와 `Meshes/`, `Materials/`, `Textures/`, `Animations/`가 자동 생성되어 파일들이 분류되어야 한다.
   - 복합 모델(`.bammodel`)에는 원본 소스 경로가 보존되어 원본 수정 후 재임포트 시 서브폴더 구조를 깨지 않고 갱신되어야 한다.
