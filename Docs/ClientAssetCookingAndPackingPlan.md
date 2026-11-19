# Client 리소스 쿠킹·바이너리화·팩 배포 계획

- 작성일 2026-11-19
- 상태 · 설계안. 현재 폴더 내보내기와 개별 바이너리 리소스 로드를 확인했다. 통합 쿠커, 팩 작성기와 Client 팩 읽기는 구현 예정이다.
- 첫 대상 · Windows x64 Release, DirectX 12, Editor의 기존 빌드 및 패키징 화면.

## 1. 현재 구현과 목표

`ClientPackage.cpp`는 Windows 실행 파일과 DLL, 설정, 시작 매니페스트 및 리소스를 배포 폴더에 준비한다. `CopyRuntimeResources()`는 `Assets/`와 `Engine/Resources/`에서 허용 확장자를 가진 개별 파일을 복사한다. 시작 씬에서 확인할 수 있는 참조를 수집하고 `[Package inventory]`에 범위를 기록한다. 일부 모델·메시·머티리얼·스켈레톤·애니메이션은 임포트 시 이미 BEVE 바이너리로 저장된다. 현재 확인한 Client 로드 경로는 실제 디렉터리와 파일을 읽으며, 팩을 마운트하는 경로는 검색에서 확인되지 않았다. [기존 배포와 선택 수집 계획](BuildAndPackagingPlan.md) · [ClientPackage](../Editor/Source/Build/Private/ClientPackage.cpp) · [모델 임포터](../Editor/Source/AssetManager/Importer/Private/ModelImporter.cpp) · [리소스 로더](../Engine/Source/Resource/Private/ResourceManager.cpp)

목표는 Editor에서 저장한 씬을 시작점으로 필요한 에셋과 간접 의존성을 확정하고, 플랫폼에 맞는 런타임 데이터로 준비한 뒤 소수의 데이터 팩으로 제공하는 것이다. 배포된 Client는 독립 환경에서 팩의 항목을 필요한 시점에 읽는다. 원본 프로젝트의 `Assets/`, 셰이더 컴파일러, 임포터와 Editor 없이 실행한다.

```mermaid
flowchart LR
    Editor[Editor 빌드 프로필] --> Collect[시작 씬과 의존성 수집]
    Collect --> Cook[플랫폼별 변환과 기존 바이너리 재사용]
    Cook --> Pack[리소스 팩과 매니페스트 작성]
    Pack --> Stage[스테이징 검증과 배포 폴더 게시]
    Stage --> Client[Client가 팩 마운트]
    Client --> Load[URI로 필요한 항목 읽기]
```

- **수집**은 배포 대상과 포함 이유를 결정한다. **쿠킹**은 개별 에셋을 대상 플랫폼이 읽을 형식으로 준비한다. **패킹**은 준비된 데이터 여러 개를 색인이 있는 파일에 묶는다. **패키징**은 실행 파일·DLL·설정·팩을 검증하고 배포 폴더를 게시한다. 첫 버전에서도 단계별 실패 이유와 결과를 각각 기록한다.
- Editor는 시작 씬·대상 플랫폼·옵션과 진행 표시를 관리한다. UI 없는 쿠킹 코드는 기존 변환 도구를 활용한다. 기존 `BamAssetWorker`는 지원하는 모델 변환을 맡고, 전체 쿠킹을 별도 프로세스로 확장할지는 작업량과 오류 격리 필요성을 확인한 후 결정한다. Engine은 읽기 전용 팩 및 리소스 로드를, Client는 마운트와 실행 초기화 순서를 소유한다. [소스 조직 지침](SourceOrganizationGuidelines.md) · [에셋 워커 계획](OutOfProcessAssetPipelinePlan.md)

## 2. 의존성 수집과 쿠킹 규칙

### 2.1 입력과 참조 목록 확정

- 시작 씬, 빌드 프로필의 추가 씬, 시작 리소스 매니페스트, 렌더 프로필, 필수 엔진 리소스, 런타임 모듈과 명시적 추가 에셋을 수집 시작점으로 삼는다. 씬이 바인딩한 프리팹·그래프, 모델이 가리키는 메시·LOD·스켈레톤·클립·머티리얼, 머티리얼 인스턴스의 부모·텍스처·셰이더, 타임라인의 VFX·오디오까지 순회한다.
- `AssetReferenceValidator`가 확인하는 문서 참조와 Scene/Prefab Archive의 `ResourceReferences`를 활용하되, 파일에 있는 의존성을 읽을 수 있는 타입만 완료로 표시한다. BEVE 리소스는 GPU·오디오 초기화 없이 메타데이터와 지원하는 참조를 읽는 경로를 마련한다. 기존 파일에는 포맷별 리더를 적용하고, 새 임포트나 저장에 참조 목록을 기록하는 것은 작업량이 확인된 뒤 결정한다. [참조 검사](../Editor/Source/AssetManager/Private/AssetReferenceValidator.cpp) · [씬·프리팹 리소스 참조](../Engine/Source/World/Scene/Private/SceneArchive.cpp) · [시작 매니페스트](../Engine/Source/Resource/Private/StartupResourceLoader.cpp)
- 수집 키는 정규화한 `Project://`·`Engine://` URI다. 공유와 순환 의존성은 URI마다 한 번 처리한다. 기존 상대 경로는 쿠킹 입력 단계에서 현재 해석 규칙으로 확인해 정규화하고, 해석이 모호하면 진단한다. `ImportSourceUri`와 DCC 원본·임포트 캐시는 제작 의존으로 분류한다. 런타임에 실제 필요한 사진·오디오 등은 같은 확장자라도 사용처에 따라 포함한다. 절대 경로와 `Editor://` 의존성은 배포 참조로 허용하지 않는다. [마운트 기준](ResourceAndAssetSeparationPlan.md)
- 코드나 그래프에서 이름으로 조합해 로드하는 씬·프리팹·오디오 등은 빌드 프로필이나 모듈이 포함 목록으로 선언한다. 필수 항목 누락이나 검사 실패는 출처·의존 URI와 함께 보고한다. 첫 도입은 지원 범위가 확인된 대표 씬부터 진행하고, 그 외 프로젝트에는 현재 폴더 내보내기를 제공한다.

### 2.2 리소스 유형별 처리

- **씬·프리팹.** 기존 Archive 스키마를 사용한다. 첫 팩 검증에는 JSON 페이로드도 담을 수 있다. 이어서 동일 씬을 BEVE로 쿠킹해 엔티티 ID, 지원하는 엔티티 참조, 컴포넌트와 리소스 참조가 동등한지 확인한다. 출처 문서를 덮어쓰지 않는다. `Scene::CreateFromArchive()` 같은 읽기 경계는 재사용한다. [Archive 지침](ReflectionArchiveGuidelines.md) · [SceneArchive](../Engine/Source/World/Scene/Private/SceneArchive.cpp)
- **모델·메시·스켈레톤·클립·머티리얼.** 대상 플랫폼에서 읽을 수 있는 기존 BEVE 산출물을 검증해 재사용한다. JSON에서 BEVE로 변환할 때 타입과 버전, 필드, 의존 경로를 확인한다. 범용 문자열 치환으로 에셋 경로나 타입을 변환하지 않는다. 모델 원본을 재변환해야 하는 경우에는 기존 모델 변환기를 사용하고, Editor에서 수정한 재질·연결 값을 유지한다.
- **텍스처·셰이더.** 기존 DDS·`.bamtex`와 대상 백엔드용 DXIL 산출물을 먼저 재사용한다. 재변환이 필요한 텍스처는 기존 변환기의 컬러 공간·노멀맵·밉 정책을 검증하고, 셰이더는 단계·진입점·매크로와 include 의존성을 포함해 쿠킹한다. 필수 DXIL 누락은 게시 전에 보고한다. [텍스처 변환기](../Tools/Converters/TextureConverter/Private/DirectXTexCompressor.cpp) · [렌더 프로필 셰이더](RenderPipelineProfilePlan.md)
- **Animator Controller·타임라인·그래프·VFX.** 기존 런타임이 읽는 직렬화 형식을 먼저 팩에 담고 타입별 왕복을 확인한다. 바이너리 포맷으로 바꿀 때는 런타임 리더와 함께 전환한다. 그래프를 BEVE로 저장하는 것과 실행 바이트코드로 컴파일하는 것은 각각의 완료 기준을 둔다.
- **오디오·폰트.** FMOD의 `AudioClip`은 현재 파일명으로 읽거나 스트리밍한다. 폰트도 사용 중인 로더의 바이트 읽기와 수명 요건을 확인한다. 첫 단계에서는 파일 경로가 필요한 항목만 매니페스트의 허용 목록에 남기고, 이후 바이트 또는 범위 읽기를 지원하는 타입부터 팩으로 옮긴다. 스트리밍 오디오의 전체 파일 메모리 복사는 피하고 재생·탐색·종료를 검증한다. [AudioClip](../Engine/Source/Audio/Private/AudioClip.cpp)

쿠킹 캐시 키에는 입력 내용, 변환 결과에 영향을 주는 의존 파일, 옵션, 플랫폼·아키텍처·백엔드, 도구와 출력 스키마 버전을 포함한다. 기존 DDC의 경로와 해시 구성 요소는 재사용 가능성을 확인한다. 출력 폴더를 바꿔도 동일 결과를 재사용하고, 원본 삭제·의존성 변경이 팩에 반영되어야 한다. 변환 결과의 캐시와 최종 배포 폴더는 서로 다른 수명으로 관리한다. [기존 Cook 캐시 계획](BuildAndPackagingPlan.md#53-cook-캐시와-이전-배포본-재사용) · [DDC 공통 코드](../Tools/Converters/Common/DdcCommon.h)

## 3. 팩과 런타임 읽기 구조

### 3.1 첫 포맷과 배포 구성

첫 파일 확장자는 **`.bampak`을 제안**한다. Windows 첫 버전은 압축 없이 색인과 페이로드를 저장한다. 동작과 크기·I/O를 측정한 뒤 항목별 압축을 검토한다. 이미 압축된 텍스처·오디오의 추가 압축은 실측으로 결정한다. 압축 여부와 페이로드 포맷은 서로 다른 필드로 관리한다.

- **헤더**는 매직·팩 형식 버전과 색인 범위, **색인**은 UTF-8 URI, 에셋 타입·페이로드 형식, 64비트 오프셋·크기, 무결성 해시를 갖는다. 실제 필드 폭과 바이트 순서는 작성기와 리더를 함께 만들 때 고정한다. 메모리 구조체의 패딩에 파일 형식을 의존시키지 않는다.
- URI 정규화는 `/` 구분자와 마운트 이름을 통일한다. 허용된 마운트의 상대 경로만 색인에 기록하고 루트 밖으로 벗어나는 경로, 동일 URI와 Windows에서 대소문자만 다른 항목을 거부한다. 범위를 벗어나는 오프셋·크기, 중첩된 항목, 잘린 색인, 미지원 버전과 손상된 필수 데이터를 진단한다. 색인·항목 수와 읽을 수 있는 크기를 제한해 손상 파일이 큰 할당을 유발하지 않도록 한다.
- 첫 묶음은 엔진 리소스와 프로젝트 리소스 각 1개다. 코드와 DLL, 루트 `Config/Project.json`, 팩 매니페스트와 필요한 부트스트랩 설정은 개별 파일로 둔다. 첫 버전의 후보 구성은 아래와 같다. 팩은 내용이 같은 입력과 옵션에서 동일한 엔트리 순서와 해시를 얻도록 작성한다. 큰 팩은 전체를 메모리에 모으지 않고 순차 작성한다.

```text
BamEngine-Windows-x64/
  Client.exe
  Engine.dll, GameModule.dll, 런타임 DLL
  Config/Project.json
  Config/InputActions.json
  Config/PackageManifest.json
  Content/Engine.bampak
  Content/Project.bampak
```

매니페스트는 마운트할 팩과 버전·해시, 시작 씬과 부트스트랩 의존성, 파일 경로가 필요한 예외를 기록한다. 작업 시각과 절대 경로는 작업 로그에서 확인하고, 팩에는 재현에 필요한 논리 URI와 데이터만 기록한다. 전체 팩 해시는 게시 전 검증에 사용한다. Client 기동에서는 헤더·색인 및 필수 항목을 검사하고 나머지 항목은 읽을 때 검증해, 팩 전체를 매번 시작 시 읽는 비용을 피한다.

### 3.2 기존 로더에 연결하는 순서

1. `PlatformPath`의 프로젝트·엔진 URI 의미를 재사용하고, 에셋에 대한 **존재 확인과 바이트·범위 읽기**를 한 경계로 모은다. 현재 `ResolveUri()` 소비자는 `std::filesystem::path`를 받으므로, 해당 반환 값에 팩 항목을 가짜 디스크 경로로 섞지 않는다. 개발 모드는 디렉터리 입력, 팩 모드는 색인 입력을 제공한다. [PlatformPath](../Engine/Source/Core/Path/Public/PlatformPath.h)
2. `ResourceManager`의 로더 선택, 리소스 핸들 유효성, `AssetKey`·`AssetPath` 폴백을 유지하면서 실제 파일 존재 검사와 읽기를 새 경계로 옮긴다. 디스크와 팩의 동일 URI가 같은 핸들 키를 사용한다. Archive는 기존 `Decode()`와 JSON·BEVE 파서를 활용한다. 논리 URI는 유지하되 팩 항목의 포맷으로 디코더를 고른다. 이를 통해 `Intro.bamscene.json`을 참조하는 기존 논리 이름과 실제 BEVE 데이터 형식을 구분할 수 있다. [Archive 인터페이스](../Projects/Archive/include/archive/ArchiveInterface.h) · [리소스 로더](../Engine/Source/Resource/Public/ResourceManager.inl) · [핸들 복원](../Engine/Source/Resource/Public/ResourceHandleArchiveAdapter.h)
3. Client는 부트스트랩 설정을 읽은 뒤 팩을 마운트하고 시작 매니페스트·셰이더·시작 씬을 연다. `SceneManager::LoadScene()`, `StartupResourceLoader`, 프로필의 셰이더 로드, Animator/Graph 파일처럼 직접 경로를 읽는 소비자는 실제 파일 접근이 있는 지점마다 전환한다. 첫 팩의 필수 경로를 확인한 뒤 타입을 점진적으로 확대한다. Client가 참조 중인 데이터는 팩 핸들이 닫히기 전에 읽기나 스트리밍을 끝낸다. [Client 초기화](../Client/Source/Execution/Private/ClientGameRuntime.cpp) · [씬 로드](../Engine/Source/World/Scene/Private/SceneManager.cpp) · [시작 리소스 로더](../Engine/Source/Resource/Private/StartupResourceLoader.cpp)
4. 팩 배포 모드에서는 매니페스트가 허용한 개별 파일만 사용한다. 필수 팩·항목 누락 시 패키지 경로와 요청 URI를 로그에 기록한다. 현재 작업 디렉터리나 개발 체크아웃에 있는 파일이 누락을 가리지 않도록 런타임 루트를 제한한다. 로그·게임 저장 데이터는 기존 쓰기 가능 경로를 사용한다.

## 4. 적용 순서와 완료 기준

1. **선택 수집과 검증.** 기존 `[Package inventory]`에 URI·포함 이유·변환 방식·미지원 의존성을 기록한다. 시작 씬 하나, 프리팹 하나와 명시적 동적 로드 하나를 기준으로 필요한 파일만 개별 스테이징한다. 외부 참조와 필수 파일 누락을 게시 전에 잡고 독립 Client가 실행되면 다음 단계로 진행한다.
2. **첫 팩 읽기·쓰기.** 이미 로드 가능한 씬·메시·머티리얼의 작은 세트를 변경 없이 넣는다. `Archive` 파서와 기존 리소스 생성 경로에 연결해 원래 개별 파일과 색인 조회의 동등성을 확인한다. 인덱스 범위·중복 URI·잘린 파일·손상 데이터·미지원 버전의 헤드리스 검사를 추가한다.
3. **실제 시작 흐름 연결.** Windows Client가 팩에서 시작 씬과 필요한 엔진 리소스를 직접 읽게 한다. 재사용하는 BEVE 산출물과 새로 쿠킹한 장면을 같은 배포본에 넣는다. 캐릭터 입력, 씬·프리팹 전환, 스키닝, 타임라인, UI·오디오를 같은 입력으로 검증한다. 파일 입력이 남은 오디오·폰트 등의 허용 항목과 전환 현황을 배포 목록에 남긴다. [PIE와 Client 비교 절차](RenderPipelineProfilePlan.md#3단계-수동-검증)
4. **Editor 내보내기와 게시.** 기존 빌드 화면에서 팩 모드를 선택하고 수집·쿠킹·팩 작성·검증 단계와 실패 URI를 표시한다. 긴 변환·해시·파일 쓰기는 백그라운드에서 진행하고 취소 시 임시 작업을 정리한다. 존재하는 배포본은 검증한 스테이징으로 교체하며 기존 백업·복구 흐름을 재사용한다. 성공 시에도 원본 에셋과 제작 프로젝트 경로를 수정하지 않는다. UI 키는 `Editor.BuildProfile.csv`의 한·영 원본을 사용한다. [배포 게시 절차](BuildAndPackagingPlan.md) · [UI·로캘 지침](EngineEditorClientGuidelines.md)
5. **증분 작업과 성능.** 같은 입력으로 두 번 내보내고, 텍스처만 변경·에셋 삭제·렌더 프로필 변경을 각각 확인한다. 변경분만 다시 쿠킹하고 이전 팩에서 제거할 항목이 새 배포본에 남지 않아야 한다. 단계별 시간, 캐시 적중률, 배포 파일 수·바이트, Client 첫 화면과 첫 에셋 로드 시간, 최대 메모리를 기존 폴더 배포와 비교한다. 크기나 초기 로드가 병목으로 확인되면 항목별 압축·팩 분할을 검토한다.

패치 팩, 암호화·서명, DLC·다운로드, Web 배포는 Windows 팩의 호환·복구·성능 검증 뒤 요구에 따라 다룬다.

## 5. 검증 기준

- [ ] **바이너리 동등성.** 동일 씬의 JSON·BEVE 및 개별 파일·팩 로드에서 씬과 프리팹 계층, 리소스 키, Animator·타임라인 참조와 시각 결과가 같다.
- [ ] **독립 실행.** 출시용 팩 모드에서 개발 체크아웃을 참조할 수 없는 위치 또는 별도 PC에서 실행한다. 같은 테스트 씬에서 입력·프리팹·씬 전환·VFX·소리·UI가 동작하며, 미사용 에셋과 DCC 원본이 배포 목록에 없다.
- [ ] **실패 격리.** 필수 엔트리 누락, 잘못된 팩 버전, 손상, 잘못된 동적 포함 경로와 취소를 확인한다. 실패한 작업은 이전 배포본을 교체하지 않는다. 팩 누락을 개별 개발 파일이 가리지 않는다.
- [ ] **재현과 비용.** 같은 입력에서 팩 항목과 해시가 재현된다. 증분 사례별로 다시 쿠킹한 에셋과 실제 배포 크기를 기록한다. 초기 로드와 메모리도 측정한다.

구현 후 사용자가 먼저 실행할 기존 회귀 검사다. 팩 검사 타깃은 구현 단계에서 추가한다.

```powershell
cmake --preset x64-debug -DBUILD_TESTING=ON
cmake --build --preset x64-debug --config Debug --target Editor BamAssetWorker SceneArchiveCheck AssetReferenceCheck RenderPipelineProfileCheck AuditLocalization
ctest --preset x64-debug --output-on-failure -R "SceneArchiveCheck|AssetReferenceCheck|RenderPipelineProfileCheck|AssetWorkerModelCheck|LocalizationAuditUtility"
```

이번 문서 작성에서는 소스, 기존 배포·마운트 기준과 회귀 검사 타깃을 확인했다. 쿠킹·팩 구현, 빌드·실행, 크기와 로드 성능은 이후 단계에 남아 있다.
