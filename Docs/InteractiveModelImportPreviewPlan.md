# 대화형 3D 모델 임포트 프리뷰 시스템 개발 계획 (Interactive Model Import Preview Plan)

- 작성일: 2026-11-04
- 상태: 계획 수립 및 기반 인프라(Phase 1) 구현 완료, 모달 통합(Phase 2) 진행 중
- 연계 문서:
  - [통합 개발 우선순위 (DevelopmentPriorities.md)](DevelopmentPriorities.md)
  - [엔진·에디터·클라이언트 개발 지침 (EngineEditorClientGuidelines.md)](EngineEditorClientGuidelines.md) 16절(3D 뷰포트 일원화), 17절(가변 스플리터)
  - [리소스 및 직렬화 가이드라인 (ReflectionArchiveGuidelines.md)](ReflectionArchiveGuidelines.md)

---

## 1. 배경 및 문제 정의

### 1.1 외부 3D 에셋의 이종 좌표계 및 파편화 문제
- Blender, Maya, 3ds Max, Blender glTF Exporter 등 3D 저작 툴과 포맷마다 좌표계(Right-Handed vs Left-Handed), 업 벡터(+Y Up vs +Z Up), 스케일 단위(cm, m, inch 등 0.01x ~ 100x), 정점 와인딩 순서(CW vs CCW)가 상이하다.
- 예: glTF/GLB(오른손 +Y Up) 모델(`CesiumMan.glb` 등)을 엔진으로 임포트할 때, Assimp의 `aiProcess_MakeLeftHanded`가 Z축 반전 및 쿼터니언을 비틀며 모델이 거꾸로 뒤집히거나(Pitch 180도 회전) 크기가 과도하게 작거나 커지는 문제가 발생한다.

### 1.2 사전 시각적 피드백의 부재
- 기존 시스템에서는 외부 모델을 드롭하거나 임포트할 때 플래그(`MakeLeftHanded`, `FlipUVs`, `UniformScale`, `RotationOffset`)를 지정하더라도, 실제 변환 결과가 어떻게 나올지 미리 볼 수 없다.
- 결과적으로 아티스트나 개발자가 에셋을 임포트 $\rightarrow$ 씬/에디터에 배치 $\rightarrow$ 뒤집힘 확인 $\rightarrow$ 에셋 삭제 $\rightarrow$ 플래그를 바꿔 다시 임포트하는 비효율적인 시행착오를 반복해야 한다.
- 또한 원본 `.glb`, `.gltf`, `.fbx` 파일은 엔진 내부 리소스 포맷(`.bammodel`, `.bammesh`)이 아니므로 콘텐츠 브라우저에서 기본 큐브 아이콘으로만 표시되어 식별이 어렵다.

---

## 2. 목표 및 핵심 설계 원칙

### 2.1 대화형 3D 모델 임포트 프리뷰 모달 구축
- 외부 3D 파일을 드래그 앤 드롭하거나 임포트할 때 표시되는 [Model Import Options] 모달 창 내에 **실시간 대화형 3D 프리뷰어**를 내장한다.
- 좌측 3D 뷰포트에서 모델을 마우스로 회전/패닝/줌(`Alt + LMB/MMB/RMB`)하며 관찰하고, 우측 옵션 패널에서 스케일, 회전 오프셋(예: "Pitch 180" 원클릭 버튼), 좌표계 플래그를 조절할 수 있도록 한다.

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
- [x] **`ModelConverter` & `ModelImporter` 옵션 확장**:
  - `ModelConversion::Options` 및 `ModelImportDesc`에 `UniformScale`(float), `RotationOffset`(array<float, 3>) 추가.
  - Assimp 임포트 직후 루트 노드 변환 행렬에 스케일 및 오일러 회전 적용.
  - `Editor.csv`에 `ModelImport.*` 로캘 키 추가.

---

### Phase 2: 임포트 모달 내 대화형 3D 프리뷰어 통합 (현재 작업)
- [ ] **`ContentBrowserPanel` 3D 프리뷰 위젯 임베딩**:
  - `ContentBrowserPanel`에 `PreviewViewportWidget m_ImportPreviewWidget;` 멤버 추가.
  - `ContentBrowserPanel::Initialize`에서 `m_ImportPreviewWidget.Initialize(L"ImportModalPreview", 320, 320, true)` 초기화.
  - `ContentBrowserPanel::Update(dt)`에서 모달 활성화 시 `m_ImportPreviewWidget.BeginFrame(dt)` 및 `m_ImportPreviewWidget.Submit(dt)` 호출로 백엔드 렌더링 파이프라인 등록.
- [ ] **임시 프리뷰 씬 및 엔티티 생성**:
  - 외부 모델 파일(`m_PendingModelImportPath`) 감지 시, `PreviewViewportWidget`의 내부 프리뷰 씬에 임시 엔티티 생성.
  - 메쉬/머티리얼 임시 빌드 및 `RenderMeshComponent` 바인딩.
  - 모델의 AABB(Axis-Aligned Bounding Box)를 계산하여 카메라 거리 자동 피팅(`FocusOnEntity` 호출).
- [ ] **실시간 파라미터 동기화**:
  - 모달의 `UniformScale` 및 `RotationOffset` 슬라이더를 프리뷰 엔티티의 `TransformComponent`와 바인딩.
  - "Pitch 180" 원클릭 버튼 클릭 시 프리뷰어가 즉각 정방향으로 회전함을 확인 가능.
- [ ] **모달 레이아웃 고도화**:
  - 수평 가변 스플리터를 통해 좌측 3D 뷰포트, 우측 옵션 트리(Transform, Meshes & Materials, Animation & Rigging) 배치.
  - 모달 하단에 [Import], [Cancel] 버튼 및 진행 상태 표시.

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

## 4. 완료 기준 (Acceptance Criteria)

1. **시각적 정확성**:
   - [Model Import Options] 모달 창을 열었을 때, 원본 모델이 좌측 3D 뷰포트에 정상 렌더링되어야 한다.
   - PBR 조명과 바닥 그리드가 함께 표시되어 모델의 크기와 방향을 직관적으로 가늠할 수 있어야 한다.
2. **실시간 조작성**:
   - `Alt + 마우스 버튼`으로 모델 주위를 자유롭게 회전/패닝/줌할 수 있어야 한다.
   - 스케일 및 회전 슬라이더 조작 시 60 FPS로 끊김 없이 모델의 크기와 방향이 즉각 회전/스케일링되어야 한다.
3. **가변 레이아웃**:
   - 가변 스플리터를 통해 좌측 뷰포트와 우측 옵션 창의 너비를 마우스 드래그로 조절할 수 있어야 한다.
4. **일관성 및 안정성**:
   - [Cancel]을 누르거나 모달을 닫았을 때 임시 프리뷰 씬 자원이 누수 없이 안전하게 해제되어야 한다.
   - 최종 [Import] 시 모달에서 조절한 스케일과 회전 오프셋이 실제 에셋 변환 결과에 100% 동일하게 반영되어야 한다.
