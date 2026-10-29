# 다음 목표: Client 실행 설정과 빌드·패키징

- 작성일: 2026-10-24
- 상태: 계획 / 구현 보류
- 관련 지침: [EngineEditorClientGuidelines.md](EngineEditorClientGuidelines.md)의 3절, 12절, 13절
- 직렬화 지침: [ReflectionArchiveGuidelines.md](ReflectionArchiveGuidelines.md)
- 분류·이관 기준: [SourceOrganizationGuidelines.md](SourceOrganizationGuidelines.md)
- 통합 우선순위: [DevelopmentPriorities.md](DevelopmentPriorities.md)의 P2-E / 현재 보류

## 1. 진행 결정

Client의 시작 씬을 `ProjectSettings.StartupScenePath`에 연결하는 작업은 지금 진행하지 않는다.
사용자 결정에 따라 빌드·패키징 작업을 시작할 때 실행 경로, 시작 리소스, 배포 설정과 함께 연결한다.
통합 계획에서 배포를 완료 목표로 다루더라도 이 보류를 해제하지 않는다.
재개 시에는 제작 UI와 공용 로딩/변환 처리를 구분하고, Client에 Editor 의존성을 추가하지 않는다.

이 문서는 다음 개발 목표를 기록한 계획이다. 기능 구현이나 실행 검증이 완료됐다는 뜻이 아니다.
현재 에디터 설정 저장·복원 작업과 Client 배포 작업의 범위를 구분한다.

## 2. 현재 코드에서 확인한 상태

- `Engine/Source/Core/Config/Public/Config.h`에 공용 `ProjectSettings`가 있다.
  `SchemaVersion`, `ProjectName`, `StartupManifestPath`, `StartupScenePath`를 저장한다.
- `Config/Project.json`의 `StartupScenePath`는 현재 빈 문자열이다.
- Editor는 `ConfigManager`로 프로젝트 설정을 읽고 `StartupManifestPath`를 시작 리소스 로드에 사용한다.
- `Client/Source/Main.cpp`의 `LoadStartupScene()`은 `Resources/Scenes/NewScene2.json`을 고정으로 읽는다.
  Client에는 `ProjectSettings`를 읽어 시작 씬에 반영하는 연결이 아직 없다.
- `Client/CMakeLists.txt`는 Visual Studio 디버거 작업 디렉터리를 소스 루트로 지정하고,
  `BAMENGINE_PROJECT_ROOT`를 정의한다. 이 정의의 실제 사용 여부와 런타임 경로 의존성은 추가 확인 대상이다.
- `StartupResourceLoader`는 현재 Editor의 AssetManager 소스 영역에 있다.
  Client에서 재사용할 범위와 의존성은 빌드·패키징 작업에서 검토한다.

위 내용은 문서 작성 시점의 소스 확인 결과다. Client 빌드·실행 및 배포 검증 결과는 아니다.

## 3. 완료 목표

Editor에서 제작하고 저장한 프로젝트를 배포 폴더로 구성하고,
개발 저장소와 Editor 없이 Client에서 실행할 수 있게 한다.

- 배포된 프로젝트 설정으로 시작 씬과 시작 리소스를 선택한다.
- 실행 위치와 데이터 기준 경로를 명시하고, 임의의 현재 작업 디렉터리에 의존하지 않는다.
- 실행에 필요한 DLL, 프로젝트 모듈과 리소스를 포함한다.
- 누락된 설정·파일·의존성은 원인을 식별할 수 있는 로그와 실패 결과로 전달한다.
- 같은 빌드·패키징 처리를 Editor UI 밖에서도 호출할 수 있게 구성한다.

## 4. 구현 순서

### 1단계: 실행 경로와 설정 계약

- 실행 파일, 프로젝트 루트, 설정, 리소스와 로그의 기준 경로를 정한다.
- 실행 파일 위치를 배포 루트의 기본 후보로 검토하고, 개발 실행과 배포 실행의 차이를 명시한다.
- `Config/Project.json`의 배치 위치와 경로 해석 규칙을 정한다.
- 시작 씬의 빈 값, 잘못된 스키마 버전과 지원하지 않는 경로에 대한 실패 정책을 정한다.
- 에디터 작업 복원용 `LastScenePath`와 배포 실행용 `StartupScenePath`의 용도를 구분한다.

### 2단계: Client 시작 흐름 연결

- ReflectionRegistry가 준비된 뒤 기존 `ConfigManager`로 `ProjectSettings`를 읽는다.
- 하드코딩된 시작 씬을 제거하고 검증된 `StartupScenePath`를 사용한다.
- `StartupManifestPath`의 리소스 로드와 시작 씬 로드 순서를 연결한다.
- 실행 리소스 로드는 Engine/Client에서 재사용할 수 있는 위치와 인터페이스로 정리한다.
  Editor의 임포트 UI나 편집 상태를 Client 의존성으로 가져오지 않는다.
- 설정 파싱·경로 검증·리소스 초기화·씬 복원 실패를 구분해 보고한다.

### 3단계: Build와 Cook

- 우선 Windows에서 Visual Studio로 Release Client와 필요한 프로젝트 모듈을 빌드한다.
- 실행에 필요한 셰이더와 에셋의 변환 결과를 확인한다.
- 비주얼 스크립트 그래프의 최적화 포장(바이너리 바이트코드 Baking 및 C++ Nativization, [VisualScriptingBlueprintPlan.md](VisualScriptingBlueprintPlan.md) 참고) 대상을 구분한다.
- 원본 에셋과 실행용 데이터의 포함 기준을 정한다.
- 자동 참조 수집만으로 찾기 어려운 동적 로드 에셋은 명시적으로 포함할 방법을 마련한다.

### 4단계: Stage와 Package

- 전용 출력 폴더에 실행 파일, Engine DLL, 필요한 런타임 DLL과 프로젝트 모듈을 수집한다.
- 프로젝트 설정, 시작 씬, 시작 매니페스트와 실행 리소스를 경로 규칙에 맞게 배치한다.
- Editor 개인 설정, `.bam/User` 작업 상태와 개발 로그는 배포 대상에서 제외한다.
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
└─ Resources/
   ├─ Config/Startup/
   ├─ Scenes/
   └─ 실행에 필요한 리소스
```

## 6. 완료 검증

- [ ] 배포 폴더를 개발 저장소 밖으로 옮겨 실행한다.
- [ ] 현재 작업 디렉터리가 배포 폴더와 달라도 지정한 프로젝트를 실행한다.
- [ ] `StartupScenePath`를 바꾸면 해당 씬이 선택된다.
- [ ] 시작 리소스와 씬의 에셋 참조가 복원되고 실제 화면·입력·필요한 게임 동작을 확인한다.
- [ ] Editor 실행 파일과 개인 작업 상태 없이 Client가 실행된다.
- [ ] 설정 누락, 잘못된 버전과 존재하지 않는 시작 씬에서 원인을 기록하고 실패한다.
- [ ] 필수 리소스나 프로젝트 모듈 누락 시 원인을 확인할 수 있다.
- [ ] 빌드, 배포 폴더 생성, 실제 실행 결과를 각각 기록한다.

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
