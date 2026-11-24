# 멀티 프로젝트 분리 및 외부 폴더 마운트 아키텍처 설계 (Multi-Project & Content Mount Plan)

- 작성일: 2026-11-24
- 상태: Phase 1~5 구현 완료 (정적 검증 완료, 사용자 빌드·실행 검증 대기)
- 관련 문서: [ResourceAndAssetSeparationPlan.md](ResourceAndAssetSeparationPlan.md), [BuildAndPackagingPlan.md](BuildAndPackagingPlan.md), [DiskCacheDevelopmentPlan.md](DiskCacheDevelopmentPlan.md), [EditorPluginArchitecturePlan.md](EditorPluginArchitecturePlan.md)
- 관련 코드: `Engine/Source/Core/Path/Public/PlatformPath.h`, `Engine/Source/Core/Path/Public/ProjectDescriptor.h`, `Engine/Source/Resource/Public/ResourceManager.h`, `Editor/Source/Settings/Public/EditorSettingsService.h`, `Editor/Source/Project/Public/ProjectSessionManager.h`, `Editor/Source/ImGui/ProjectHub/Public/ProjectHubModal.h`, `Editor/Source/ImGui/ContentBrowserPanel/Public/ContentBrowserPanel.h`, `Engine/Source/Resource/Public/PakFormat.h`, `Editor/Source/AssetManager/Private/AssetReferenceValidator.cpp`, `Editor/Source/Build/Private/ClientPackage.cpp`, `Engine/examples/VirtualPathCheck.cpp`

---

## 1. 배경 및 목표

현재 BamEngine은 `PlatformPath`에 `Engine://`, `Editor://`, `Project://`, `Plugin://` 가상 마운트 계층을 갖추고 있으나, 실행 시점에는 **엔진 저장소 루트 = 단일 프로젝트 루트**로 고정되어 있다. 이로 인해 `Config/Project.json`, `Assets/`, `.bam/cache/`, `Temp/`, `Logs/`가 단일 작업 공간에 혼재된다.

본 설계의 목표는 다음과 같다.

1. **프로젝트 물리 격리**: 새 프로젝트를 임의의 독립 폴더에 생성하고, 프로젝트 간 `Assets/`, `Config/`, 캐시, 복구 데이터를 완전히 분리한다.
2. **에디터 내 프로젝트 전환(In-Process Switch)**: 에디터 프로세스를 종료·재실행하지 않고, 실행 중인 에디터 안에서 활성 프로젝트만 안전하게 언로드 후 교체한다.
3. **외부 폴더 가상 마운트(`Mount://`)**: 프로젝트 `Assets/` 외부의 공용 에셋 라이브러리나 참조 폴더를 복사 없이 콘텐츠 브라우저에 연결하되, 물리 절대 경로 오염을 차단하고 의존성 기반 `.bampak` 패키징과 통합한다.
4. **2단계 C++ 모듈 분리 대비**: 1단계(에셋·설정·캐시·외부 마운트 분리) 완료 후 프로젝트별 독립 C++ `GameModule` 빌드·핫리로드로 자연스럽게 확장할 수 있는 경계를 마련한다.

---

## 2. 디렉터리 구조 및 `.bamproject` 규격

### 2.1 엔진 루트와 프로젝트 루트 분리

```text
<EngineRoot>/                          # 공유 (읽기 전용 성격)
├─ Engine/Resources/                   # Engine://
├─ Editor/Resources/                   # Editor://
└─ out/build/.../Editor.exe

<UserProjects>/MyGameA/                # ProjectRoot (프로젝트별 완전 격리)
├─ MyGameA.bamproject                  # 프로젝트 매니페스트 (진입 파일)
├─ Config/
│  ├─ Project.json                     # 프로젝트 런타임 설정 (시작 씬, 렌더 프로필 등)
│  ├─ InputActions.json                # 프로젝트 입력 액션/바인딩
│  └─ EditorWorkspace.json             # 프로젝트별 에디터 창/최근 씬/빌드 프로필
├─ Assets/                             # Project:// (타 프로젝트와 비공유)
│  ├─ Scenes/
│  ├─ Materials/
│  ├─ Models/
│  └─ ...
├─ Saved/ (또는 Temp/, Logs/)          # 하트비트 자동 저장, 크래시 리포트, 프로젝트 로그
└─ .bam/cache/                         # 프로젝트 전용 썸네일/DDC/밉/지형 캐시
```

- **전역 에디터 설정 분리**: 최근 프로젝트 목록(`RecentProjects.json`), 테마/단축키/언어 같은 사용자 전역 `EditorPreferences`는 `%APPDATA%/BamEngine/`에 보관한다.
- **하위 호환(Fallback)**: `--project <path>` 인자나 마지막 프로젝트가 없으면 기존처럼 `<EngineRoot>`의 `Config/Project.json`을 기본 프로젝트로 연다.

### 2.2 `.bamproject` 파일 스키마 (초안)

```json
{
  "SchemaVersion": 1,
  "ProjectName": "MyGameA",
  "EngineVersion": "0.1.0",
  "AssetDirectory": "Assets",
  "ContentMounts": [
    {
      "Name": "SharedKit",
      "Path": "D:/AssetLibrary/Kit01",
      "ReadOnly": true,
      "ShowUnderAssets": false
    },
    {
      "Name": "TeamAudio",
      "Path": "../CommonAudio",
      "ReadOnly": true,
      "ShowUnderAssets": true
    }
  ],
  "Modules": []
}
```

- `Path`는 프로젝트 루트 기준 상대 경로와 절대 경로를 모두 허용한다.
- `Modules`는 2단계(프로젝트별 C++ 모듈 분리)에서 사용할 확장 슬롯으로 예약한다.

---

## 3. 가상 경로 확장 및 외부 폴더 마운트 (`Mount://`)

### 3.1 OS 심볼릭 링크 대신 엔진 가상 마운트를 채택하는 이유

- 현재 `ClientPackage.cpp`와 파일 감시자는 `FILE_ATTRIBUTE_REPARSE_POINT`를 무한 재귀 및 보안 위험으로 의도적으로 차단하고 있다.
- OS 심볼릭 링크는 PC 이동·Git 협업 시 깨지며, 실수로 삭제 시 외부 원본 폴더가 직접 훼손된다.
- 반면 `PlatformPath`에는 이미 `Plugin://<pluginId>/...` 가상 마운트 패턴이 구현되어 있으므로, 동일한 메커니즘으로 `Mount://<Alias>/...`를 추가하면 아키텍처 일관성과 원본 보호를 동시에 달성할 수 있다.

### 3.2 `PlatformPath` API 확장

- **`SetEngineRoot` / `GetEngineRoot`**: 기존 `GetProjectRoot()`에서 엔진 설치/저장소 루트를 명시적으로 분리한다.
  - `Engine://` → `<EngineRoot>/Engine/Resources`
  - `Editor://` → `<EngineRoot>/Editor/Resources`
  - `Project://` → `<ProjectRoot>/<AssetDirectory>`
- **외부 마운트 API**:
  - `RegisterContentMount(const ContentMountDesc& desc)` (`Name`, `RootPath`, `ReadOnly`)
  - `UnregisterContentMount(std::string_view name)` / `ClearContentMounts()`
  - `IsMountUri(std::string_view uri)` / `ResolveMountUri(std::string_view uri)`
  - `ToVirtualUri(physicalPath)`: `Project://` → `Engine://` → `Plugin://` → `Mount://<Name>/` 순으로 정규화한다.
  - `IsReadOnlyTarget(pathOrUri)`: `ReadOnly == true`인 `Mount://` 대상의 삭제·이동·이름 변경을 차단한다.
- **절대 경로 오염 금지**: 외부 마운트 폴더의 에셋을 씬·머티리얼·타임라인에 연결할 때는 반드시 `Mount://<Name>/...` URI로만 직렬화한다.

### 3.3 콘텐츠 브라우저 UI 연동

- 좌측 트리에 `Project Content (Assets/)`, `Engine Content`, **`External Mounts`** 섹션을 제공한다.
- `ShowUnderAssets: true`로 설정된 마운트는 `Assets/` 하위 가상 노드(별도 링크 아이콘·뱃지 표시)로도 탐색할 수 있게 UI 표시 레이어에서 매핑한다.
- 우클릭/상단 메뉴에 `[외부 폴더 연결…]`, `[마운트 설정(읽기 전용 토글)]`, `[연결 해제]`를 제공한다.
- `ResourcePicker`와 드래그 앤 드롭은 `Project://`와 `Mount://` 에셋을 동일하게 선택·참조할 수 있다.

---

## 4. 인-프로세스 프로젝트 전환(Hot-Switch) 아키텍처

에디터를 재실행하지 않고 활성 프로젝트만 교체하려면 **모든 프로젝트 종속 상태와 백그라운드 작업의 수명주기를 결정적 순서로 정리**해야 한다.

### 4.1 8단계 전환 파이프라인 (`ProjectSessionManager::SwitchProject`)

1. **사전 검증 및 사용자 확인 (취소 가능 구간)**
   - 대상 `.bamproject` 유효성(존재, 스키마, `AssetDirectory` 접근 가능) 검증.
   - PIE 실행 중이면 중지 확인 또는 자동 정지.
   - 미저장 씬 및 모든 리소스 편집기(Material, Graph, Animator, Timeline, VFX 등)의 `RunWithDiscardConfirmation` 실행. 사용자가 취소하면 전환 즉시 중단.
2. **백그라운드 작업 및 감시자 정지 (Quiesce)**
   - `AssetHotReloadManager`, `ContentBrowserFileListener`, `ShaderFileListener` 등 프로젝트 경로를 감시하는 모든 `efsw` 리스너 중지.
   - `AsyncResourceLoader`, `ResourceStreamer`, `AsyncResourceOpenService`, `AssetWorkerHost`의 대기 큐 취소 및 진행 중 작업 드레인(Drain).
   - `RecoveryManager`의 현재 프로젝트 하트비트 타이머 중지 및 세션 락 정리.
3. **에디터 상태 및 UI 패널 정리**
   - 열린 리소스 편집기 창 닫기, `SceneEditHistory`(Undo/Redo) 초기화, `EntityClipboard`·선택 상태(`SelectionManager`) 초기화.
   - `AssetCache` 썸네일 및 `ContentBrowserPanel` 캐시 디렉터리 트리 초기화.
4. **씬 및 프로젝트 리소스 언로드**
   - 활성 씬 언로드 및 GPU 커맨드 큐 플러시(`WaitForGPUIdle`).
   - `ResourceManager`에서 **프로젝트/마운트 스코프 리소스(`Project://`, `Mount://`)만 해제**하고, `Engine://`·`Editor://` 빌트인 리소스(기본 셰이더, 폰트, 더미 텍스처)는 유지하여 전환 속도를 확보한다.
5. **경로 및 마운트 재바인딩**
   - `PlatformPath::SetProjectRoot(newProjectRoot)` 및 `SetProjectAssetRoot(newAssetRoot)`.
   - `PlatformPath::ClearContentMounts()` 후 새 `.bamproject`의 `ContentMounts` 등록.
   - 디스크 캐시 루트(`.bam/cache/`) 및 `Temp/`·`Logs/` 경로를 새 `ProjectRoot` 기준으로 재설정.
6. **프로젝트 설정 및 워크스페이스 로드**
   - `EditorSettingsService`가 새 `Config/Project.json`, `Config/InputActions.json`, `Config/EditorWorkspace.json` 로드.
   - 프로젝트 로캘 카탈로그(`Assets/Data/Localization/`) 재로드.
7. **감시자·서비스 재시작 및 시작 씬 로드**
   - `AssetHotReloadManager` 및 콘텐츠 브라우저 파일 감시자를 새 `Assets/` 및 쓰기 가능 마운트 경로에 재등록.
   - `RecoveryManager`를 새 프로젝트 `Temp/` 기준으로 재시작(비정상 종료 스냅샷 존재 시 복원 모달 표시).
   - `Project.json`의 `StartupScenePath`(또는 워크스페이스의 마지막 씬) 로드.
8. **전환 완료 및 최근 목록 갱신**
   - 윈도우 타이틀을 `BamEngine Editor - <ProjectName>`으로 갱신하고 전역 `RecentProjects.json` 업데이트.

### 4.2 실패 처리 및 안전 경계

- **1단계(검증/확인) 실패**: 기존 프로젝트 상태를 전혀 건드리지 않고 종료.
- **5~7단계(로드 중) 실패**: 부분 로드된 상태를 정리하고 빈 씬(Safe Fallback) 상태로 전환한 뒤 오류 모달과 최근 프로젝트 허브를 표시한다. 이전 프로젝트로 되돌리는 과정에서 반파 상태가 남는 것을 방지한다.
- **리소스 스코프 구분 필수**: 4단계에서 `ResourceManager`가 `Engine://` 리소스를 실수로 해제하면 UI 폰트/기본 머티리얼이 깨지므로, 리소스 소유 스코프(`Engine` / `Editor` / `Project` / `Mount`) 판별을 명확히 한다.

---

## 5. 의존성 기반 `.bampak` 패키징 연동

방금 구현된 의존성 기반 패키징(`ClientPackageRequest::OnlyReferencedAssets = true`)과 외부 마운트는 다음과 같이 연결한다.

1. **`AssetReferenceValidator` 확장**:
   - `Mount://<Name>/...`를 `IsMountedReference`의 정상 런타임 루트로 인정하고, 씬/머티리얼/모델 등에서 참조하는 외부 마운트 에셋을 재귀 수집한다.
2. **`ClientPackage` 스테이징 및 패킹**:
   - 외부 마운트 폴더 전체를 복사하지 않고, **`ValidateBuildScene`이 수집한 실제 참조 파일 + `AdditionalAssets`에 명시된 `Mount://` 파일만** 스테이징한다.
   - 패킹 시 `Content/Project.bampak` 내부에 `Mount://<Name>/<relative>` URI로 기록하거나 `PackageManifest.json`에 마운트 스킴을 등록한다.
   - 배포된 독립 `Client`는 원본 외부 폴더가 없는 환경에서도 `.bampak`에서 `Mount://<Name>/...`를 그대로 읽어 실행할 수 있다.

---

## 6. 2단계 설계: 프로젝트별 독립 C++ 게임 모듈(`GameModule`) 분리

1단계에서 에셋·설정·캐시·외부 마운트가 완전히 분리된 뒤, 2단계에서는 게임플레이 C++ 코드도 프로젝트별로 독립시킨다.

### 6.1 목표 구조

```text
<UserProjects>/MyGameA/
├─ MyGameA.bamproject                  # "Modules": [{ "Name": "MyGameAModule", "Type": "Runtime" }]
├─ Source/
│  ├─ CMakeLists.txt                   # find_package(BamEngine) 또는 EngineRoot 상대 참조
│  └─ MyGameAModule/
│     ├─ Public/
│     └─ Private/
└─ Binaries/Win64/
   └─ MyGameAModule.dll                # 프로젝트별 핫리로드 대상 DLL
```

### 6.2 핵심 설계 포인트

1. **엔진 SDK / CMake 인터페이스 분리**:
   - 프로젝트 `Source/CMakeLists.txt`가 `<EngineRoot>`의 `Engine` 타깃(헤더·임포트 라이브러리·`ReflectionGenerator`)을 외부 소비자로 링크하도록 `BamEngineConfig.cmake`를 제공한다.
2. **인-프로세스 전환 시 모듈 언로드/로드 연동**:
   - `ProjectSessionManager::SwitchProject`의 4단계에서 기존 프로젝트의 `ModuleHotReloadManager`가 로드한 게임 모듈 DLL을 `Unregister` → `FreeLibrary`로 언로드하고, 7단계에서 새 프로젝트의 `Binaries/Win64/<Module>.dll`을 로드한다.
   - 씬 언로드 및 `GraphSystem`/`ReflectionRegistry` 정리 **이후**에 DLL을 언로드해야 댕글링 함수 포인터 크래시를 막을 수 있다.
3. **빌드 및 패키징 연동**:
   - `ClientPackage.cpp`의 고정 `RuntimeBinaries` 목록(`GameModule.dll`)을 `.bamproject`의 `Modules` 목록 기반으로 동적 수집하도록 전환한다.

---

## 7. 단계별 구현 로드맵 및 검증 기준

| 단계 | 범위 | 주요 산출물 | 완료 기준 (검증) | 상태 |
|---|---|---|---|---|
| **Phase 1** | `PlatformPath` 엔진/프로젝트 루트 분리 + `.bamproject` 로더 + `Mount://` 해석 | `PlatformPath`, `ProjectDescriptor`, `VirtualPathCheck` | `Engine://`, `Project://`, `Mount://` 해석·역변환·`ReadOnly` 가드, `.bamproject` I/O 단위 테스트 통과 | **완료** |
| **Phase 2** | `ProjectSessionManager` 인-프로세스 프로젝트 전환 + 리소스 스코프 언로드 | `ProjectSessionManager`, `ResourceManager` 프로젝트 스코프 정리, `EditorSettingsService` | A → B → A 반복 전환 8단계 파이프라인, 리소스 스코프 분리 해제, `%APPDATA%` 최근 프로젝트 저장 | **완료** |
| **Phase 3** | 프로젝트 허브(생성/열기/템플릿) + 콘텐츠 브라우저 외부 폴더 연결 UI | `ProjectHubModal`, `ContentBrowserPanel` 외부 마운트 트리·메뉴, 로캘 CSV | 새 프로젝트 생성 후 빈 `Assets/` 격리 확인, 외부 폴더 연결·참조·읽기 전용 차단 확인 | **완료** |
| **Phase 4** | `AssetReferenceValidator` & `ClientPackage`의 `Mount://` 의존성 패키징 연동 | `PakFormat.h`, `AssetReferenceValidator`, `ClientPackage` | `Mount://` 가상 URI 팩 지원, 참조 마운트 에셋 선택적 스테이징 및 `Project.bampak` 패킹 검증 | **완료** |
| **Phase 5 (2단계)** | 프로젝트별 독립 C++ 모듈(`Source/`, `Binaries/`) 빌드·핫리로드·전환 연동 | `ProjectDescriptor` 모듈 경로 해석, `BamEngineConfig.cmake`, `ModuleHotReloadManager`/`ModuleBuildManager`/`HotReloadManager` 프로젝트 전환 연동, `ClientPackage`/`Client` 동적 모듈 수집·로드 | 프로젝트 A/B가 서로 다른 C++ 컴포넌트/노드를 가진 DLL을 전환 로드·패키징 성공 | **완료** |
