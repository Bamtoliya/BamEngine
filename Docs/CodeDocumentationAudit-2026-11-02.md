# 코드·문서 정합성과 잔여 위험 점검 — 2026-11-02

- 기준: `main`, `0dea2d8b` 및 점검 시점의 작업 트리. 문서 수정 전 `git -c core.fsmonitor=false status --short` 결과는 비어 있었다.
- 방법: 소스·호출부·설정·CMake·관련 Docs·기존 로그의 정적 대조.
- 변경 권한: Markdown 작성·수정만 사용했다. C++·Python·PowerShell·설정·에셋은 변경하지 않았다.
- 실행 범위: 새 빌드, CTest, Editor/Client/GPU 실행, 패키징, 임포트 및 성능 측정은 수행하지 않았다.
- 적용 기준: [공통 지침](EngineEditorClientGuidelines.md), [소스 조직](SourceOrganizationGuidelines.md), [직렬화 지침](ReflectionArchiveGuidelines.md), [바인딩 계약](Shader/BINDING_LAYOUT.md).

## 1. 핵심 판단과 범위

기능 추가보다 **편집 데이터 보호와 실패 결과 전달을 먼저 보완하는 것이 필요하다.**
특히 애니메이터 편집기와 모델 임포트의 일부 경로는 파일 저장 실패를 성공처럼 처리한다.
패키징 스크립트에는 출력 디렉터리 삭제 범위와 필수 산출물 수집 문제가 있다.

반대로 계층형 애니메이터 연결, 컨트롤러 자동 로드 진입점, Client의 설정 기반 시작 씬,
`BAM_EDITOR` 정의·일부 디버그 API 격리, 표준 스트림 정리는 이미 코드에 반영되어 있다.
이 항목들을 다시 처음부터 구현하는 계획은 현재 상태와 맞지 않는다.

다음 영역의 주요 실행 경로를 읽었다.

- Client 초기화·경로·시스템 등록·입력 프레임·렌더 패스·시작 씬과 패키징 스크립트.
- Runtime 생성·정리·루프, ConfigManager, 리소스 로더·아카이브 파일 저장.
- 씬 문서 저장·확인 컨트롤러, 설정 저장 위치, 리소스 피커·썸네일 캐시.
- 모델 변환기의 머티리얼 추출, 모델 임포트의 커밋·저장·등록, 모델 계층 생성.
- 애니메이션 평가·상태 머신·컴포넌트·렌더 제출과 애니메이터 편집기.
- 머티리얼 기본 슬롯, DirectX12 루트 바인딩·계측, 타일 광원 목록, 모듈 빌드 설정.
- CartPole 환경·RL 소켓 서버·Python 어댑터.

전체 소스 파일의 모든 함수와 모든 플랫폼을 검토한 결과는 아니다.
특히 물리 해결기 전체, 전체 GPU 동기화·디스크립터 수명, 모든 그래프 노드,
독립 Reflection/Archive 모듈 전체 및 네트워크 장기 계획의 구현 검증은 포함하지 않는다.
기존 구현 기록의 완료 표시를 이 점검에서 재검증한 것으로 해석하지 않는다.

## 2. 구현 반영을 확인한 항목

아래의 **정적 확인**은 해당 연결이 소스에 있다는 의미다. 전체 완료 기준 통과와 구분한다.

### 2.1 Client 경로와 설정 기반 시작 씬 — 정적 확인 + 기존 초기화 로그

- [Main.cpp](../Client/Source/Main.cpp) `Initialize`, `ResolveProjectRoot`, `LoadProjectSettings`, `LoadStartupScene`, `UpdateViewportCamera`.
- 실행 파일 옆의 Resources/Config 또는 상위 프로젝트 마커로 루트를 찾고 Runtime에 주입한다.
- Quad/Cube/Sphere 기본 메시를 등록하고, 설정의 `StartupScenePath`가 있으면 사용한다.
- [Project.json](../Config/Project.json)은 `Resources/Scenes/RaytracingScene.json`을 지정한다.
- [PlatformPath.cpp](../Engine/Source/Core/Private/PlatformPath.cpp)의 프로젝트 상대 경로 해석이 있다.
- 기존 `Logs/Client.log`의 2026-11-02 09:35:39.048에 초기화 성공 기록이 있다.
- 한계: 설정 실패·매니페스트·배포·렌더링 동등성은 아래 F04·F05 및 6절의 미완료 범위다.

### 2.2 계층형 모델의 애니메이터 연결과 컨트롤러 자동 로드 — 정적 확인

- [SceneModel.cpp](../Engine/Source/World/Scene/Private/SceneModel.cpp)의 `InstantiateModel`은 루트에 Animator, 메시 노드에 SkinnedMeshRenderer를 생성한다.
- [AnimationSystem.cpp](../Engine/Source/System/Render/Private/AnimationSystem.cpp):12·52·66에서 Animator별 갱신, 하위 Skeleton 검색, `controllerPath` 변경 시 로드를 수행한다.
- [MeshRenderSystem.cpp](../Engine/Source/System/Render/Private/MeshRenderSystem.cpp):161에서 자신/부모의 Animator를 탐색한다.
- [AnimatorComponent.h](../Engine/Source/Core/Public/Components/Render/AnimatorComponent.h)에 직렬화 대상 `controllerPath`와 `LoadController`가 있다.
- 따라서 “동일 엔티티에서만 두 컴포넌트를 조회한다”는 기존 계획의 설명은 갱신 대상이다.
- 한계: 파일 경로·재생 경계·제어 노드·실패 후 재시도는 F06·F07에 남아 있다.

### 2.3 에디터용 빌드 정의와 표준 스트림 정리 — 정적 확인

- [루트 CMakeLists.txt](../CMakeLists.txt):23·39의 `BAMENGINE_WITH_EDITOR` 옵션과 Editor 추가 조건이 있다.
- [Engine/CMakeLists.txt](../Engine/CMakeLists.txt):58에서 옵션이 켜지면 Engine의 PUBLIC 정의로 `BAM_EDITOR=1`을 전달한다.
- [GraphSystem.h](../Engine/Source/System/Graph/Public/GraphSystem.h):52 및 구현의 디버그 훅과 [ResourceManager.h](../Engine/Source/Resource/Public/ResourceManager.h):65의 스냅샷 API에 조건부 컴파일이 있다.
- LocalizationManager와 ReflectionRegistry의 과거 `std::cerr` 위치는 현재 `BAM_LOG`를 사용한다.
- 한계: 같은 Engine을 링크하는 Client도 에디터 포함 구성에서는 정의를 전달받는다. `BAM_SHIPPING` 정의·최종 제거 범위와 Editor OFF 빌드 성공은 확인하지 않았다.

### 2.4 모델 임포트의 머티리얼·임베디드 텍스처 경로 — 정적 확인

- [ModelConverter.cpp](../Tools/ModelConverter/Private/ModelConverter.cpp):352 이후에서 머티리얼 인자·텍스처와 압축 임베디드 데이터를 추출한다.
- [ModelImporter.cpp](../Editor/Source/AssetManager/Importer/Private/ModelImporter.cpp):348 이후에 텍스처 추출·MaterialInstance 생성·모델 슬롯 연결이 있다.
- Model과 SceneModel의 머티리얼 참조는 `MaterialInterface`를 사용한다.
- 기존 [ImplementationLog](ImplementationLog.md)의 Fox 임포트·시각 확인은 한 샘플에 대한 과거 기록으로 유지한다.
- 한계: 저장 실패·재임포트·외부 텍스처 이동·모든 FBX/glTF 변형의 지원 완료 근거는 아니다(F02).

### 2.5 씬 문서의 저장 실패 보호 — 정적 확인

- [SceneDocument.cpp](../Editor/Source/Document/Private/SceneDocument.cpp):58의 `SaveTo`는 저장 가능 여부·이력 정리·직렬화·파일 저장을 검사하고 성공 뒤 `MarkSaved`를 수행한다.
- [EditorDocumentController.cpp](../Editor/Source/Document/Private/EditorDocumentController.cpp)의 `RequestAction`, `ResolveConfirmation`, `NotifySaveResult`가 보호 흐름을 제공한다.
- [ArchiveInterface.cpp](../Projects/Archive/src/ArchiveInterface.cpp)의 `SaveEncodedToFile`은 임시 파일 쓰기·닫기 결과를 검사한 뒤 대상 경로로 rename한다.
- 이 경로는 애니메이터 전용 저장의 최소 개선에 재사용할 기존 구현이다. 새 저장 프레임워크부터 만들 필요는 없다.
- 한계: 씬·프리팹·모든 리소스 편집기의 공통 보호 완료나 현재 전체 테스트 통과를 의미하지 않는다.

### 2.6 렌더링 기반과 머티리얼 슬롯 보완 — 정적 확인

- DirectX12 루트 파라미터 `[0]~[10]`, Material `space1`, Object `space2`, Pass `space3`는 현재 바인딩 문서와 대응한다. 셰이더 전체 ABI 검증은 아니다.
- DirectX12의 지연 GPU 타임스탬프 회수·`QueryMemoryStats`, 공통 디퍼드 파이프라인의 `DispatchLightCulling` 호출을 확인했다.
- Renderer의 비인덱스 인스턴싱 `DrawInstanced` 분기와 MeshRenderSystem의 패스별 LOD 평가·Geometry 패스 상태 갱신 분기가 있다.
- MaterialInterface와 MaterialInstance에 기본 텍스처 슬롯 0~3을 채우는 코드, 인스턴스 슬롯 캐시 복구가 있다. 기본 텍스처 확보 실패까지 보호하는 코드는 아니다(F08).
- 과거 GPU 시간·드로우콜 관찰은 새 성능 측정으로 승격하지 않는다.

### 2.7 피커 최적화·모듈 빌드·RL 첫 예제 — 정적 확인

- ResourcePicker는 타입별 확장자 필터와 팝업 등장 시 디스크 목록 캐시를 사용한다. 최초 스캔 비용·누락 확장자·CWD 문제는 별도다.
- GameModule의 PCH, ModuleBuildManager의 200ms 디바운스와 `/p:BuildProjectReferences=false`가 있다. 현재 명령은 여전히 `cmake.exe --build`다.
- `Client --rl`은 SDL 창·Runtime 생성 전에 CartPole/RLServer 경로로 진입하며 서버는 loopback에 바인딩한다.
- CartPole의 관측·난수·물리 상태는 환경 인스턴스 멤버다. 같은 프로세스의 벡터 환경이나 일반 멀티플레이 구현까지 확인한 것은 아니다.

## 3. 우선 수정할 결함과 위험

각 항목의 재현은 사용자 실행용 확인 조건이다. 이번 점검에서 실제 실패를 주입하지 않았다.

### F01 — 높음 / 애니메이터 편집기의 저장·불러오기 실패와 미저장 데이터 보호

- 위치: `AnimatorGraphPanel.cpp`:85·145·196·214, `AnimatorStateMachine.cpp`:238.
- 현재 처리: `SaveDocument`와 `OpenDocument`는 bool 반환값을 무시하고 `m_IsDirty=false`로 바꾼다. New는 현재 데이터를 즉시 교체한다. 패널에는 씬 문서 컨트롤러와 같은 보호 연결이 없다.
- 발생 조건: 쓰기 불가 위치에 저장, 손상된 컨트롤러 열기, 편집 후 New/Open 또는 앱 종료.
- 영향: 저장하지 못한 편집이 저장된 것처럼 보이거나 새 문서로 덮인다. `SaveToFile`은 `ofstream(..., trunc)` 후 쓰기·닫기 결과 검사 없이 true를 반환하므로 기존 파일 보존도 보장하지 않는다.
- 최소 개선: 기존 Archive의 파일 저장을 사용하고 성공 결과를 전파한다. 로드는 임시 상태 머신에 완료한 뒤 교체한다. 성공 시에만 경로·저장 기준점을 갱신하며 New/Open/종료에 저장·버리기·취소를 연결한다.
- 확인: 쓰기 금지·디스크 쓰기 실패·손상 JSON에서 기존 파일/문서/변경 표시를 유지하는지, New/Open 취소가 실제 교체를 막는지 확인한다.

### F02 — 높음 / 모델 임포트의 머티리얼 단계가 실패를 숨기고 재임포트 계약을 어김

- 위치: `ModelImporter.cpp` `CommitPreparedModel`:348~439, `resolveTexture`:371~400.
- 현재 처리: 기본 머티리얼 로드 실패는 로그만 남기고 진행한다. MaterialInstance 생성 실패는 continue하며, 430행의 머티리얼 저장 반환값은 무시한다.
- 영향: 메시·모델 임포트 성공 뒤에도 머티리얼 파일/슬롯이 빠질 수 있다. 같은 함수의 Skeleton/Animation 저장은 실패를 반환하므로 단계별 실패 의미도 다르다.
- 재임포트: 임베디드 텍스처는 파일이 있으면 쓰지 않는다. `Overwrite=true`여도 원본 텍스처 변경이 반영되지 않는다. 머티리얼 출력에는 메시/골격과 같은 `ValidateImportOutput` 검사가 없다.
- 추가 위험: 등록 롤백은 리소스 핸들을 정리하며 이미 쓴 모든 파일을 되돌리는 트랜잭션은 아니다. 외부 텍스처는 원본 위치에서 로드하므로 프로젝트 밖 파일을 삭제·이동하거나 패키징하면 참조가 깨질 수 있다.
- 최소 개선: 기존 `ValidateImportOutput`·저장 결과 검사 패턴을 머티리얼/텍스처에도 적용한다. 필수 실패와 허용 폴백을 구분하고 Overwrite 정책을 통일한다. 외부 텍스처 수집과 파일 롤백 범위를 문서화한다.
- 확인: 머티리얼 출력만 쓰기 실패, 기본 머티리얼 누락, 동일 모델 재임포트, 프로젝트 밖 텍스처 이동 후 재실행을 검사한다.

### F03 — 높음 / 패키징 스크립트가 출력 경로를 검증하지 않고 재귀 삭제함

- 위치: [PackageClient.ps1](../Tools/Scripts/PackageClient.ps1):20.
- 현재 처리: 전달받은 `OutputDir`가 존재하면 `Remove-Item -Recurse -Force`를 실행한다.
- 발생 조건: `-OutputDir .`, 저장소 루트, 소스/리소스 경로 또는 기존 데이터 폴더를 잘못 전달함.
- 영향: 기존 프로젝트나 임의 디렉터리를 삭제할 수 있다. 기본 출력값만 사용하면 반드시 발생한다는 뜻은 아니다.
- 최소 개선: 절대 경로를 정규화하고 허용된 패키지 출력 루트의 하위인지 검사한다. 저장소 루트·입력 폴더·드라이브 루트를 거부하고, 전용 스테이징 폴더 성공 후 교체한다. `-LiteralPath`를 사용한다.
- 확인: 위험 경로를 전달해도 **삭제 전에** 실패하는지 코드 수정 후 검증한다. 현재 스크립트로 파괴적인 재현을 수행하지 않는다.

### F04 — 높음 / 패키징 스크립트의 바이너리 위치와 수집 목록이 실행 계약과 다름

- 위치: `PackageClient.ps1`:9·39·44, 루트 CMake의 출력 경로, `Config/Project.json` 및 시작 씬.
- 현재 처리: `out/build/x64-debug/bin` 아래에서 곧바로 Client.exe를 찾는다. 현재 Visual Studio 구성의 산출물은 `bin/Debug`, Release는 `bin/Release`에 있다.
- 필수 파일 누락도 warning으로 처리하고 마지막에 성공 문구를 출력한다.
- 복사 대상에는 프로젝트 `Config/Project.json`, `Resources/Config`, `Resources/Model`, `Resources/Scripts`가 없다. 현재 시작 씬에는 `Resources/Model/Fox_fox_material.bammatinst` 참조가 있다.
- 영향: 빈/불완전 배포 폴더도 성공처럼 보이며 지정 씬·모델·스크립트와 시작 매니페스트가 누락된다.
- 최소 개선: 실제 구성별 산출물 위치를 사용하고 필수 바이너리/설정/참조 누락 시 실패한다. 런타임 DLL 전체 의존성은 별도 수집·검증한다. Cook과 Stage를 구분한다.
- 확인: 저장소 밖 배포 폴더에서 다른 CWD로 실행하고 개발 경로를 사용할 수 없는 조건에서 설정·모델·그래프 참조 복원을 확인한다.

### F05 — 높음 / Client의 프로젝트 설정 실패·버전·시작 매니페스트 처리가 연결되지 않음

- 위치: `Main.cpp`:95~106·235~259·481~489, `ConfigManager.h`의 `ConfigLoadResult`.
- 현재 처리: `LoadProjectSettings`가 반환 결과를 버리고, 설정이 없거나 잘못되어도 기본값을 유지한 채 진행한다. `SchemaVersion`을 검사하지 않는다. 빈 시작 씬은 NewScene2로 폴백한다.
- `StartupManifestPath`는 Client에서 소비하지 않고 `AssetDirectory`도 Runtime 생성 시 고정 `Resources`를 대체하지 않는다. 필요한 리소스 일부는 씬 참조를 통해 지연 로드될 수 있으므로 모든 씬이 반드시 실패한다고 단정하지 않는다.
- 영향: 잘못된 설정이 다른 씬 실행으로 보일 수 있고, 씬 참조에 없는 공통 기본 리소스·시작 순서가 Editor와 달라진다.
- 최소 개선: 기존 ConfigLoadResult를 그대로 사용해 누락/구문 오류/미지원 버전을 구분한다. 빈 시작 씬의 정책을 확정하고, UI 없는 시작 리소스 처리를 재사용 가능한 경계로 연결한다. 새 로더를 중복 작성하지 않는다.
- 확인: 설정 없음·손상 JSON·미지원 버전·빈 씬·없는 씬·기본 텍스처 누락에서 로그와 종료 코드를 각각 확인한다.

### F06 — 보통 / CWD 독립 경로 전환이 피커와 애니메이터 파일까지 끝나지 않음

- 위치: `ResourcePicker.h`:169, `AnimatorStateMachine.cpp`:238·257, `AnimationSystem.cpp`:66~77.
- 현재 처리: 피커는 `path("Resources")`로 스캔한다. 컨트롤러는 문자열 경로를 바로 ifstream/ofstream에 전달한다.
- 발생 조건: 실행 CWD가 저장소 루트와 다르고 미로드 에셋을 피커에서 찾거나 상대 `controllerPath`를 사용함.
- 영향: 런타임의 다른 리소스는 로드되어도 피커 후보가 없거나 컨트롤러만 로드되지 않는다. 실패 경로는 `loadedControllerPath`에 기록되어 같은 경로를 복구해도 자동 재시도하지 않는다.
- 추가 누락: Shader 확장자 필터에는 로더가 지원하는 `.bamshader.json`이 없다(`ResourcePicker.h`:79).
- 최소 개선: 기존 PlatformPath/프로젝트 에셋 루트를 재사용하고, 실패 여부와 재시도/명시적 새로고침 정책을 둔다. 확장자 목록을 실제 등록 로더와 맞춘다.
- 확인: 다른 CWD의 미로드 피커, 상대/절대 컨트롤러 경로, 실패 후 같은 파일 복구를 확인한다.

### F07 — 보통 / 애니메이션 반복·부분 트랙·제어 API의 의미가 일치하지 않음

- 위치: `AnimationSystem.cpp`:96~105, `AnimationEvaluator.cpp`:56~79, `AnimatorComponent.h`의 Play/Pause/Stop, `GraphGameplayLibrary.h`:276~321, `AnimatorStateMachine.cpp`:132 이후.
- 비반복 종료: System은 시간을 클립 끝으로 clamp하지만 평가기는 항상 fmod한다. 끝 시간은 다시 0이 되어 마지막 포즈 대신 첫 포즈에서 멈출 수 있다. TicksPerSecond=0은 평가기에서는 25로 대체하나 System의 비반복 종료 조건에서는 제외된다.
- 부분 트랙: 트랙이 존재하면서 위치/회전/크기 중 일부 키가 없으면 바인드 로컬 TRS 대신 0/identity/1을 사용한다. 비기본 로컬 변환을 가진 뼈의 회전 전용 트랙 등에서 포즈가 달라질 수 있다.
- ASM 제어: Stop/SetTime/GetCurrentTime은 `AnimatorComponent::currentTime`을 다루지만 ASM은 별도 `m_currentStateTime`을 진행한다. ASM 사용 시 Stop→Play가 재시작을 보장하지 않고 조회 시간은 실제 상태 시간과 다를 수 있다. `AnimState::isLooping`도 현재 Update에서 사용되지 않는다.
- 최소 개선: 반복/clamp·TPS 기본값 정책을 공통 평가 경계에서 정한다. 누락 채널은 올바르게 분해한 바인드 TRS로 채우고, 기존 런타임 상태에 시간 조회·이동·초기화 동작을 연결한다.
- 확인: 첫/마지막 포즈가 다른 비반복 클립, TPS=0, 회전 키만 있는 뼈, ASM Stop/Play·SetTime·isLooping=false를 검사한다.

### F08 — 보통 / 기본 머티리얼 텍스처가 없으면 슬롯 보호가 조용히 생략됨

- 위치: `MaterialInterface.cpp`:575~634, `MaterialInstance.cpp`:48~140.
- 현재 처리: 비어 있는 슬롯은 `GetResourceHandle`로 이미 로드된 기본 텍스처만 찾는다. 없는 텍스처/RHI 자원은 건너뛰며 기본 슬롯 바인딩의 실패 결과도 반환하지 않는다.
- 발생 조건: white/blue/black 기본 텍스처 미로드 또는 자원·바인딩 실패. Client의 매니페스트 미연결은 별도 확인할 경로다.
- 영향: Bind가 성공이어도 필수 슬롯이 채워졌다는 보장이 없어 이전 상태 잔류 가능성이 남는다.
- 최소 개선: 초기화에서 기본 텍스처 확보를 검증하고 바인딩 실패를 전달한다. 기존 P1-E 플래그 계획과 이 즉시 보호 문제를 구분한다.
- 확인: 기본 텍스처를 로드하지 않은 구성과 강제 바인딩 실패에서 명시적 실패 또는 안전 폴백을 확인한다.

### F09 — 보통 / RL 기본 예제의 프로토콜·재현성과 Python 폴백 한계

- 위치: `RLServer.cpp`:114·128·152 이후, `CartPoleEnvironment.cpp`:24, `bam_gym_env.py`:8·69.
- 프로토콜: JSON 파서 대신 일부 문자열 패턴과 숫자 검색으로 명령을 해석한다. 정상 JSON의 다른 공백 배치도 인식하지 못하며 잘못된 입력을 환경 호출 전에 구조적으로 검증하지 않는다.
- 송신: send 반환값을 무시한다. 응답 일부만 전송되면 줄바꿈을 기다리는 Python이 멈출 수 있다. 개행이 없는 입력의 버퍼 길이 상한도 없다.
- seed: 0을 “재시드 안 함”으로 사용하므로 `reset(seed=0)` 반복 재현이 성립하지 않는다.
- 폴백: numpy를 무조건 import하므로 표준 라이브러리만으로 실행되지 않는다. Gymnasium 미설치 분기의 `sample: lambda: ...`는 인스턴스 메서드 호출에 self가 전달되어 실패한다.
- 최소 개선: 기존 JSON 라이브러리로 요청 스키마/행동을 검사하고 송신 완료·프레임 길이를 관리한다. seed 미지정과 0을 구분하며 폴백의 실제 지원 범위를 정한다.
- 확인: 다양한 JSON 공백·잘못된 행동·부분 송신·과대 요청, seed=0 반복, Gymnasium 없는 numpy 환경을 각각 검사한다.
- 범위: loopback 단일 환경의 기본 연결은 코드에 있다. 일반 네트워크 세션·병렬 환경·전체 Gymnasium 계약 검증 완료와 구분한다.

### F10 — 보통 / 타일 광원 용량 초과는 폴백 없이 잘림

- 위치: [tiledLightCullingCS.hlsl](../Resources/Shader/hlsl/tiledLightCullingCS.hlsl):5·123~169, [lightingPS.hlsl](../Resources/Shader/hlsl/lightingPS.hlsl):684~686.
- 현재 처리: 타일 저장 용량은 256개이며 목록에 들어가지 않은 광원은 조명 순회에서 제외된다.
- 발생 조건: 한 타일과 교차하는 대상 광원이 256개를 초과함. 저장 순서도 병렬 atomic 추가 순서에 영향을 받는다.
- 영향: 초과 광원의 기여 누락, 조건에 따라 선택 목록 변동 가능성. 용량 안의 장면에 동일하게 발생한다는 뜻은 아니다.
- 최소 개선: 오버플로 플래그/계측과 정확한 대체 경로 또는 명시적 지원 상한·안정적인 선택 정책을 정한다.
- 확인: 동일 타일에 255/256/257개 이상 광원, 이동 카메라, 기준 전체 순회와의 영상 차이를 확인한다.

### F11 — 보통 / 애니메이터의 직접 입력·고정 표시 문구가 공통 지침과 다름

- 위치: `AnimatorGraphPanel.cpp`:191~192·242~260·312·373~402·442·471·510.
- 근거: 프로젝트 소유 캔버스에서 ImGui 키/마우스 상태·MouseDelta를 직접 읽는다. `Cancel (ESC)`와 파일 필터 설명 등에 직접 표시 문자열이 있다. 타입 식별자 자체를 유지하는 선택과 구분한다.
- 영향: InputManager의 프레임·화면 좌표·분리 창 기준과 달라질 위험, 언어 전환 시 미번역 UI가 남는다. 외부 위젯 내부 입력 예외에 해당하는 코드가 아니다.
- 최소 개선: 버튼 반환값/호버 판정은 유지하고 사용자 정의 입력을 기존 InputManager로 바꾼다. 기존 키를 찾아 재사용하며 필요한 키는 영어·한국어 CSV에 함께 등록한다.
- 확인: 언어 전환·텍스트 편집 중 Escape/단축키·분리 창/DPI·캔버스 드래그를 검사한다.

### F12 — 보통 / 썸네일의 디코드 크기·캐시·업로드 예산이 없음

- 위치: [AssetCache.cpp](../Editor/Source/AssetManager/AssetCache/Private/AssetCache.cpp):41·306 및 헤더의 캐시/대기열.
- 현재 처리: 원본 이미지 전체를 RGBA로 디코드해 복사하고, 완료 목록을 Update에서 처리하며, 캐시는 명시적 무효화/전체 정리 외의 예산 기반 퇴거가 없다.
- 발생 조건: 고해상도 이미지 다수 탐색, 많은 작업의 동시 완료.
- 영향: CPU 버퍼·GPU 텍스처 누적과 프레임 업로드 집중 위험. 이번에 메모리 누수나 FPS 저하를 측정한 것은 아니다.
- 최소 개선: 기존 캐시에 축소 크기·바이트 예산·LRU 또는 동등한 퇴거·프레임 업로드량 상한을 추가한다.
- 확인: 다수 대형 이미지를 순회하며 최대 CPU/GPU 메모리·대기열·프레임 업로드 비용을 기록한다.

### F13 — 보통 / Release 모듈 빌드가 Debug 빌드 디렉터리를 사용함

- 위치: [HotReloadManager.cpp](../Editor/Source/HotReload/Private/HotReloadManager.cpp):12~14, [ModuleBuildManager.cpp](../Editor/Source/HotReload/Private/ModuleBuildManager.cpp):160 이후.
- 현재 처리: 빌드 디렉터리는 `out/build/x64-debug`로 고정하고 Release 에디터에서는 `--config Release`를 전달한다.
- 근거: 현재 x64-debug CMake 캐시는 `CMAKE_CONFIGURATION_TYPES=Debug`다. 제공된 프리셋도 Debug/Release 디렉터리를 나눈다.
- 영향: Release 에디터의 모듈 컴파일 실패 또는 잘못된 구성/산출물 경로 사용 위험.
- 최소 개선: 사용 중인 빌드 구성과 실제 디렉터리를 함께 선택하고 로더가 읽는 DLL 경로까지 맞춘다.
- 확인: Debug·Release 각각에서 모듈 변경→컴파일→실제 해당 구성 DLL 교체를 확인한다.

## 4. 추가 조사할 기존 실행 오류

`Logs/Editor.log`의 2026-11-02 09:36:33~09:36:49에
`BlackGBufferMaterial.bammat`, `DefaultMaterial.bammat`, `SkinningMaterial.bammat`,
`SpriteMaterial.bammatinst` 등 여러 바이너리의 `stage=archive file read` 오류가 있다.

- 해당 파일과 `.bammat.json`의 병존을 확인했다. 파일 부재만으로 설명할 수 없다.
- 현재 ResourceManager는 `.json`을 JsonArchive, `.bam*`를 BeveArchive로 분기한다.
- 이 로그만으로 과거 바이너리 형식·파일 손상·버전 불일치 중 원인을 확정하지 않는다.
- 실패 파일의 생성 경로·헤더/포맷과 현재 Decode 계약을 별도로 대조하고, 실패한 에셋이 UI/씬에서 정상처럼 보이는지 확인해야 한다.
- 로그의 정상 종료·비동기 작업 성공 문구는 앞선 에셋 로드 실패를 해소했다는 증거가 아니다.

## 5. 문서 불일치와 이번 반영

- **BuildAndPackagingPlan**: 머리말은 1~2단계 완료, 본문은 보류·고정 시작 씬으로 충돌했다. 경로/시작 씬은 반영, 매니페스트·실패 정책·배포는 미완료로 나눴다.
- **DevelopmentPriorities**: P0-B의 매크로/로거 정리, P2-D의 계층/컨트롤러 연결, P2-E의 설정 기반 시작 씬을 현재 코드에 맞췄다. 데이터 보호 결함을 기능 확장보다 먼저 배치했다.
- **EngineDevelopmentPlan / AnimatorGraphPlan**: “동일 엔티티만 조회”라는 설명을 제거하고 현재 계층 연결과 남은 정확성·저장 보호를 구분했다. Phase 표시는 기능별 전체 완료 기준과 구분했다.
- **BuildConfigurationAndPreprocessing**: 로거 전환·Graph/Resource 디버그 API 가드를 구현 반영으로 표시했다. 공유 Engine 구성의 매크로 전파와 Shipping 미완료는 남겼다.
- **ImGuiDevelopmentPlan**: 실제 설정 경로와 씬 문서 보호 상태를 맞추고, 애니메이터/피커/캐시의 잔여 항목을 연결했다.
- **ModuleHotReloadOptimization**: 초기 병목 설명을 과거 분석으로 구분하고 PCH·디바운스·현재 명령과 Release 경로 문제를 추가했다.
- **성능 기록**: 기존 1.10→0.25ms·56→57회 관찰은 보존하되, 비교 조건·반복 통계가 첨부된 최신 통제 실험으로 간주하지 않는다.
- **ImplementationLog**: 동일한 2026-10-30 그래프 전환 제목이 5회 반복된다. 이력 중복 정리는 별도 작업으로 남기고 이번에는 기존 기록을 삭제하지 않았다.
- **과거 API 설명 정정**: 경로 변경 기록의 `PlatformPath::ToProjectRelative()`는 현재 공개 헤더에 없다. ResourceManager 내부 변환과 구분하도록 후속 점검 기록에 명시했다.

## 6. 아직 완료로 처리하지 않는 범위

- Client의 공통 시작 매니페스트, 설정 실패 정책, 에셋 루트 설정 반영, Cook/Stage/Package 및 저장소 밖 실행.
- Client의 현재 단일 백버퍼 Forward 경로와 Editor의 GBuffer/Lighting/HDR/후처리 동등성. 지오메트리 표시와 같은 의미가 아니다.
- 애니메이터 저장·교체 보호, 재생 경계·부분 트랙·ASM 제어, 독립 미리보기·2D 블렌드·AnimNotify·레이어/본 마스크.
- MaterialFeatureFlags 및 그래프 상수/채널 입력 연결, 기본 텍스처 부재 시 안전 정책.
- 리소스 피커 최초 동기 스캔 비용, 썸네일 예산, 참조 색인·재임포트·외부 파일 수집.
- 에디터 없는 Engine/Client 구성, Shipping 제거 범위, Release 핫리로드.
- RL 프로토콜 견고성·seed=0·폴백, 씬 물리 연동·벡터 환경·정책 추론.
- 일반 게임 네트워크 세션·리플리케이션·RPC. RL 전용 loopback 서버의 존재로 완료 처리하지 않는다.

## 7. 검증 근거와 권장 확인 순서

### 기존 로그의 범위

- Debug `LastTest.log`: 2026-11-02 08:38 시작/종료만 있고 테스트별 결과가 없다. 현재 전체 통과 근거가 아니다.
- Debug `LastTestsFailed.log`: `7:SceneArchiveCheck`가 남아 있다. 이 파일만으로 현재도 실패한다고 확정하지 않으며, 해소 증거가 없는 재확인 대상으로 둔다.
- Release `LastTest.log`: 10월 11일 시작/종료만 있다. 최신 Release 검증 근거가 아니다.
- Client/Editor 실행 로그의 마지막 시각은 HEAD 커밋 시각 09:43:49보다 이전이다. 해당 로그를 HEAD 전체의 재검증으로 간주하지 않는다.
- ImplementationLog의 과거 자동 검사·UI·성능 관찰은 날짜/범위가 있는 별도 근거로 유지한다.

### 추천 순서

구체적인 실행 순서·선행 조건·단계별 완료 기준은 [DevelopmentPriorities.md 10절](DevelopmentPriorities.md#10-조사-시점의-문서코드-차이와-재조정된-단기-실행-순서-2026-11-02-갱신)을 따른다.

- 기준 확인 → 저장/임포트 보호(F01/F02)·삭제 범위 보호(F03) → 현재 경로/재생/기본 슬롯(F06~F08)과 Client 설정 실패 보고(F05 일부) → 머티리얼 기능 확장 순으로 진행한다.
- 썸네일 예산·참조 검증은 통합 설정 창과 편의 메뉴보다 먼저 다룬다. 현재 편집을 막는 비용이 확인되면 우선순위를 올린다.
- 전체 배포(F04/F05 잔여), RL(F09), 광원 용량 초과(F10), Release 핫리로드(F13)는 해당 경로 사용 조건에 맞춰 선행 수정한다. F11은 애니메이터 수정과 함께 적용한다.
- 보류된 전체 배포를 신규 렌더링·애니메이션의 일괄 선행 조건으로 만들지 않는다. 세부 순서를 이 문서에 중복 관리하지 않는다.

### 사용자가 실행할 빌드·검사

아래 명령은 이번에 실행하지 않았다. 저장소 루트의 Visual Studio 개발 환경에서 실행한다.
configure/build/CTest는 산출물·로그를 생성한다.

```powershell
cmake --preset x64-debug -DBAMENGINE_BUILD_REFLECTION_CHECK=ON -DBAMENGINE_BUILD_GRAPH_CHECK=ON
cmake --build out/build/x64-debug --config Debug
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure --no-tests=error
```

첫 결과에서 `SceneArchiveCheck`, `EditorHistoryCheck`, `EditorSettingsCheck`,
`InputManagerCheck`, 그래프 검사와 로캘 검사의 실제 등록·실행 수를 확인한다.
선택한 검사만 통과한 경우 그 범위만 기록한다. 애니메이터/임포터의 위 실패 조건은 별도 확인이 필요하다.

```powershell
cmake --preset x64-release -DBAMENGINE_BUILD_REFLECTION_CHECK=ON -DBAMENGINE_BUILD_GRAPH_CHECK=ON
cmake --build out/build/x64-release --config Release
ctest --test-dir out/build/x64-release -C Release --output-on-failure --no-tests=error
```

Editor OFF 구성은 같은 빌드 디렉터리를 재사용해 현재 구성과 섞지 말고 별도 디렉터리에서 검증한다.
패키징 스크립트는 F03/F04 수정 후 실행한다.
실행 결과를 기록할 때 커밋·구성·GPU·해상도·재현 입력·성공/실패와 미검증 범위를 함께 남긴다.
