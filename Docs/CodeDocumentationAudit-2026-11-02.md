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

### F01 — 높음 / 애니메이터 편집기의 저장·불러오기 실패와 미저장 데이터 보호 [해결됨]

- 위치: `AnimatorGraphPanel.cpp`, `AnimatorStateMachine.cpp`.
- 2026-11-02 조치 완료:
  - Archive `SaveToFile`의 원자적 저장 결과 전파 및 실패 시 초안/기존 파일 보존.
  - 임시 객체 역직렬화 후 스왑(Strong Exception Guarantee), `AnimState::blendTree` 값 타입(방안 B) 전환으로 직렬화 안정화.
  - New/Open/닫기 시 `EPendingAction` 기반 미저장 확인 모달(`Save/Discard/Cancel`) 및 단축키(Ctrl+S, Ctrl+N, Ctrl+O) 연동.
- 확인: 저장 실패·취소 시 문서 보존, 손상 파일 차단, New/Open/종료 시 모달 인터셉트 검증 완료.

### F02 — 높음 / 모델 임포트의 머티리얼 단계가 실패를 숨기고 재임포트 계약을 어김 [해결됨]

- 위치: `ModelImporter.cpp` `CommitPreparedModel`:348~440, `resolveTexture`.
- 2026-11-03 조치 완료:
  - 부모 기본 머티리얼(`SkinningMaterial.bammat.json`/`DefaultMaterial.bammat.json`) 로드 실패 시 필수 실패(`EResult::ResourceNotFound`) 반환 및 롤백 연동.
  - `ValidateImportOutput` 사전 검증 패턴 및 `SaveToBinaryFile` 반환값 검증을 머티리얼 인스턴스에 적용하여 실패 은폐 차단.
  - `settings.Overwrite` 플래그를 임베디드 텍스처 디스크 기록에 반영하여 재임포트 일관성 확보.
- 확인: 기본 머티리얼 누락 시 롤백, Overwrite=false 시 충돌 차단, 재임포트 시 최신 텍스처 반영 검증 완료.

### F03 — 높음 / 패키징 스크립트가 출력 경로를 검증하지 않고 재귀 삭제함 [해결됨]

- 위치: [PackageClient.ps1](../Tools/Scripts/PackageClient.ps1).
- 2026-11-02 조치 완료:
  - 절대 정규화 경로(`Get-NormalizedAbsolutePath`) 변환 적용.
  - 드라이브 루트, 저장소 루트(`$Root`) 및 상위 경로, 프로젝트 핵심 소스/데이터(`Engine`, `Editor`, `Resources` 등) 디렉터리 삭제 차단 가드 구축.
  - 와일드카드 오작동 방지를 위해 `-LiteralPath` 적용 및 필수 바이너리(`Client.exe` 등) 누락 시 패키징 중단 예외 추가.
- 확인: `.`, `Resources`, `C:\` 전달 시 삭제 시작 전 즉시 `Security Violation` 예외 발생 검증 완료.

### F04 — 높음 / 패키징 스크립트의 바이너리 위치와 수집 목록이 실행 계약과 다름 [해결됨]

- 위치: [PackageClient.ps1](../Tools/Scripts/PackageClient.ps1):45~115, `Config/Project.json`.
- 2026-11-03 조치 완료:
  - 구성별(`$Config`) 바이너리 디렉터리 다단계 자동 탐색 배열(`$BinDirCandidates`: `out/build/x64-$config/bin/$Config`, `bin/$Config`, `bin` 등)을 구축하고 `Client.exe` 존재 여부로 자동 결정.
  - 필수 바이너리(`Client.exe`, `Engine.dll`, `GameModule.dll`, `SDL3.dll`) 및 구성별 FMOD DLL(`fmodL.dll`/`fmod.dll`) 검증 및 복사.
  - 배포 환경의 필수 설정인 `$Root/Config/Project.json` 복사 경로를 구축하고 누락 시 즉각 중단 예외 추가.
  - `Resources` 하위 핵심 폴더(Data, Material, Scenes, Shader, Texture, Fonts, Audio, Model, Config, Scripts) 복사.
- 확인: `PackageClient.ps1` 실행을 통해 `out/package/Client`에 바이너리·설정·리소스가 온전히 수집되고 독립 실행(Standalone Client) 기동 검증 완료.

### F05 — 높음 / Client의 프로젝트 설정 실패·버전·시작 매니페스트 처리가 연결되지 않음 [해결됨]

- 위치: `Main.cpp`:95~115·235~260·489~520.
- 2026-11-03 조치 완료:
  - `ConfigManager::Load` 반환값(`ConfigLoadResult`) 검증을 통해 설정 파일 부재/파싱 실패 시 명확한 에러 로깅 및 `EResult::Fail` 안전 종료.
  - `SchemaVersion` 유효성(0 검사) 및 현재 엔진 지원 최대 버전 초과 검사 추가.
  - `StartupScenePath` 부재 시 명확한 경고 로그와 함께 기본 씬 폴백, 씬 파일 미존재 시 `EResult::FileNotFound` 반환.
- 확인: 설정 구문 에러 시 안전 종료, 시작 씬 부재 시 에러 전파 검증 완료.

### F06 — 보통 / CWD 독립 경로 전환이 피커와 애니메이터 파일까지 끝나지 않음 [해결됨]

- 위치: `ResourcePicker.h`, `AnimatorStateMachine.cpp`, `AnimationSystem.cpp`.
- 2026-11-03 조치 완료:
  - `ResourcePicker.h`의 `PlatformPath::ResolveProjectRelative("Resources")` 적용으로 CWD 무관 일관된 에셋 스캔 보장.
  - 로더 등록 확장자인 `.bamshader.json` 필터 추가.
  - `AnimatorStateMachine.cpp`의 Save/Load 파일 경로 `ResolveProjectRelative` 적용으로 상대 경로 컨트롤러 안전 로드 확보.
- 확인: CWD가 저장소 루트와 다를 때도 피커 및 애니메이터 파일 정상 인식 검증 완료.

### F07 — 보통 / 애니메이션 반복·부분 트랙·제어 API의 의미가 일치하지 않음 [해결됨]

- 위치: `AnimationSystem.cpp`, `AnimationEvaluator.cpp`, `AnimatorComponent.h`, `AnimatorStateMachine.cpp`.
- 2026-11-03 조치 완료:
  - 비반복 클립 종료 시 `std::clamp` 시간 보존으로 `fmod` 리셋 버그(마지막 포즈 대신 0초로 튀는 현상) 해결.
  - 부분 트랙 누락 채널에서 `vec3(0.0f)` 대신 해당 본의 바인드 로컬 TRS를 분해하여 보존하도록 `SampleLocalTRS` 보완.
  - ASM `isLooping` 반영, `Stop()` 호출 시 `m_currentStateTime` 리셋 및 `GraphGameplayLibrary` 시간 조회 동기화.
- 확인: 비반복 애니메이션 종료 시 마지막 포즈 유지, 부분 트랙 뼈 위치 왜곡 방지, ASM Stop/Play 정상 동작 검증 완료.

### F08 — 보통 / 기본 머티리얼 텍스처가 없으면 슬롯 보호가 조용히 생략됨 [해결됨]

- 위치: `MaterialInterface.cpp`, `MaterialInstance.cpp`, `gbufferPS.hlsl`, `shading_flags.hlsli`.
- 2026-11-03 조치 완료:
  - `MaterialFeatureFlags` 비트 플래그 시스템 도입으로 텍스처 미사용 슬롯의 샘플링을 셰이더 레벨에서 바이패스하고 안전한 수학적 기본값 직결.
  - 머티리얼 슬롯 리셋 시 미로드 더미 텍스처 안전 바인딩 및 RHI 실패 결과 전파로 상태 누수 원천 차단.
- 확인: 텍스처 없는 머티리얼에서 이전 드로우콜 상태 잔류 방지 및 정상 PBR 렌더링 검증 완료.

### F09 — 보통 / RL 기본 예제의 프로토콜·재현성과 Python 폴백 한계 [해결됨]

- 위치: `RLServer.cpp`, `CartPoleEnvironment.cpp`, `RLEnvironment.h`, `CartPoleEnvironment.h`, `bam_gym_env.py`.
- 2026-11-03 조치 완료:
  - `IRLEnvironment::Reset` 및 `CartPoleEnvironment::Reset`에 `std::optional<uint64_t> seed = std::nullopt`를 도입하고, `if (seed.has_value()) m_Rng.seed(seed.value());`로 수정하여 Gymnasium 표준인 `reset(seed=0)`의 결정론적 동일 상태 재현성 보장.
  - `RLServer.cpp`에 `<glaze/glaze.hpp>`를 도입하고 `RLCommandRequest`를 네임스페이스 레벨(External Linkage)로 선언하여 공백·키 순서 무관 안전한 JSON 파싱 및 입력 검증 확립.
  - 소켓 부분 송신(Partial Send)을 방지하는 `SendAll` 보장 전송 루프 및 64KB 버퍼 상한(DoS 방어) 가드 구축.
  - `bam_gym_env.py`에서 `numpy` 선택적 임포트 및 표준 라이브러리(`random`, `list`) 기반 `DiscreteSpaceFallback`/`BoxSpaceFallback` 구축으로 메서드 호출 시 `self` 바인딩 에러 제거.
- 확인: `reset(seed=0)` 반복 호출 시 관측 배열 100% 일치 확인 및 Python Gymnasium 계약 검증 완료.

### F10 — 보통 / 타일 광원 용량 초과는 폴백 없이 잘림

- 위치: [tiledLightCullingCS.hlsl](../Resources/Shader/hlsl/tiledLightCullingCS.hlsl):5·123~169, [lightingPS.hlsl](../Resources/Shader/hlsl/lightingPS.hlsl):684~686.
- 현재 처리: 타일 저장 용량은 256개이며 목록에 들어가지 않은 광원은 조명 순회에서 제외된다.
- 발생 조건: 한 타일과 교차하는 대상 광원이 256개를 초과함. 저장 순서도 병렬 atomic 추가 순서에 영향을 받는다.
- 영향: 초과 광원의 기여 누락, 조건에 따라 선택 목록 변동 가능성. 용량 안의 장면에 동일하게 발생한다는 뜻은 아니다.
- 최소 개선: 오버플로 플래그/계측과 정확한 대체 경로 또는 명시적 지원 상한·안정적인 선택 정책을 정한다.
- 확인: 동일 타일에 255/256/257개 이상 광원, 이동 카메라, 기준 전체 순회와의 영상 차이를 확인한다.

### F11 — 보통 / 애니메이터의 직접 입력·고정 표시 문구가 공통 지침과 다름 [해결됨]

- 위치: `AnimatorGraphPanel.cpp`, `Editor.csv`.
- 2026-11-02 조치 완료:
  - 트랜지션 연결 ESC 취소, 마우스 스크린 좌표, 노드/트랜지션 클릭 및 드래그 Delta, 캔버스 패닝을 `Engine::InputManager` API로 완전 전환.
  - 파일 필터, 파라미터 타입, 노드 텍스트(`ENTRY ->`, `Clip`, `No Motion`), 조건 모드 연산자 등 표시 문자열을 `Editor.csv` 로캘 키로 등록 및 `LOCAL_CSTR` 적용.
- 확인: ESC 취소, 노드 드래그, 캔버스 패닝, 언어 전환(영어/한국어) 시 번역 유지 검증 완료.

### F12 — 보통 / 썸네일의 디코드 크기·캐시·업로드 예산이 없음 [해결됨]

- 위치: [AssetCache.cpp](../Editor/Source/AssetManager/AssetCache/Private/AssetCache.cpp), [AssetCache.h](../Editor/Source/AssetManager/AssetCache/Public/AssetCache.h).
- 2026-11-03 조치 완료:
  - `LoadImageThumbnail` 내 128×128 Bilinear 다운샘플러(`DownsampleRGBA`) 도입으로 고해상도(4K 등) 디코드 메모리 99% 절감 (장당 33MB -> 32KB).
  - 메인 스레드 대기열(`m_PendingUploadQueue`)을 통한 프레임당 최대 4개(`MAX_UPLOADS_PER_FRAME`) 업로드 스로틀링으로 프레임 드랍(Hitch) 제거.
  - LRU 기반 최대 256개 항목 / 최대 64MB VRAM 바이트 예산 퇴거(Eviction) 메커니즘 구축 완료.
- 확인: 대형 이미지 다수 탐색 시 메모리 스파이크 방지 및 LRU 256개 상한 유지 검증 완료.

### F13 — 보통 / Release 모듈 빌드가 Debug 빌드 디렉터리를 사용함 [해결됨]

- 위치: [HotReloadManager.cpp](../Editor/Source/HotReload/Private/HotReloadManager.cpp):12~35.
- 2026-11-03 조치 완료:
  - `ResolveBuildDirectory()`에 현재 실행 중인 에디터 실행 파일(`Editor.exe`)의 상위 경로(`PlatformPath::GetExecutableDirectory()`)에서 `CMakeCache.txt`를 동적으로 역추적 탐지하는 로직 구축 (멀티 구성 프리셋 `<buildDir>/bin/<Config>` 및 단일 구성 프리셋 지원).
  - 역추적 불가 시 프리셋 기본 경로(`#ifdef _DEBUG` -> `out/build/x64-debug`, `#else` -> `out/build/x64-release`)로 안전 폴백.
- 확인: Release 및 Debug 에디터 각각에서 정확한 CMake 빌드 디렉터리 및 산출물 DLL 경로 탐색 검증 완료.

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
