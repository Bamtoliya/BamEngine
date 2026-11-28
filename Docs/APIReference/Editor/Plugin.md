# Editor Plugin / Host Services

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Experimental / Internal** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 구조
EditorPanelPluginApi 헤더에는 PluginApiV1/V2, PanelRegistrationV1/V2, PanelDrawContextV1/V2, PanelHostServicesV2가 있다. 엔진 IModule과 에디터 panel plugin은 다른 ABI 경계로 취급한다. 생성된 plugin scaffold 및 API 버전에 맞춰 구현한다. 이번에는 전체 struct 멤버와 export 함수 계약을 확보하지 않았으므로 가짜 plugin entry point 예제를 제시하지 않는다.

## Host API (에디터 내부용)
`PluginHost::Initialize(...)`, `Update(gui)`, `RefreshInstalledPackages`, `GetCount`, `GetEntry(index)`, `GetStatus(index)`, `GetPanels(index)`, `Request(index,action)`, `GetApprovalPrompt`, `ResolveApproval(approved)`, `BuildAndReloadPluginSource(pluginId)`, `GetPluginBuildLogs`, `Clear`. `GetEntry`는 at(index)를 사용하므로 인덱스 유효성을 확인한다. 로드된 플러그인 내부가 host 수명을 강제로 초기화하지 않는다.

## 안전과 호환성
approval UI를 우회하지 않는다. 네이티브 플러그인은 프로세스 권한으로 실행되는 코드다. API version, struct size, compiler/CRT/ImGui context, callback 소유권, unload 순서를 검증한다. 비승인/빌드 실패/reload 실패 시 기존 상태 및 오류 출력, project switch 후 mount 해제와 panel 복원을 검증한다. [EditorPluginArchitecturePlan](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/EditorPluginArchitecturePlan.md) 참조. 이 문서는 host interface 중심 초안이며 plugin 작성용 ABI 완전 레퍼런스가 아니다.

## 관련 소스
- [EditorPanelPluginApi.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/HotReload/Public/EditorPanelPluginApi.h) — `Editor/Source/HotReload/Public/EditorPanelPluginApi.h`
- [PluginHost.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/Plugin/Public/PluginHost.h) — `Editor/Source/Plugin/Public/PluginHost.h`
- [ResourcePanelPluginApi.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/HotReload/Public/ResourcePanelPluginApi.h) — `Editor/Source/HotReload/Public/ResourcePanelPluginApi.h`

## 검증 및 참고 예제
- [PanelPluginGeneratorCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/examples/PanelPluginGeneratorCheck.cpp)
