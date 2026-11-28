# 플러그인 개발

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

**파일 확인 / 기존 지침 기반.** PluginManagerPanel, EditorPanelPluginApi, ResourcePanelPluginApi 등 확장 관련 파일이 확인된다. 플러그인 생성, 빌드, 승인, 패키징과 런타임 등록의 구체적 절차는 아직 확인하지 않았다.

## 확장 경계

게임 규칙은 [C++ 프로젝트 개발](CppDevelopment.md)의 프로젝트 모듈에 우선 배치한다. 에디터 전용 패널과 리소스 편집기는 Client 런타임과 분리한다. 런타임 플러그인은 Engine 공개 API에 의존하도록 설계한다.

## 추가 조사 및 검증

- PluginManager의 실제 등록·활성화 절차와 플러그인 descriptor 규격.
- 패널 생성 예제, DLL 빌드 설정, 엔진 ABI 호환성.
- 등록 해제 및 핫리로드 시 콜백·리소스 수명.
- Editor와 Client 패키지 각각의 플러그인 포함 규칙.

위 항목은 검증 과제이며 구현 완료 또는 사용 가능한 메뉴로 해석하지 않는다.

## 관련 문서 및 소스

- [EngineEditorClientGuidelines.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/EngineEditorClientGuidelines.md)
- [전체 근거 및 검토 범위](Sources.md)

