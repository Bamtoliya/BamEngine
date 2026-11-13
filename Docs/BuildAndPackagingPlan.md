# 다음 목표: Client 실행 설정과 빌드·패키징

- 작성일: 2026-10-24 (최종 갱신: 2026-11-13)
- 상태: Editor의 Windows x64 Release 빌드 및 폴더 내보내기에 Deferred 렌더 프로필 검증과 배포 경로 기록을 연결했다. 저장소 밖 실행과 Editor·Client 렌더링 일치 검증은 남아 있다.
- 렌더링 공유 후속 설계: [Render Pipeline Profile](RenderPipelineProfilePlan.md)
- WebGPU 진행 상태: `Tools/WebGPUProbe/main.cpp`의 독립 브라우저 렌더링 프로브까지 기록했다. Client 씬 로드와 빌드 패널 내보내기 연결은 후속 범위다.
- 관련 지침: [EngineEditorClientGuidelines.md](EngineEditorClientGuidelines.md)의 3절, 12절, 13절
- 직렬화 지침: [ReflectionArchiveGuidelines.md](ReflectionArchiveGuidelines.md)
- 분류·이관 기준: [SourceOrganizationGuidelines.md](SourceOrganizationGuidelines.md)
- 통합 우선순위: [DevelopmentPriorities.md](DevelopmentPriorities.md)의 P2-E
- 최신 대조: [2026-11-06 점검](CodeDocumentationAudit-2026-11-06.md) A01·A02·A08·A10. 이번 갱신에서 새 빌드·실행·패키징은 하지 않았다.

## 1. 진행 결정

2026-10-24에는 Client 시작 씬·빌드·패키징을 함께 보류했다.
2026-11-02 현재 코드는 실행 루트 탐색과 `StartupScenePath` 연결을 이미 포함하므로 그 부분은 구현 반영 상태로 갱신한다.
2026-11-13 기준 Editor 패널에 Windows Release 빌드와 폴더 내보내기가 연결됐다.
Client 시작 매니페스트 연결, 참조 기반 최소 에셋 수집과 저장소 밖 실행 검증이 남아 있다.
후속 작업에서는 제작 UI와 공용 로딩/변환 처리를 구분하고, Client에 Editor 의존성을 추가하지 않는다.

2026-11-02 우선순위 조정에서는 **삭제 범위 보호(F03)**와 **기존 Client의 설정 실패 보고(F05 일부)**를 작은 안정화 작업으로 분리한다.
바이너리/설정/참조 수집(F04), 매니페스트·에셋 루트·배포 렌더 경로와 Cook/Stage/Package는 배포 재개 경로에서 진행한다.
전체 배포 완료를 다른 제작·렌더링·애니메이션 작업의 공통 선행 조건으로 두지 않는다.
2026-11-06 사용자 결정에 따라 A01·A02의 출력 안전성과 A08의 Client 설정 정책을 포함한 Client 실행·배포 작업은 빌드·패키징 단계에서 함께 진행한다. 통합 작업 순서는 [DevelopmentPriorities.md 11절](DevelopmentPriorities.md)을 따른다.

이 문서는 코드 반영 범위와 남은 개발 목표를 함께 기록한다. 1~2단계 전체나 배포 검증 완료를 의미하지 않는다.
현재 에디터 설정 저장·복원 작업과 Client 배포 작업의 범위를 구분한다.

## 2. 현재 코드에서 확인한 상태

- `Engine/Source/Core/Config/Public/Config.h`에 공용 `ProjectSettings`가 있다.
  `SchemaVersion`, `ProjectName`, `AssetDirectory`, `EngineResourceDirectory`, `StartupManifestPath`, `StartupScenePath`, `ShaderSourceDirectory`를 가진다.
- `Config/Project.json`의 `StartupScenePath`는 `Scenes/RaytracingScene.bamscene.json`이며, 내보내기 시 선택한 씬의 에셋 루트 상대 경로를 배포본에 기록한다.
- Editor는 `ConfigManager`로 프로젝트 설정을 읽고 `StartupManifestPath`를 시작 리소스 로드에 사용한다.
- Client는 실행 파일 위치와 프로젝트 마커로 루트를 찾고 Runtime/PlatformPath에 주입한다.
- `LoadProjectSettings()`가 설정을 읽고 `LoadStartupScene()`이 지정한 씬을 선택한다. 빈 값일 때만 NewScene2로 폴백한다.
- 설정 로드 결과·스키마 버전을 검사한다. `NotFound`는 경고 후 기본값으로 성공, 파싱 실패는 실패, 0 또는 지원 초과 버전은 InvalidArgument다. Runtime을 만들기 전에 설정을 읽어 리소스 루트를 전달한다. Client의 범용 `StartupManifestPath` 연결은 남아 있다.
- Client는 프로필 경로를 지정하면 Deferred 타깃, 조명, 씬 후처리, UI와 백버퍼 출력 패스를 구성한다. 빈 경로는 기존 단일 Forward 패스를 사용한다. [렌더 프로필 수동 검증](RenderPipelineProfilePlan.md#3단계-수동-검증)
  Editor의 디퍼드·조명·HDR/후처리와 동등한 렌더링 경로는 아니다.
- `Client/CMakeLists.txt`의 Visual Studio 디버거 작업 디렉터리는 소스 루트다. 해당 타깃에 과거 `BAMENGINE_PROJECT_ROOT` 정의는 없다.
- `StartupResourceLoader`는 현재 Editor의 AssetManager 소스 영역에 있다.
  Client에서 재사용할 범위와 의존성은 빌드·패키징 작업에서 검토한다.
- `PackageClient.ps1`에는 바이너리 탐색·설정 수집·일부 경로 삭제 거부가 있다. 그러나 `Client`/`Utility`와 임의 외부 출력 삭제를 막지 못하며, 기존 출력 삭제 후 `New-Item -LiteralPath`에서 실패할 수 있다. 과거 독립 실행 기록은 현행 스크립트 성공의 근거가 아니다(A01·A02).

기존 ImplementationLog에는 다른 CWD에서 Client 초기화·지오메트리 표시를 확인한 기록이 있다.
이번 갱신은 소스와 기존 로그의 대조이며 해당 실행이나 저장소 밖 배포를 새로 검증한 결과가 아니다.

## 3. 완료 목표

Editor에서 제작하고 저장한 프로젝트를 배포 폴더로 구성하고,
개발 저장소와 Editor 없이 Client에서 실행할 수 있게 한다.

- 배포된 프로젝트 설정으로 시작 씬과 시작 리소스를 선택한다.
- 실행 위치와 데이터 기준 경로를 명시하고, 임의의 현재 작업 디렉터리에 의존하지 않는다.
- 실행에 필요한 DLL, 프로젝트 모듈과 리소스를 포함한다.
- 누락된 설정·파일·의존성은 원인을 식별할 수 있는 로그와 실패 결과로 전달한다.
- Editor의 `파일 > 빌드 및 패키징…` 패널에서 설정, 실행, 결과 확인과 내보내기를 진행한다.

## 4. 구현 순서

### 선행 안전성 보완 (2026-11-06)

- 빌드·패키징 단계가 시작되면 A01/A02를 먼저 처리한다. 출력은 관리 영역의 전용 하위 폴더로 제한하고 실제 경로·입력 겹침을 검증한다.
- 모든 필수 바이너리·설정을 기존 출력 삭제 전에 검사한다. 지원되는 디렉터리 생성 API로 staging을 만든 뒤 성공한 결과만 교체한다.
- 경로 거부 검사는 삭제 없이 수행하며 소스 경로를 실제 재귀 삭제하는 테스트를 하지 않는다.
- A08에 따라 배포의 설정 누락 실패 정책을 확정한다. 현행 기본값 폴백을 문서만으로 변경 완료 처리하지 않는다.

### 1단계: 실행 경로와 설정 계약 — 경로 헬퍼 반영 / 실패 정책 미완료

- 실행 파일, 프로젝트 루트, 설정, 리소스와 로그의 기준 경로를 정한다.
- 실행 파일 위치를 배포 루트의 기본 후보로 검토하고, 개발 실행과 배포 실행의 차이를 명시한다.
- `Config/Project.json`의 배치 위치와 경로 해석 규칙을 정한다.
- 시작 씬의 빈 값, 잘못된 스키마 버전과 지원하지 않는 경로에 대한 실패 정책을 정한다.
- 에디터 작업 복원용 `LastScenePath`와 배포 실행용 `StartupScenePath`의 용도를 구분한다.

### 2단계: Client 시작 흐름 연결 — 설정 기반 씬 반영 / 매니페스트 미연결

- ReflectionRegistry가 준비된 뒤 기존 `ConfigManager`로 `ProjectSettings`를 읽는다.
- 설정 기반 씬 선택은 반영되었다. 설정 검증과 빈 시작 씬의 명시적 실패/폴백 정책을 마무리한다.
- `StartupManifestPath`의 리소스 로드와 시작 씬 로드 순서를 연결한다.
- 실행 리소스 로드는 Engine/Client에서 재사용할 수 있는 위치와 인터페이스로 정리한다.
  Editor의 임포트 UI나 편집 상태를 Client 의존성으로 가져오지 않는다.
- 설정 파싱·경로 검증·리소스 초기화·씬 복원 실패를 구분해 보고한다.

### 3단계: Build와 Cook

- `BuildProfilePanel`의 `Client 빌드`가 기존 CMake `x64-release` 프리셋을 사용해 별도 `x64-client-release` 폴더에서 Editor 지원을 끈 Windows Release Client와 GameModule을 빌드한다. 패널은 완료 로그를 표시하고 복사할 수 있게 한다.
- 시작 씬, 내보낼 상위 폴더, 패키지 폴더 이름과 프로그램 이름은 Editor Workspace에 저장한다. `현재 씬 사용`은 편집 모드에서 저장된 현재 씬을 선택한다.
- 선택한 시작 씬의 저장된 JSON 참조 검사는 패널에서 수행한다. 빌드가 생성하는 셰이더와 배포 대상 참조는 산출물을 기준으로 확인한다.
- `AnimatorComponent.controllerPath`와 컨트롤러 문서에 들어 있는 클립 참조를 같은 검사에서 따라간다. Runtime과 패키저는 동일한 컨트롤러 경로 해석을 사용한다.
- 실행에 필요한 셰이더와 에셋의 변환 결과를 확인한다.
- 비주얼 스크립트 그래프의 최적화 포장(바이너리 바이트코드 Baking 및 C++ Nativization, [VisualScriptingBlueprintPlan.md](VisualScriptingBlueprintPlan.md) 참고) 대상을 구분한다.
- 원본 에셋과 실행용 데이터의 포함 기준을 정한다.
- 자동 참조 수집만으로 찾기 어려운 동적 로드 에셋은 명시적으로 포함할 방법을 마련한다.

### 4단계: Stage와 Package

- `빌드 및 내보내기`는 Windows x64 Release 빌드 성공 후 사용자가 정한 폴더를 게시한다. 기본값은 `BamEngine-Windows-x64`와 `Client.exe`다. 프로그램 이름에는 확장자를 붙이지 않으며 같은 값을 배포 설정의 `WindowTitle`에 기록한다. 패널에서 최종 실행 파일 경로를 확인하고 성공 후 배포 폴더를 열 수 있다.
- 기존 BamEngine Client 폴더에는 전체 경로를 보여주는 **취소 / 덮어쓰기** 확인을 표시한다. 스테이징 검증이 끝나면 승인 시점의 폴더가 그대로인지 확인한 뒤 이전 패키지를 형제 백업 폴더로 옮기고 새 패키지를 게시한다. 게시 실패 시 복원하며, 복원 실패 또는 성공 후 이전 패키지의 위치는 로그에 남긴다. 기존 배포본은 백업에 보관한다.
- 대상이 일반 폴더가 아니거나 프로젝트 루트 또는 에셋·엔진 리소스·빌드 입력과 겹치면 내보내기를 거부한다. 이 기능이 만든 패키지는 `.bam-client-package` 마커로, 이전 기본 패키지는 `Client.exe`, `Engine.dll`, `Config/Project.json` 조합으로 식별한다.
- 첫 폴더 출력은 현재 Windows Client의 알려진 실행 바이너리, 검사로 확인한 파일 및 두 리소스 루트의 런타임 파일 형식을 보존한다. 배포본 `Config/Project.json`에 선택한 씬을 기록한다. 바이너리 참조 분석, 최소 에셋 수집과 실제 독립 실행은 후속 검증 항목이다.
- 전용 출력 폴더에 실행 파일, Engine DLL, 필요한 런타임 DLL과 프로젝트 모듈을 수집한다.
- [ResourceAndAssetSeparationPlan.md](ResourceAndAssetSeparationPlan.md)의 원칙에 따라, 런타임 필수 엔진 빌트인 리소스(`Engine/Resources/`)와 시작 씬이 참조하는 프로젝트 애셋(`Assets/`)을 분리 수집한다.
- 프로젝트 설정, 시작 씬, 시작 매니페스트와 실행 리소스를 경로 규칙에 맞게 배치한다.
- Editor 개인 설정, `.bam/User` 작업 상태, 에디터 전용 리소스(`Editor.csv`, 에디터 아이콘/셰이더 등), 미사용 원본 테스트 파일(`.zip`, `.blend`) 및 개발 로그는 배포 대상에서 제외한다.
- 첫 완료 목표는 실행 가능한 배포 폴더로 삼는다. 압축 배포나 설치 프로그램은 이후 범위로 둔다.

### 5단계: 선택적 쿠킹과 증분 스테이징

**상태:** Windows x64 Release 폴더 내보내기에 배포 목록 비교 로그를 추가했다. 선택적 수집과 증분 스테이징은 후속 작업이다. 현재 `ClientPackage.cpp`는 고정된 DLL 목록을 복사하고 `Assets/`와 `Engine/Resources/`에서 허용 확장자의 파일을 모두 순회한다. `AssetReferenceValidator`는 시작 씬에서 JSON과 Animator 컨트롤러 참조를 따라가지만, 바이너리 에셋 내부와 런타임 코드의 동적 로드까지 수집하지는 못한다. [패키저](../Editor/Source/Build/Private/ClientPackage.cpp) · [참조 검사](../Editor/Source/AssetManager/Private/AssetReferenceValidator.cpp)

#### 5.1 배포에 필요한 항목 수집

- **시작점:** 빌드 프로필에 선택된 시작 씬과 추가 씬, 런타임 시작 리소스, 렌더 파이프라인 프로필, 프로젝트에서 활성화한 런타임 모듈 및 명시적 추가 에셋을 목록의 시작점으로 삼는다. 씬 전환, 코드에서 이름으로 로드하는 에셋, 런타임 스폰용 프리팹은 추가 목록이나 모듈 선언으로 포함한다.
- **프로젝트 애셋:** 씬 → 프리팹 → 컴포넌트 → 메시·머티리얼·텍스처, Animator → 스켈레톤·클립처럼 간접 참조도 순회한다. `Project://`와 `Engine://`를 런타임과 같은 규칙으로 해석하고, 동일 항목과 순환 참조는 한 번만 처리한다. JSON과 바이너리 포맷의 참조는 타입별 리더나 임포트·저장 시 생성한 의존성 목록으로 읽는다.
- **엔진 리소스:** Client 시작에 필요한 최소 리소스에 선택한 렌더 파이프라인·셰이더 백엔드, 활성 시스템·모듈의 리소스, 씬에서 직접 참조하는 `Engine://` 파일을 합친다. 예를 들어 Windows D3D12의 Deferred 씬에서 쓰는 스키닝 그림자·파티클 셰이더와 기본 텍스처는 해당 기능의 의존성으로 포함한다. Editor 전용 데이터와 대상 백엔드에서 사용하지 않는 셰이더는 수집 목록에서 제외한다.
- **누락과 진단:** 각 항목에 포함 이유와 참조 출처를 남긴다. 필수 의존성을 찾지 못하면 어느 씬·컴포넌트·모듈에서 요구했는지 보고하고 게시 전에 실패한다. 아직 의존성 분석을 지원하지 않는 포맷은 명시적 포함 목록으로 보완하고, 의존성이 검증되지 않은 프로젝트에는 기존 전체 수집 경로를 사용한다.

#### 5.2 런타임 모듈과 시스템 선택

- **모듈 선택:** 프로젝트에서 활성화한 런타임 모듈을 시작으로 전이 의존성을 확장한다. 각 모듈은 필요한 다른 모듈, 등록하는 컴포넌트와 시스템, 초기화 순서의 제약, 런타임 라이브러리와 리소스, 지원 대상 플랫폼을 선언한다. 빌드 작업은 선택 목록에 해당하는 타깃을 빌드하고, 스테이징은 그 바이너리와 의존 DLL을 포함한다. 네이티브 링크 의존성에 더해 `ModuleManager`로 동적 로드하는 모듈도 선언에 포함한다.
- **시스템 선택:** Client 기본 시스템, 선택 모듈이 등록하는 시스템, 포함 씬·프리팹이 요구하는 시스템을 합친다. 공용 여부는 2D/3D 구분보다 실제 의존성으로 판단한다. 예를 들어 둘 다 사용하는 Transform·입력 서비스는 공용으로, Tilemap이나 스키닝 애니메이션은 사용하는 기능이 요구할 때 포함할 후보다. 씬 데이터에 직접 나타나지 않는 입력 제어·스폰·전역 시스템은 모듈 또는 프로젝트에서 명시한다.
- **현재 코드에서의 전환:** Client는 `InitializeSystems()`에서 시스템을 직접 등록하고, `GameModule`은 `GameplayScore`를 직접 로드한다. 먼저 PIE와 Client가 같은 모듈·시스템 선택 목록을 소비하도록 한다. 시스템 등록을 건너뛰면 실행 작업은 줄지만, 시스템 코드가 `Engine.dll`에 링크되어 있으면 그 DLL의 크기는 그대로다. 바이너리 크기 절감은 필요한 경우 해당 기능을 별도 모듈 또는 선택 빌드 타깃으로 옮긴 뒤 측정한다. [Client 초기화](../Client/Source/Main.cpp) · [런타임 모듈 설계](EditorPluginArchitecturePlan.md#7-gamemodule에서-사용하는-런타임-플러그인)

#### 5.3 Cook 캐시와 이전 배포본 재사용

- **Cook:** 수집된 리소스 중 변환이 필요한 항목만 대상 플랫폼의 런타임 형식으로 만든다. 이미 호환되는 메시·텍스처·셰이더는 기존 산출물을 쓴다. 캐시 키에는 입력과 변환에 영향을 주는 의존성의 내용 해시, 대상 플랫폼·아키텍처·백엔드, 변환 옵션과 도구·포맷 버전을 넣는다. 변환 결과는 배포 폴더 바깥의 빌드 캐시에 보관해 출력 경로를 바꿔도 재사용할 수 있게 한다.
- **Stage:** 현재 수집 목록으로 새 형제 스테이징을 구성한다. 같은 출력 경로에 다시 내보낼 때는 직전 배포본의 상대 경로와 내용이 일치하는 항목을 재사용한다. 첫 적용은 변경 빈도가 낮은 엔진 셰이더·텍스처부터 한다. 동일 NTFS 볼륨의 개발용 출력은 읽기 전용으로 관리하는 리소스에 한해 하드링크를 사용할 수 있다. 하드링크의 어느 경로에서 파일을 수정해도 모든 경로에 반영되므로, 편집 가능한 데이터와 다시 쓰는 설정·프로필은 독립 파일로 만든다. 링크를 사용할 수 없거나 독립 복사본이 필요하면 복사한다. 원본이 삭제됐거나 수집 목록에서 빠진 파일은 새 스테이징에 들어가지 않는다.
- **Publish:** 새 스테이징을 검사한 뒤 기존 결과를 형제 `.bam-previous-*` 폴더로 옮기고 게시한다. 실패 시 복원하는 기존 흐름을 유지한다. 최종 배포본은 개발 원본이나 캐시 경로에 의존하지 않고 실행되어야 한다.

#### 5.4 적용 순서와 완료 기준

1. Windows 폴더 내보내기의 `[Package inventory]`에서 스테이징한 프로젝트 애셋과 엔진 리소스의 전체 크기, 확인된 의존성, 분류가 필요한 파일의 수·바이트와 예시를 비교한다. 확인된 의존성은 시작 씬, 참조 검사 결과, 선택한 렌더 프로필과 필수 렌더 리소스를 포함한다. 런타임 바이너리는 현재 고정 목록의 이름·수·바이트를 기록한다. 생성 설정과 패키지 마커는 이 집계에서 제외한다. 분류가 필요한 파일에는 동적 로드·미지원 포맷 의존성이 있을 수 있으므로, 파일의 필요 여부를 확인한 뒤 선택적 수집을 적용한다.
2. `RaytracingScene3`로 선택적 수집을 검증한다. Fox의 조작·애니메이션·스키닝 그림자, 하늘 앞 파티클, 런타임에서 로드하는 프리팹이나 추가 씬이 PIE와 저장소 밖 Client에서 동작해야 한다. 미사용 대용량 에셋과 비활성 선택 모듈은 빠지고, 필수 파일이나 모듈 누락은 기존 배포본을 교체하기 전에 보고돼야 한다.
3. 같은 입력으로 다시 내보내고, 이후 텍스처 변경·애셋 삭제·렌더 프로필 변경을 각각 확인한다. 변경된 입력만 다시 변환·복사되고, 제거된 파일이 남지 않으며, 사용하던 출력 경로와 새 경로 모두 독립 실행이 가능해야 한다. Cook 캐시 적중률, 재사용 파일 수, 실제 복사 바이트, 단계별 시간과 최종 패키지 크기를 전체 수집 기준값과 비교한다.

## 5. 초기 배포 폴더 후보

아래 구조는 제안이며, 1단계에서 경로 계약을 확정한 뒤 적용한다.

```text
Package/
├─ Client.exe
├─ Engine.dll
├─ 필요한 런타임 DLL
├─ GameModule.dll            # 해당 프로젝트가 사용하는 경우
├─ Config/
│  └─ Project.json
├─ Engine/
│  └─ Resources/             # 런타임 최소 엔진 빌트인 리소스 (Shaders, Textures, Meshes, Localization)
└─ Assets/                   # 프로젝트 사용자 애셋 (Scenes, Models, Materials 등)
```

## 6. 완료 검증

- [ ] 배포 폴더를 개발 저장소 밖으로 옮겨 실행한다.
- [ ] 현재 작업 디렉터리가 배포 폴더와 달라도 지정한 프로젝트·상대 컨트롤러 경로·미로드 리소스 탐색이 동작한다.
- [ ] `StartupScenePath`를 바꾸면 해당 씬이 선택된다.
- [ ] 시작 리소스와 씬의 에셋 참조가 복원되고 실제 화면·입력·필요한 게임 동작을 확인한다.
- [ ] Editor 실행 파일과 개인 작업 상태 없이 Client가 실행된다.
- [ ] 설정 누락은 현행 기본값 폴백과 배포용 실패 정책을 구분해 확정·검증한다. 파싱 실패·잘못된 버전·존재하지 않는 시작 씬은 실패 결과를 확인한다.
- [ ] 필수 리소스나 프로젝트 모듈 누락 시 원인을 확인할 수 있다.
- [ ] 빌드, 배포 폴더 생성, 실제 실행 결과를 각각 기록한다.
- [ ] 패키징 출력 경로가 소스/리소스/드라이브 루트이면 삭제 전 거부한다.
- [ ] 필수 바이너리·설정·씬 참조 누락 시 패키징을 실패로 처리한다.

## 7. 재개 시 확인할 파일

- `Client/Source/Main.cpp`
- `Client/CMakeLists.txt`
- `Engine/Source/Core/Config/Public/Config.h`
- `Engine/Source/Core/Config/Public/ConfigManager.h`
- `Engine/Source/Runtime/Private/Runtime.cpp`
- `Editor/Source/Application/Private/Application.cpp`
- `Editor/Source/AssetManager/Public/StartupResourceLoader.h`
- `Editor/Source/AssetManager/Private/StartupResourceLoader.cpp`
- `Config/Project.json`
- `Resources/Config/Startup/StartupManifest.json`

재개할 때는 이 목록의 최신 코드와 실제 배포 산출물을 다시 확인한다.
이전에 제안한 Client 코드 조각은 적용 완료로 간주하지 않는다.
