# 프로젝트 관리

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

**코드 확인:** `ProjectHubModal`, `ProjectSessionManager`, `ProjectDescriptor`.

## 새 프로젝트

1. 시작 시 프로젝트 허브가 표시되면 사용하거나, 에디터의 `File` 메뉴에서 Project Hub를 연다.
2. 프로젝트 이름(예: `Sandbox3D`)과 저장할 대상 폴더를 지정한다.
3. 템플릿을 선택한다. 현재 UI에는 Empty / 3D / 2D가 존재한다.
4. C++로 게임을 개발할 경우 C++ 모듈 생성 체크박스를 켠다.
5. 프로젝트를 생성하고 열린 프로젝트의 이름과 Content Browser의 루트를 확인한다.

**주의:** 현재 `ProjectDescriptor::ScaffoldNewProject`는 `(void)projectTemplate;`로 템플릿 인자를 사용하지 않으며 `kEmptySceneJson`을 Main 씬에 쓴다. 따라서 2D/3D를 선택해도 카메라나 조명 등이 자동으로 구성된다고 기대하지 말고 직접 생성한다.

생성되는 핵심 파일 및 선택적 폴더의 예:

```text
Sandbox3D/
├── Sandbox3D.bamproject
├── Config/
│   └── Project.json
├── Assets/
│   └── Scenes/
│       └── Main.bamscene.json
└── Source/                       # C++ 모듈 옵션을 켠 경우
    ├── CMakeLists.txt
    └── <ModuleName>/
        ├── CMakeLists.txt
        └── <ModuleName>.cpp
```

모듈 빌드 후 `Binaries/Win64/`에 DLL이 출력되는 SDK 설정이 있다. 실제 모듈 이름은 `.bamproject`의 `Modules`를 확인한다. 캐시, 로그, 임시 파일, 사용자 설정 등은 실행 과정에서 추가될 수 있다.

## 기존 프로젝트 열기 및 전환

- Project Hub의 Open Project로 `.bamproject` 파일 선택 또는 Open Folder로 프로젝트 폴더 선택.
- File 메뉴의 Recent Projects로 기존 작업 공간에 접근.
- 프로젝트를 바꾸기 전에 PIE를 종료하고 씬 및 리소스 편집기의 변경을 저장한다.
- 전환 뒤 창 제목, Content Browser 루트, 시작 씬, 프로젝트 설정과 모듈을 각각 확인한다. 특히 A → B → A를 반복하여 이전 프로젝트 데이터가 남지 않는지 테스트한다.

전환 코드에는 사전 검증, 기존 상태 정리, 경로 재바인딩, 시작 씬 로드 등이 있다. 2026-11-24 설계 문서는 Phase 1~5 구현을 기록했으나 실환경 검증은 대기 상태로 표시한다. 로드 실패 시 빈 씬으로 대체될 수 있으며 항상 이전 프로젝트로 자동 복귀한다는 보장은 없다. legacy `Config/Project.json` 기반 폴더도 전환 코드가 인식한다.

## 가상 경로와 외부 에셋

| URI | 의미 |
|---|---|
| `Project://` | 활성 프로젝트의 Assets 루트(또는 descriptor의 AssetDirectory) |
| `Engine://` | 엔진 공용 Resources |
| `Editor://` | 에디터 Resources |
| `Plugin://<ID>/` | 플러그인 리소스 |
| `Mount://<Alias>/` | 프로젝트에 등록한 외부 콘텐츠 폴더 |

외부 텍스처/모델 라이브러리를 복사 없이 사용하려면 Content Browser의 External Mounts에서 외부 폴더 연결을 선택한다. Alias를 지정하고 가능하면 Read Only를 유지한다. `ShowUnderAssets` 옵션은 Assets 쪽 가상 노드 표시용이다. 참조는 마운트 URI를 사용하고 다른 PC에서 폴더 경로가 달라질 수 있음에 유의한다. 원본 폴더 삭제나 이름 변경 전에 참조 영향을 반드시 확인한다.

## 관련 문서 및 소스

- [MultiProjectArchitecturePlan.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/MultiProjectArchitecturePlan.md)
- [`ProjectHubModal.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/ProjectHub/Private/ProjectHubModal.cpp) — `Editor/Source/ImGui/ProjectHub/Private/ProjectHubModal.cpp`
- [`ProjectSessionManager.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/Project/Private/ProjectSessionManager.cpp) — `Editor/Source/Project/Private/ProjectSessionManager.cpp`
- [`ProjectDescriptor.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Path/Public/ProjectDescriptor.h) — `Engine/Source/Core/Path/Public/ProjectDescriptor.h`
- [`ProjectDescriptor.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Path/Private/ProjectDescriptor.cpp) — `Engine/Source/Core/Path/Private/ProjectDescriptor.cpp`
- [`ContentBrowserPanel.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserPanel.cpp) — `Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserPanel.cpp`
- [전체 근거 및 검토 범위](Sources.md)

