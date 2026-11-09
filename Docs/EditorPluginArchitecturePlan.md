# 에디터 플러그인 아키텍처 설계

- 작성일: 2026-11-10
- 상태: 개정 설계안. 네 가지 생성 템플릿과 공용 Window/Panel 계약을 기준으로 진행한다.
- 대상: Windows x64 네이티브 Editor·Runtime 확장과 코드 없는 콘텐츠 패키지. Builtin과 Custom을 같은 발견·활성화 경로에서 다룬다.
- 관련 문서: [엔진·에디터·클라이언트 기준](EngineEditorClientGuidelines.md), [소스 구성 기준](SourceOrganizationGuidelines.md), [에디터 UI 계획](ImGuiDevelopmentPlan.md), [개발 우선순위](DevelopmentPriorities.md).

## 1. 현재 구현과 전환 목표

- CMake의 `bam_add_resource_panel_plugin()`이 `EditorResourcePlugins.txt`를 생성한다. `ImGuiManager::CreateResourceEditors()`가 목록을 읽어 `PluginResourceEditor`를 만들고, `HotReloadManager`는 해당 패널 목록으로 빌더를 등록한다. 새 DLL의 발견, 빌드 및 등록이 리소스 편집기 타입에 연결돼 있다. [CMake](../Editor/Plugins/CMakeLists.txt) · [패널 생성](../Editor/Source/ImGui/Private/ImGuiManager.cpp) · [빌더 등록](../Editor/Source/HotReload/Private/HotReloadManager.cpp)
- Sprite DLL은 에셋 열기·저장과 편집 세션을 소유한다. 현재 호스트는 새 DLL에서 같은 스냅샷 계약을 확인하고, 새 세션 복원 후 이전 세션을 해제한다. 일반 진단 샘플은 같은 섀도 복사 로더에 별도의 진입점을 연결한다. [Sprite 구현](../Editor/Plugins/SpriteAnimationTools/Private/SpriteAnimationTools.cpp) · [리소스 패널 호스트](../Editor/Source/ImGui/ResourceEditor/Private/PluginResourceEditor.cpp) · [진단 샘플](../Editor/Source/ImGui/PanelLifecycleProbe/Private/PanelLifecycleProbe.cpp)
- 전환 후에는 공용 호스트가 **플러그인 발견 → 모듈 로드 → 기능 등록**을 처리한다. 일반 패널과 리소스 편집기부터 이 경로를 공유한다. 메뉴·단축키 액션, 속성 편집기, 임포터는 각각 실제 사용처에 맞는 계약을 추가한다.
- 위 내용은 소스와 문서의 정적 대조 결과다. 이번 설계 작성에서 Editor 빌드, UI 실행 및 성능 계측은 수행하지 않았다.

## 2. 플러그인, 모듈, 등록 기능

- **플러그인**은 관리자에 표시되고 설치·활성화되는 단위다. 안정적인 플러그인 ID, 버전, 호환 범위와 DLL 위치를 선언한다.
- **모듈**은 빌드와 코드 교체의 단위다. 패키지 형식은 모듈 목록을 표현한다. 첫 구현은 모듈이 없는 콘텐츠 패키지와 네이티브 DLL 하나가 있는 패키지를 검증한다. DLL 하나는 여러 창과 액션을 등록할 수 있다. 한 패키지에서 Editor·Runtime DLL을 함께 로드하는 기능은 각 환경의 로딩 경로를 검증한 뒤 추가한다.
- **등록 기능**은 Editor가 사용할 구체적인 동작이다. 패널, 액션, 속성 편집기, 임포터처럼 호출 시점과 소유권이 다른 기능을 각각 등록한다. 리소스 편집은 일반 패널에 붙이는 선택 기능이다.
- **Builtin Plugin**은 에디터 설치본이 제공하는 확장이다. **Custom Plugin**은 사용자가 명시적으로 설치하거나 연결한 확장이다. 두 출처는 같은 패키지 형식과 기능 등록 계약을 사용한다. 네이티브 모듈은 공용 SDK를 사용한다. 에디터의 기본 도킹·문서·입력 기반은 본체가 소유한다. 관리자에 표시되는 GameModule, 셰이더 재컴파일, 에셋 변경 감시는 각 서비스의 기존 수명 주기를 사용한다. [게임 모듈 로더](../Editor/Source/HotReload/Private/ModuleHotReloadManager.cpp)

## 3. 발견, 활성화와 빌드

### 패키지 메타데이터

모든 패키지에 `plugin.json`을 둔다. 패키지는 네이티브 모듈, 콘텐츠 또는 둘을 함께 제공할 수 있다. 관리자는 코드를 실행하기 전에 메타데이터와 로드 가능 여부를 표시한다. 아래 두 예시는 같은 형식을 사용한다.

```json
{
  "manifestVersion": 1,
  "id": "bam.sprite-animation",
  "name": "Sprite Animation Tools",
  "version": "1.0.0",
  "modules": [
    {
      "id": "sprite-animation-editor",
      "environment": "Editor",
      "apiVersion": 1,
      "sdkCompatibility": "<SDK가 제공한 호환 식별자>",
      "binary": "Binaries/Win64/EditorSpriteAnimationTools.dll"
    }
  ]
}
```

```json
{
  "manifestVersion": 1,
  "id": "sample.desert-encounters",
  "name": "Desert Encounters",
  "version": "1.0.0",
  "modules": [],
  "contentRoot": "Content"
}
```

- `manifestVersion`은 패키지 파일 형식, `version`은 배포 버전이다. 모듈의 `environment`는 `Editor` 또는 `Runtime`이고, `apiVersion`과 `sdkCompatibility`는 해당 환경에서 로드할 계약을 확인한다. 네이티브 Builtin과 Custom에는 같은 SDK 호환 기준을 적용한다.
- `binary`, `contentRoot` 및 선택적인 모듈별 실행 의존 DLL 경로는 패키지 기준 상대 경로다. 심볼릭 링크와 정션을 해석한 실제 경로가 패키지 밖이면 거부한다. 로더는 확인된 절대 경로로만 DLL을 연다.
- 출처와 신뢰 여부는 호스트가 정한다. 패키지 파일의 값으로 Builtin 권한을 선언할 수 없다. 기본 활성화 정책도 설치본 목록이나 사용자 설정에 둔다.
- 의존 모듈이 아직 로드되지 않아도 목록에서 상태를 표시할 수 있다. 첫 버전은 SDK 의존만 다룬다. 플러그인 간 의존성이 필요해지면 버전 범위, 활성화 순서, 순환 검사를 같은 패키지 형식의 후속 버전에 추가한다.

### Builtin과 Custom 정책

- **Builtin**은 설치본이 지정한 플러그인 위치에서 발견한다. 설치본 목록이 기본 활성화 여부를 정하고, 사용자는 선택형 도구를 관리자에서 비활성화할 수 있다.
- **Custom**은 프로젝트에서 사용자가 추가한 플러그인 위치 또는 명시적으로 등록한 외부 위치에서 발견한다. 발견 직후에는 목록과 호환 상태만 표시한다. 사용자가 해당 패키지를 승인하고 활성화해야 네이티브 코드가 실행된다. 승인 범위는 프로젝트, 설치 출처와 바이너리 버전이다. 로컬 개발용 소스를 승인한 경우, 연결된 타깃의 빌드 결과를 반복 교체할 수 있다.
- 동일한 ID의 Custom이 Builtin을 덮어쓸 수 없다. Custom끼리 ID가 겹치면 충돌 상태로 표시하고, 사용자가 출처를 하나 선택하기 전에는 둘 다 실행하지 않는다. 활성화 후 기능 ID 충돌이 발견되면 신규 등록 전체를 거부한다.
- Custom DLL은 Editor 프로세스에서 실행된다. 신뢰 설정은 로드 여부를 제어하며, DLL 실행 중의 접근 권한은 Editor 프로세스와 같다.
- 프로젝트별 활성화 설정과 설치본의 기본값은 별개로 보관한다. 로드된 모듈 세대, 빌드 진행 및 인스턴스 포인터는 런타임 상태로 관리한다.

### 새 플러그인 생성 템플릿

생성 화면은 아래 네 가지를 제공한다. 템플릿은 처음 생성할 파일과 예제 동작을 정한다. 생성 후 모듈과 등록 기능의 조합은 개발자가 결정한다.

1. **Blank** — Editor 또는 Runtime 모듈을 선택한다. 공용 진입점과 빈 등록 또는 초기화 함수를 생성한다. 창 없이 실행하는 액션이나 `GameplayScore` 같은 런타임 서비스의 출발점이다.
2. **Editor Standalone Window** — 메뉴에 나타나는 창과 공용 UI로 그리는 예제 버튼을 생성한다. 같은 창을 독립 창과 도킹 패널 사이에서 옮길 수 있다. 에셋 편집이 필요하면 이 창에 열기·저장 기능을 추가한다.
3. **Third Party Library** — 외부 라이브러리를 호출하는 래퍼 모듈을 생성한다. 헤더와 링크 라이브러리는 개발용 빌드 설정으로 관리하고, 실행에 필요한 외부 DLL은 패키지의 실행 의존성으로 선언한다. 정적 라이브러리를 사용하는 경우에는 링크 입력만 필요하다.
4. **Game Feature (Content Only)** — 모듈이 비어 있는 `plugin.json`과 콘텐츠 폴더를 생성한다. 관리자에서 활성화하면 등록된 에셋을 프로젝트에서 찾고 사용할 수 있다. 첫 범위는 콘텐츠 연결과 해제까지다. 자동 컴포넌트 주입이나 실행 중 게임 규칙 전환은 런타임 기능이 실제로 필요해질 때 설계한다.

### 소스가 있는 개발 환경

- 생성 화면에서 이름과 템플릿을 선택하면 Editor가 패키지 파일과 시작용 소스를 만든다. SDK의 공용 CMake 틀이 타깃·출력·PDB 설정을 준비한다. 플러그인마다 독립 타깃이 생기며, 기본 작성 흐름에서는 CMake 파일을 직접 작성할 필요가 없다. 코드 없는 패키지는 빌드 구성도 만들지 않는다.
- CMake 타깃, 감시 폴더, 빌드 트리 경로는 **개발용 목록**에만 둔다. Builtin 소스는 저장소의 빌드 트리에서, Custom 소스는 공용 SDK를 참조하는 별도 빌드 트리에서 구성한다. 설치형 Editor의 소스 빌드에는 호환 SDK 헤더·링크 라이브러리와 C++ 툴체인이 필요하다. 배포용 바이너리 패키지는 소스 경로 없이 설치할 수 있다.
- 승인된 소스 플러그인에는 **빌드 후 다시 로드**와 **소스 저장 시 자동 빌드**를 제공한다. 바이너리 패키지는 설치·활성화·비활성화 및 호환되는 새 바이너리 로드 경로를 사용한다. 같은 관리자에서 출처와 빌드 가능 여부를 표시한다.
- DLL의 패널 코드만 바꾸면 해당 타깃만 빌드한다. SDK 계약이나 호스트 서비스 변경은 호스트와 관련 DLL을 함께 빌드한다. 플러그인 폴더 밖의 기존 CMake·호스트 소스를 개발용 목록에서 임의로 지정해 실행하지 않는다. 빌드 버튼은 로컬 빌드 구성에서 생성한 타깃 정보만 사용한다.

## 4. 개발자가 사용하는 SDK

DLL은 버전이 있는 공용 진입점 `BamGetEditorPluginV1`을 제공한다. SDK는 고정 크기 값, 크기·버전이 있는 구조체, 함수 포인터와 불투명 핸들을 사용한다. C++ 작성용 래퍼는 구현에 필요한 호출을 줄이고, STL 객체나 Editor 내부 클래스의 소유권은 모듈 경계를 넘기지 않는다. ImGui 등 엔진과 공유하는 타입은 호환 식별자가 일치하는 내부 SDK 범위에서 사용한다.

Window와 Panel은 동일한 `RegisterPanel()` 계약을 사용한다. 작성자는 **고정 ID, 제목 로캘 키, 메뉴 위치, Draw 콜백**을 전달한다. Editor는 독립 창과 도킹 상태, 포커스와 Workspace 복원을 관리한다. 업데이트와 세션은 필요할 때 선언한다. 콘텐츠 브라우저와 연결할 때 파일 열기 기능을 추가하고, 편집 가능한 도구는 미저장 확인과 저장을 함께 추가한다. 기존 Sprite는 창 ID, `SpriteAnimation.Save`와 Workspace 키를 유지하거나 명시적인 이전 규칙을 적용한다.

```text
RegisterPanel(진단 패널)
RegisterPanel(Sprite 패널 + 리소스 편집 기능)
RegisterAction(창 없이 실행하는 명령)
```

### 기능별 계약과 적용 순서

- **일반 패널** — 공용 호스트가 창, 도킹과 포커스를 관리한다. DLL은 UI와 필요 시 업데이트를 제공한다. 최초 검증 대상은 프레임 진단 샘플이다.
- **패널의 리소스 편집 기능** — 지원 형식과 열기 콜백을 등록한다. 쓰기를 지원하면 저장 가능 여부, 미저장 확인과 저장도 함께 제공한다. 에셋 전환과 닫기는 호스트의 기존 문서 흐름으로 연결한다. 최초 대상은 Sprite Animation이다. [ResourceEditorInterface](../Editor/Source/ImGui/ResourceEditor/Public/ResourceEditorInterface.h)
- **메뉴·단축키 액션** — 고정 액션 ID, 로캘 키, 문맥, 실행 조건과 함수를 등록한다. 호스트는 기존 `ShortcutManager`에서 충돌 검사와 사용자 바인딩을 처리한다. 다음 확장 후보로 창 없는 진단 명령을 사용한다. [ShortcutManager](../Editor/Source/Shortcut/Private/ShortcutManager.cpp)
- **인스펙터 속성 편집기** — 지원하는 리플렉션 타입과 그리기 콜백을 등록한다. 편집 결과는 호스트의 편집·Undo 경로에 전달한다. DLL 함수 포인터가 Undo 이력에 남는 설계는 언로드를 제한하므로 값과 호스트 명령으로 기록한다. 기본 그리기와 동작이 다른 구체 사례를 고른 뒤 API를 확정한다. [속성 그리기 진입점](../Editor/Source/ImGui/InspectorPanel/Public/ReflectionPropertyDrawer.h)
- **에셋 임포터** — 지원 형식과 변환 작업을 등록한다. 출력 검증과 게시 정책은 기존 임포트 경로에 연결한다. 실행 중인 작업은 DLL 세대를 참조한다. 해당 작업이 종료되거나 취소가 완료될 때까지 세대를 해제하지 않는다. [IImporter](../Editor/Source/AssetManager/Importer/Public/ImporterInterface.h)
- **그래프 노드** — Editor에만 필요한 검색·표현 정보와 실행 시 필요한 노드 동작을 구분한다. 게임에서 실행되는 노드 함수는 런타임 확장 계약에서 제공한다. 에디터 플러그인 관리자에서 노드 실행 DLL을 임의로 교체하는 기능은 별도 설계로 다룬다. [GraphRegistry](../Engine/Source/Graph/Public/GraphRegistry.h) · [그래프 실행 기준](EngineEditorClientGuidelines.md#8-visual-scripting)

## 5. 수명과 교체

- **공용 호스트**는 메타데이터, 모듈 핸들, 빌드 요청·결과와 등록 소유권을 관리한다. `ImGuiManager`와 플러그인 관리 화면은 이 목록을 소비한다. 패널·액션·임포터 호출자는 안정적인 등록 ID를 사용한다.
- **등록 준비**는 후보 세대 전용 목록에서 진행한다. 새 DLL의 ID, 호환 식별자, API 버전, 필수 콜백과 ID 충돌을 검사한다. 준비 중에는 활성 목록이나 편집 중인 에셋에 쓰기 부작용을 일으키지 않는다.
- **교체**는 소스 감지 → 대상 타깃 빌드 → DLL·PDB 복사 → 새 DLL 검증 → 모든 활성 세션 복원 → 메인 스레드에서 연결 교체 순으로 진행한다. 복사와 컴파일은 프레임 밖의 작업으로 처리한다. 같은 타깃을 빌드하는 도중 다시 저장하면 후속 요청을 유지하고, 빌드가 끝난 산출물만 교체 후보로 사용한다.
- **세션 복원**은 호스트가 임시 보관한 버전 있는 값으로 수행한다. Sprite는 엔진 리소스 핸들과 미저장 편집 상태를 새 세션에 넘긴다. 새 세션이 엔진 핸들을 확보할 때까지 이전 세션을 유지한다. 여러 기능을 제공하는 DLL은 모든 복원이 성공했을 때 한 번에 연결을 교체한다.
- **복구**는 빌드, 복사, 진입점 검증, 세션 생성 실패를 구분해 표시한다. 후보만 정리하고 기존 세대와 미저장 상태를 유지한다. 첫 버전에서는 활성 기능 ID, 저장 액션 ID, 상태 계약이 호환되지 않으면 재시작이 필요한 상태를 표시한다.
- **비활성화**는 미저장 편집을 확인한 뒤 새 호출을 차단하고, 등록·콜백·세션을 정리한다. 이전 DLL을 쓰는 호출이나 작업이 끝난 뒤 해당 세대를 해제한다. DLL에서 생성한 객체는 같은 DLL의 파괴 함수로 정리한다.

### 외부 라이브러리와 콘텐츠 패키지

- **Third Party Library** — 래퍼와 외부 실행 DLL을 같은 후보 세대에 준비한다. 승인된 패키지 안에서 해석한 절대 경로와 제한된 DLL 탐색 경로를 사용한다. 헤더·라이브러리의 아키텍처, ABI와 빌드 구성이 맞는지 확인하고, 실행 의존성이 빠져 있으면 후보 활성화를 취소한다. 첫 버전의 핫리로드는 래퍼 코드 변경만 지원한다. 외부 라이브러리의 교체와 전역 상태 정리는 라이브러리별 조건을 확인한 뒤 지원한다.
- **Content Only** — 활성화가 승인되면 콘텐츠 루트를 에셋 목록에 연결한다. 파일을 프로젝트 `Assets`에 덮어쓰지 않으며, 저장된 참조는 플러그인 ID와 루트 상대 경로로 식별한다. `Plugin://<id>/<path>`는 이 기능을 위한 제안이며, 현재 `PlatformPath`의 기존 가상 경로에 추가해야 한다. 콘텐츠 루트나 필수 데이터가 잘못됐으면 활성화를 취소한다. [현재 가상 경로 설계](ResourceAndAssetSeparationPlan.md#3-세부-설계)
- 콘텐츠 전용 패키지에서는 **빌드**, **DLL 다시 로드** 대신 **활성화**, **콘텐츠 새로고침**을 제공한다. 비활성화는 프로젝트의 활성 상태를 먼저 검사한다. 첫 버전은 열린 문서나 실행 중인 세션이 패키지 자원을 사용하는 동안 해제를 보류한다. 프로젝트를 다시 열 때 필요한 패키지가 빠져 있으면 누락된 플러그인 ID와 참조를 표시하고, 기존 참조는 유지한다.

## 6. 구현 순서와 완료 기준

1. **공용 발견과 Window/Panel 전환.** `EditorResourcePlugins.txt`에 묶인 발견과 빌더 등록을 모듈 단위로 옮긴다. 진단과 Sprite를 같은 `RegisterPanel()` 경로에 연결한다. 일반 창은 도킹·분리가 되고, Sprite는 F5 상태에서 미저장 FPS와 도킹을 유지하며 리로드된 뒤 기존 `Ctrl+S`로 저장되어야 한다. 두 창을 한 DLL에서 교체하는 경우와 컴파일·진입점 검증 실패도 확인한다.
2. **Blank와 Editor Standalone Window 생성.** 관리자에서 이름과 템플릿만 골라 생성·빌드하도록 공용 빌드 틀과 시작 코드를 연결한다. 같은 예제를 설치본 Builtin과 프로젝트 Custom 위치에서 각각 등록한다. 미승인 Custom, 중복 ID, 누락 DLL, SDK 불일치에서 활성 상태가 바뀌지 않아야 한다. Custom 작성 시 `Editor/Source`와 엔진의 CMake 목록을 수정할 필요가 없어야 한다.
3. **Blank의 Runtime 선택.** `GameplayScore`를 첫 사용 사례로 삼아 프로젝트가 선택한 런타임 모듈을 시작 시 연결한다. Editor의 PIE와 빌드 출력의 Client에서 함수 호출 결과를 확인하고, 선택 모듈을 끄면 기능이 호출되지 않는지 검증한다. Runtime 적용은 다음 게임 실행부터 처리한다.
4. **Third Party Library 생성.** 외부 정적 또는 동적 라이브러리를 부르는 작은 래퍼로 타깃 생성과 의존 파일 배치를 검증한다. 정상 호출, DLL 누락, 호환되지 않는 바이너리의 로드 실패를 구분해 보여주고, 실패 뒤 이전 모듈이 계속 동작해야 한다.
5. **Game Feature (Content Only) 생성.** 패키지 하나를 빌드 도구 없이 생성하고 활성화한다. 에셋 목록 노출, 저장한 참조의 재개방, 동일 파일 이름을 가진 패키지 간 경로 분리, 비활성화 후 누락 진단을 확인한다. 독립 Client 배포 패키징은 기존 보류 범위를 따른다.

각 템플릿은 생성 파일, 첫 빌드 또는 활성화, 수정 후 적용 절차를 문서화한다. 창이 없는 액션은 Blank의 다음 예제로 검증한다. 속성 편집기와 임포터는 해당 등록 계약과 해제 조건이 필요해진 시점에 추가한다.

## 7. GameModule에서 사용하는 런타임 플러그인

### 목표 구조

- 플러그인 패키지는 에디터 도구, 게임에서 실행되는 기능 또는 둘 다 표현할 수 있다. 모듈마다 실행 환경과 호환 계약을 선언한다. 예를 들어 대화 플러그인은 `DialogueRuntime.dll`로 대화를 실행하고, 같은 패키지의 `DialogueEditor.dll`로 대화 데이터를 편집할 수 있다. 첫 구현은 Blank에서 Editor와 Runtime을 각각 검증한 뒤 두 모듈을 한 패키지에서 함께 사용하는 경로로 확대한다.
- Builtin과 Custom 구분은 설치 출처와 활성화 정책에 적용한다. 게임에 필요한 런타임 모듈은 프로젝트가 명시적으로 선택한다. 게임 배포 목록은 선택한 런타임 모듈, 그 의존성 및 데이터를 포함한다. Editor 모듈과 소스 빌드 정보는 에디터 환경에 둔다.
- 의존 방향은 **GameModule → 런타임 기능의 공개 API → Engine**이다. 에디터 도구는 공개 API나 공용 에셋 형식을 사용한다. Client가 ImGui나 Editor 모듈을 요구하는 참조는 피한다.
- 정식 런타임 호스트는 시작 시 프로젝트의 필수·선택 모듈과 호환 버전을 검사하고, 의존 모듈을 먼저 초기화한다. 그래프나 씬 로딩에 등록 타입이 필요하면 해당 모듈을 먼저 로드한다. 종료는 역순으로 처리한다. 필수 모듈 로드에 실패하면 시작 단계에서 원인을 표시하고, 선택 모듈이 실패하면 연결된 기능을 사용할 수 없음을 표시한다.
- 첫 런타임 계약은 시작과 종료의 수명을 검증한다. 실행 중 교체는 시스템 실행, 등록 타입, 작업 큐, 살아 있는 컴포넌트와 핸들이 정리되거나 이전될 때 지원한다. 현재 `ModuleManager::ReloadModule()`은 이전 DLL을 먼저 언로드하므로, 에디터 플러그인의 후보 검증·실패 복구 절차를 보장하려면 런타임용 교체 설계가 별도로 필요하다. [IModule](../Engine/Source/Core/Module/Public/ModuleInterface.h) · [ModuleManager 구현](../Engine/Source/Core/Module/Private/ModuleManager.cpp)

### 학습 예제 · GameplayScore

현재 예제는 범용 런타임 패키지 발견을 구현하기 전에, **GameModule이 별도 DLL의 서비스를 호출하는 흐름**을 보여준다. `GameModule::OnLoad()`에서 실행 파일 옆의 `GameplayScore.dll`을 기존 `ModuleManager`로 로드하고, 기본 점수 100에 콤보 3을 적용해 결과를 로그에 남긴다. 기존 게임 점수나 씬 데이터는 수정하지 않는다. 예제 모듈이 없어도 GameModule 초기화는 계속된다. [호출부](../Projects/GameModule/Source/GameModule.cpp) · [DLL 구현](../Projects/Plugins/GameplayScore/Private/GameplayScoreModule.cpp)

1. **CMake 타깃** — `add_library(GameplayScore MODULE ...)`은 별도 DLL을 만든다. `add_dependencies(GameModule GameplayScore)`는 빌드 순서만 정한다. GameModule은 공개 헤더를 읽지만 GameplayScore의 import library에 링크하지 않는다.
2. **상속과 가상 함수** — `GameplayScoreModule : public Engine::IModule`은 기존 로더가 이해하는 모듈이다. `IMPLEMENT_MODULE`이 `CreateModule`과 `DestroyModule`을 내보낸다. 기존 로더는 그 함수로 객체를 만들고 `OnLoad()`와 `OnUnload()`를 호출한다. 이 예제에는 별도로 등록할 시스템이나 상태가 없어서 훅 본문이 비어 있다.
3. **내보내기** — `extern "C" __declspec(dllexport)`는 `BamGetGameplayScoreV1`을 Windows 로더가 이름으로 조회할 수 있게 한다. 이 함수는 모듈의 생성 진입점과 별개로, GameModule이 사용할 공개 서비스를 반환한다.
4. **함수 포인터와 API 테이블** — `using CalculateComboScoreFn = std::uint32_t (*)(std::uint32_t, std::uint32_t) noexcept;`는 두 정수를 받아 점수를 반환하는 함수 포인터 타입이다. `GetProcAddress`로 조회한 진입점을 같은 시그니처의 포인터로 해석하고 호출한다. 이어서 `ApiV1`의 크기와 콜백 유무를 확인하고 `api->CalculateComboScore(100, 3)`으로 DLL 안의 함수를 호출한다. 헤더는 인자와 반환 타입의 합의이고, 실제 구현은 DLL에 있다. [공개 API](../Projects/Plugins/GameplayScore/Public/GameplayScoreApi.h)
5. **수명과 소유권** — API 테이블과 함수 주소는 DLL이 로드된 동안 유효하다. 예제는 초기화 중에만 호출하고 주소를 보관하지 않는다. GameModule이 직접 로드한 경우에만 종료 시 해제를 요청하며, 이미 로드돼 있었던 모듈의 수명은 그 소유자가 관리한다. 현재 전체 종료 순서는 의존성 그래프로 정렬되지 않는다. 이 예제는 종료 중 다른 DLL의 함수를 호출하지 않는 상태 없는 서비스로 제한한다.

### 사용자 확인

```powershell
cmake --build out/build/x64-debug --config Debug --target Editor
```

Editor 타깃이 GameModule과 예제 DLL을 함께 빌드한다. 시작 후 로그에서 `Base score 100, combo 3: 300`을 찾는다. 다음 연습으로 호출부의 콤보를 `8`로 바꾸면, 예제의 최대 배율이 5이므로 결과는 `500`이다. Client를 확인할 때는 같은 빌드 트리에서 Client 타깃도 빌드한 뒤 두 DLL이 있는 출력 디렉터리의 실행 파일을 사용한다. 별도 배포 패키징은 기존 보류 범위에 둔다.

이 예제의 빌드, 시작 로그와 종료 로그는 사용자 실행 확인 항목이다. 소스 변경의 자동 감지나 실행 중 런타임 플러그인 교체는 이후 검증 단계에 속한다.

## 8. 현재 진단 패널 전환 상태 (2026-11-10)

Sprite 리소스 편집기는 CMake 카탈로그의 `ResourcePanelPluginApiV1` 경로를 사용한다. 별도 진단 샘플은 `BamGetEditorPluginV1`으로 창의 ID·제목 키·그리기 콜백을 노출한다. 이 실험은 일반 플러그인 발견이나 패키지 활성화 설계와 구분한다. 스냅샷의 진단 DLL 소스는 내보내기 매크로를 사용하며, 해당 타깃에서 매크로 정의를 볼 수 있도록 연결해야 한다. 진단 타깃의 명시 빌드, API 진입점·등록 계약 검증, 로드·언로드 및 Sprite 편집기의 저장·상태 승계는 사용자 실행 확인이 필요하다.
