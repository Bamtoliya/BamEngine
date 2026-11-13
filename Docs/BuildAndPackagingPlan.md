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
