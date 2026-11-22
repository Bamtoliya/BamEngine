# 코드 가독성·응집도·일관성 개선 계획

- 작성일: 2026-11-13
- 상태: 후보 조사 완료. **보류: 기능 개발이 모두 끝난 마무리 단계에서 착수한다(사용자 결정, 2026-11-13).** 그 전에는 이 문서의 어떤 후보도 착수하지 않는다.
- 작업 방식: 동작을 보존하는 작은 단위의 점진적 리팩터링
- 공통 기준: [SourceOrganizationGuidelines.md](SourceOrganizationGuidelines.md), [EngineEditorClientGuidelines.md](EngineEditorClientGuidelines.md)
- 전체 작업 순서: [DevelopmentPriorities.md](DevelopmentPriorities.md)

## 1. 목표와 범위

사람이 코드를 처음 읽을 때 기능의 소유자와 호출 흐름을 찾기 쉽고, 한 기능의 변경이 관련 없는 책임까지 흔들지 않도록 한다. 이를 위해 이름이나 폴더 모양을 일괄 변경하기보다 다음을 근거로 정리한다.

- 한 클래스와 파일 안에서 함께 변경되는 책임이 실제로 모여 있는가?
- 반복된 조건·상태 처리에 공통 규칙이 있는가?
- 모듈의 의존 방향, 데이터·자원 수명과 공개 계약이 분명한가?
- 기존 테스트와 사용 흐름으로 동작 보존을 확인할 수 있는가?

대상은 `Engine`, `Editor`, `Client`, `Projects`, `Tools`의 유지보수 코드다. `ThirdParty`, 에셋, 생성 결과와 빌드 산출물은 제외한다. 초기 확장자 조사(`.h`, `.hpp`, `.cpp`, `.inl`, `.c`, `.cc`, `.py`, `.ps1`, `.cmake`)에서 965개 파일을 세었지만, 이는 전체 파일의 줄 단위 검토나 결함 판정을 뜻하지 않는다.

이번 계획은 코드 구조 개선에 한정한다. 버그 수정, 기능 추가, 공개 API·ABI 및 직렬화 형식 변경은 리팩터링에 섞지 않고 별도 근거와 승인으로 다룬다.

## 2. 진행 원칙

1. 구현 전 기준 경로와 소유권을 기록하고, 가능하면 기존 동작을 재현하는 검사를 먼저 찾는다.
2. 한 번에 한 책임 또는 작은 파일 쌍만 바꾼다. 이동, 이름 변경, API 변경, 동작 변경을 한 묶음으로 처리하지 않는다.
3. 파일 크기, 오래된 스타일, 폴더 위치만으로 리팩터링하지 않는다. 실제 응집도 저하, 중복된 규칙, 변경 결합 또는 이해 비용을 확인한다.
4. 새 공통 추상화는 두 개 이상의 실제 소비자가 같은 규칙을 공유하고, 소유 위치가 분명할 때만 도입한다.
5. 입력 우선순위, Undo 트랜잭션, 자원 수명, 렌더링 경계와 저장 데이터 계약은 구조 정리와 별도로 검증한다.
6. 결과 기록에는 정적 검토, 사용자가 실행한 빌드·검사, UI/GPU 등 아직 확인하지 않은 동작을 구분한다.

## 3. 초기 후보와 우선순위

아래 순위는 코드 선언·함수 구성을 확인해 정한 **조사 우선순위**다. 리팩터링이 필요하다고 확정한 목록이 아니며, 각 항목의 첫 단계는 책임과 변경 경계를 확인하는 것이다.

| 순위 | 후보 | 확인된 구조와 이유 | 첫 리팩터링에서 보존할 계약 |
|---|---|---|---|
| 1 | `Editor/Source/ImGui/ViewportPanel/Private/SceneViewportPanel.cpp`, 대응 헤더 | 하나의 뷰포트에서 렌더링, 선택·피킹, 기즈모, 지형 조각, 타일맵 페인팅, 스플라인 편집을 조율한다. 헤더에도 타일맵 드래그, 지형 편집, 스플라인 선택·드래그의 별도 상태가 있다. 서로 다른 편집 상태의 소유자와 입력 경계를 먼저 조사할 가치가 가장 높다. | `DrawCustomViewport()`의 상호작용 우선순위, 입력 캡처 조건, Undo 시작·종료 단위, 선택과 씬 수명. 첫 파일럿 후보는 독립된 스플라인 편집 상태지만, 소유권을 분리해도 호출 순서가 보존되는지 확인한 뒤 결정한다. |
| 2 | `Editor/Source/ImGui/InspectorPanel/Private/InspectorPanel.cpp`, `ReflectionPropertyDrawer.cpp` | 인스펙터는 다중 선택 공통 속성, 반사 속성, 단일 엔티티 속성, 프리팹 헤더 및 모델 머티리얼 갱신 경로를 가진다. 반사 편집기 구현 파일도 이미 있으므로 중복 여부와 기존 재사용 경계를 확인해야 한다. | 다중 선택 시 공통 값 편집, 반사 편집 검증, 저장·Undo 동작. 공통화를 위해 두 번째 반사 편집 경로를 만들지 않는다. |
| 3 | `Editor/Source/ImGui/AnimatorGraphPanel/Private/AnimatorGraphPanel.cpp` | 문서 열기·저장, 파라미터, 캔버스·전이, 선택 인스펙터, 미리보기와 문서 복구 확인이 한 패널 클래스에 모여 있다. 그래프 편집기라는 사용 흐름은 하나이므로 기능 이름별 분리보다 상태 소유권과 테스트 경계가 실제로 나뉘는지 먼저 판단한다. | 문서 dirty 상태와 저장 보호, 선택·전이 편집, 미리보기 상태 및 리소스 수명. 독립성이 입증되지 않으면 파일 구역 정리만 한다. |
| 4 | `Engine/Source/Render/RHI/Platform/DirectX12/Private/DirectX12RHI.cpp` 및 `SDLGPURHI.cpp` | 백엔드 구현 파일이 크고 자원 생성, 바인딩, 제출, 동기화와 계측을 포함한다. 두 백엔드의 API 구현을 비교해 실제 반복·의미 불일치가 있는지 확인할 수 있지만, GPU 자원 수명과 성능 경로의 변경 위험이 높다. | RHI 계약, 백엔드별 지원 차이, GPU 완료 후 해제, 리소스 상태 전이와 성능. 비교 결과가 나오기 전에는 공통 추상화나 파일 분할을 계획에 확정하지 않는다. |
| 5 | 반복되는 에디터 UI 패턴 | 검색·필터, 리스트/그리드 선택, 빈 상태, 모달 등의 반복 코드가 실제로 같은 상호작용 규칙을 구현하는지 조사한다. 유사한 모양만으로 공용 위젯을 만들지 않는다. | `EngineEditorClientGuidelines.md`의 UI·로캘·입력 규칙과 각 패널의 기존 상호작용. 중복이 확인된 한 패턴부터 적용하고 사용 화면에서 확인한다. |
| 6 | `Engine/Source/World/Scene/Public/SceneManager.h`, `SceneManager.cpp`, `Scene.h` | `SceneManager::SavePrefab/InstantiatePrefab`이 `Scene Management` 영역 안에서 파일 I/O, 로깅, `PrefabInstanceComponent` 부착을 수행한다. 같은 프리팹 인스턴스화를 파일 기반은 SceneManager 경유(HierarchyPanel, ContentBrowserGridItem, InspectorPanel, SceneViewportPanel), 아카이브 기반은 `Scene::InstantiatePrefabArchive` 직접 호출(EntityClipboard, PrefabModeManager, PrefabEditorPanel)로 호출해 경로가 나뉜다. `PrefabInstanceComponent` 부착은 파일 경로 방식에서만 일어나며 의도 여부는 미확인이다. `Scene.h`는 엔티티·계층 관리, 모델 인스턴스화와 링크 갱신(`InstantiateModel`, `InstantiateLinkedModel`, `RefreshLinkedModelParts`, `Restore/ReplaceLinkedModelPart*`), 직렬화·프리팹(`WriteArchive`, `*PrefabArchive`)을 한 공개 API로 노출한다. 구현 파일은 이미 분리되어 있으나(SceneModel.cpp 약 22KB, SceneArchive.cpp 약 29KB, Scene.cpp 약 8KB) 클래스 경계는 그대로이므로, 모델 인스턴스화 계열도 같은 조사 범위에 넣고 호출처(ModelEditor, SceneViewportPanel, SceneArchiveCheck)를 먼저 확인한다. | 직렬화 형식, `SceneArchiveCheck`의 프리팹 저장·인스턴스화·롤백 결과, 공개 API(호출처 이전 전까지 래퍼 유지), 에디터 전용 코드의 Client 제외 가능성. 첫 단계는 `PrefabInstanceComponent` 부착 의도 확인이며, 확정 방향은 `SceneManager`에 프리팹 함수를 남기지 않는 것이다(사용자 결정). `Graph/Public/GraphFile.h`(`SaveGraphJson`/`LoadGraphJson`)의 선례에 맞춰 `Scene&`를 받는 네임스페이스 함수(가칭 `PrefabFile::Save/Instantiate`)로 옮기고 호출처를 이전한 뒤 `SceneManager`의 두 메서드를 제거한다. `Prefab`을 `Resource` 상속 클래스로 승격하는 것은 핸들 참조·캐시가 실제로 필요해질 때 검토한다. `Scene::WritePrefabArchive/InstantiatePrefabArchive`는 스테이징 씬과 롤백이 Scene 내부 상태에 묶여 있어 유지하고, `SceneModel.cpp` 계열은 Scene 내부 접근이 `m_LocalRegistry` 4곳(131, 209, 294, 332행)과 `m_NextEntityId` 저장·복원 2쌍(207/282행, 570/613행)뿐이다. 레지스트리는 공개 접근자로 대체 가능할 것으로 추정하며(미확인), 롤백용 ID 복원 방식을 정하면 `ModelInstance` 계열 함수로 분리할 수 있다. 목표 구조는 `Scene.h`(엔티티·계층·레지스트리·아카이브 핵심 API), 기능 파일(`PrefabFile`, 모델 인스턴스 함수), `SceneManager.h`(현재 씬 전환·수명)의 3층이다. |
| 7 | `Engine/Source/Resource/Public/ResourceManager.h` | 핸들·슬롯 관리, 폴더·파일 임포트, 로더 등록, 디버그·메트릭, Json/Bevy/Binary 저장·로드(6쌍)와 `SaveAllRegisteredResources`가 한 클래스에 있다. 영속화는 핸들 저장소와 변경 이유가 다르다. `AddResourceInternal`, `AllocateSlot` 등 내부 성격 함수가 public 영역에 선언되어 있어 실제 접근 범위도 확인해야 한다(미확인). | 핸들 유효성·참조 카운트, 템플릿 API, 저장 형식. 핸들 계약의 파급 범위가 크므로 호출처 조사 전에는 분할을 확정하지 않는다. |
| 8 | `Engine/Source/Resource/Public/ResourceHandleArchiveAdapter.h`의 `ResolveResourceHandle` | 아카이브 어댑터 헤더 안에 리소스 해석 정책(키·경로 조회, 후보 확장자 10개 하드코딩, `.bam*` 확장자 제거 규칙, 디스크 존재 확인과 자동 `LoadFile`)이 약 110줄로 들어 있다. 후보 확장자 순회 루프가 반복된다. 같은 규칙이 다른 경로에 중복되는지는 미확인이다. | **파일럿 1단계 완료(2026-11-23, 정적 확인)**: `detail` 네임스페이스로 `kCandidateResourceExtensions` 상수 테이블과 `HasBamExtension`, `TryFindLoadedByKeyOrPath`, `TryLoadWithCandidateExtensions`, `TryResolveLegacyResourcesPath` 헬퍼를 분리하고 중복 2차 루프를 제거함(탐색 순서·공개 API 100% 보존). 실빌드 및 `SceneArchiveCheck`/`SceneReflectionCheck` 검증 대기 중. `Resource` 영역 이관은 추후 별도 단계에서 검토한다. |

### 처음부터 분할 대상으로 삼지 않을 사례

- `Projects/Reflection/include/reflection/PropertyInfo.h`(약 1900줄)는 속성 하나의 타입 소거 접근이라는 단일 개념에 속하는 헤더 전용 템플릿이다. 컨테이너·Set·Map 섹션이 더 커지면 기존 `PropertyContainerBinding.h`, `PropertyContainerAccess.h` 방향으로 이동을 검토한다.
- `Projects/ReflectionArchive/.../ReflectionArchiveAdapter.h`(약 1640줄)는 `PropertyTraversal`, `ScalarTypeDispatch` 등으로 내부 책임이 나뉘어 있다. `Projects/Archive`의 `ArchiveInterface.h`(Write/Read 영역 분리)와 `GlazeArchiveBase.cpp`(JSON/BEVE 공통부를 베이스에 모은 구조)도 응집되어 있다.
- 공통 모듈이 엔진에 의존하는 흔적은 확인한 범위(선언 위주)에서 없었다. `ReflectionRegistry.cpp`, `SceneArchive.cpp`의 본문은 아직 보지 않았다.

- `Engine/Source/Input/Public/InputManager.h`는 키보드·마우스·액션 맵이 모두 입력 상태 질의 흐름에 속한다. 액션 맵 설정·평가(`BuildActionMaps`, `ValidateInputActionSettings`)가 커지는지만 지켜본다.
- `Engine/Source/World/Transform/Public/TransformComponent.h`는 크지만 로컬 TRS와 회전 표현의 일관성 유지라는 하나의 책임에 응집되어 있다.
- `AudioManager`, `PickingManager`는 공개 API가 짧고 목적이 하나다. `Graph*Library.h`는 함수 라이브러리 성격이라 줄 수만으로 판단하지 않는다.

- `Engine/Source/Physics/Private/Collision.cpp`는 Raycast, Contains, Intersects, Sweep 등 다수 함수가 같은 충돌 계산 책임에 속한다. 크기만으로 도형별 파일 분리를 결정하지 않는다.
- `Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserPanel.cpp`에는 탐색, 필터, 그리드, 선택, 에셋 생성과 workspace 복원이 함께 있다. 넓은 기능이지만 콘텐츠 브라우저 사용 흐름으로 응집되어 있을 수 있으므로 호출·상태 경계를 확인하기 전에는 우선 분할하지 않는다.
- 큰 통합 검사와 예제는 여러 시나리오를 한 실행으로 묶은 이유를 확인한다. 줄 수 감소만을 위해 테스트를 쪼개지 않는다.

### 신규 참여자 관점의 선별 기준과 후보

목표가 "처음 보는 사람도 코드를 따라갈 수 있게 하는 것"일 때는 파일 크기보다 **진입점에서 기능을 찾는 경로가 예측 가능한가**를 기준으로 삼는다. 아래 기준으로 위 후보를 다시 분류한다.

- 이름·위치·영역(region)이 실제 책임과 일치하는가 (예: `Scene Management` 영역 안의 프리팹 함수)
- 같은 일을 하는 경로가 하나인가 (예: 프리팹 인스턴스화 호출 경로가 둘)
- 한 함수·파일을 읽을 때 아카이브·UI·정책이 뒤섞이지 않는가
- 진입 문서가 있는가 (루트 README와 `Projects/*/README.md`는 있으나, 엔진 실행 흐름·폴더 읽는 순서를 안내하는 문서는 확인하지 못했다. 미확인)

| 구분 | 후보 | 이유 |
|---|---|---|
| 먼저 | 6 (Scene/SceneManager 프리팹·모델 책임) | 영역 이름과 책임이 어긋나고 호출 경로가 둘이다. 이름만 같은 `SavePrefab`이 3곳에 있어 처음 보는 사람이 가장 쉽게 오해한다. |
| 먼저 | 8 (`ResolveResourceHandle`) | 어댑터 헤더에 해석 정책이 있어 찾기 어렵고 순회 루프가 반복된다. 함수 하나로 범위가 작다. |
| 먼저 | 7 (ResourceManager) 중 public 영역의 내부 성격 함수 노출 | 사용자가 호출해야 할 API와 내부 구현 경계가 헤더에서 구분되지 않는다. 실제 접근 제한 여부는 확인 후 판단한다. |
| 다음 | 1~3 (뷰포트·인스펙터·애니메이터 그래프 패널) | 대형 패널이 여러 편집 상태를 가진다. 효과는 크지만 변경·검증 비용도 크다. |
| 보류 | 4 (RHI 백엔드) | 신규 참여자 이해에는 파일 분할보다 RHI 계약 문서가 효과적일 수 있다. |
| 별도 | 진입 문서 | 코드 변경 없이 효과가 큰 작업 후보다. 실행 흐름(`Client/Source/Main.cpp`의 `Application` → 엔진 초기화 → 씬 갱신)과 폴더 읽는 순서를 한 장으로 설명한다. |

## 4. 실행 단계와 완료 기준

### 단계 0 — 기준선과 의존 관계 작성

- 우선순위 1 후보의 호출 흐름, 상태 소유자, 공개 경계, 기존 검사 및 관련 문서를 짧게 기록한다.
- 관련 CMake 타깃과 현재 등록된 검사를 확인한다. 파일명에 `Check`가 있거나 과거 테스트가 통과했다는 사실만으로 현재 실행 검증을 가정하지 않는다.
- 동작을 확인할 검사가 없는 경우 리팩터링 전에 최소한의 회귀 검사 또는 수동 확인 절차를 정한다.

**완료 기준:** 변경 전·후 비교할 수 있는 동작과 검증 명령이 후보별로 정리된다. 빌드·검사는 사용자가 실행하고 실제 결과를 기록한다.

### 단계 1 — 뷰포트 편집 책임 파일럿

- `SceneViewportPanel`에서 입력 조정자와 모드별 편집 상태의 관계를 추적한다.
- 상태 수명과 입력/Undo 소유권이 명확히 분리되는 모드 하나만 선정한다. 후보는 스플라인 포인트 편집이며, 호출 순서와 선택 변경 경계가 분명하지 않으면 분리하지 않는다.
- 기존 메서드·상태를 작은 이동으로 정리하고 기능 추가, ShortcutManager/로캘 변경, 직렬화 변경은 함께 하지 않는다.

**완료 기준:** 일반 선택·기즈모·피킹·스플라인 드래그·타일맵 페인팅·지형 조각의 입력 간섭과 Undo 결과가 기존과 같고, 변경 영역을 검증하는 검사가 통과한다. UI 실행 확인이 남으면 완료로 표현하지 않는다.

### 단계 2 — 에디터 편집기 내부 경계

- 단계 1의 결과를 바탕으로 `InspectorPanel`과 `AnimatorGraphPanel`을 각각 독립 검토한다.
- 책임을 추출할 때는 데이터·상태의 단일 소유자, 호출 계약, 오류 전달과 테스트 경계를 함께 정의한다.
- 추출할 만한 경계가 없으면 함수 구역·이름·선언과 구현의 순서만 명료하게 정리하고 멈춘다.

**완료 기준:** 한 파일/클래스의 변경이 관련 없는 편집 상태를 수정하지 않으며, 기존 다중 선택·문서 저장/복구·미리보기 흐름이 유지된다.

### 단계 3 — 반복 규칙 및 백엔드 비교

- 실제로 중복된 UI 규칙과 DX12/SDLGPU 구현을 대조한다.
- 명확한 반복이 있는 한 경로부터 공통 계약을 정리한다. 성능 민감 코드나 공유 추상화는 기준 동작과 백엔드 간 결과 동등성을 먼저 검증한다.
- 차이가 의도된 기능 차이인지, 실수로 갈라진 구현인지 확인되지 않으면 통합하지 않는다.

**완료 기준:** 중복 제거로 새 간접 계층이 생기지 않고, 구현 전후 동작·백엔드별 지원 차이·성능 영향을 확인할 방법이 문서화된다.

## 5. 각 변경 묶음의 검증과 기록

- 해당 기능의 가장 작은 회귀 검사와 영향을 받는 통합 검사를 조합한다. 테스트가 없다면 정상 경로 외에 입력 경계·실패 취소·Undo/재실행 등 바뀌기 쉬운 시나리오를 정한다.
- 헤더/구현 재배치 때는 접근 수준, 초기화 순서, 멤버 수명, 리플렉션·저장 레이아웃과 ABI에 미치는 영향을 확인한다.
- CMake 등록, 생성 코드 입력, 호출부와 문서 링크를 확인한다. 파일 이동이나 API 변경이 필요하면 별도의 작업 단위로 제안한다.
- 작업 기록에는 기준 커밋/변경 범위, 실행한 명령과 검사 개수, 결과, UI/GPU 등 미검증 항목을 남긴다.
- 범위와 다음 후보는 [DevelopmentPriorities.md](DevelopmentPriorities.md)에서 관리한다. 이 문서는 구조 리팩터링의 원칙과 후보 순서만 소유한다.

## 6. 조사 한계

초기 조사는 2026-11-13 현재 작업 트리에서 첫-party 코드 파일 수, 일부 대형 파일의 선언·함수 구성을 읽고 후보를 좁힌 정적 검토다. 전체 코드에 대한 줄 단위 중복 조사, 빌드·CTest, UI/GPU 실행, 성능 측정은 하지 않았다. 따라서 이 문서는 우선순위가 확정된 결함 목록이 아니라 다음 조사·작업을 시작하기 위한 로드맵이다.
