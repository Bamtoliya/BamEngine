# BamEngine 문서

이 문서는 현재 저장소의 코드와 검증 기록을 바탕으로 정리한 사용·개발 문서의 진입점입니다. 기능이 코드에 있다는 사실과 실제 실행 검증을 통과했다는 사실은 구분합니다.

## 사용자 문서

- [시작하기](UserGuide/GettingStarted.md): 개발 환경, 의존성 준비, 에디터 구성·빌드·실행
- [에디터 기본 작업](UserGuide/EditorWorkflow.md): 씬, 엔티티, 에셋 탐색, Undo/Redo

## 개발자 기술문서

- [아키텍처 개요](Technical/Architecture.md): CMake 타깃, Editor/Client 진입점, Runtime, 씬·리소스·Reflection 흐름
- [씬 저장·복원 구조](Technical/ScenePersistence.md): 씬 Archive 데이터, Editor 저장 경계, 복원 검증과 제약

## 기존 설계·검증 문서

- [엔진·에디터·클라이언트 지침](EngineEditorClientGuidelines.md)
- [소스 조직 지침](SourceOrganizationGuidelines.md)
- [Reflection / Archive 빌드](ReflectionArchiveBuild.md)
- [셰이더 바인딩 레이아웃](Shader/BINDING_LAYOUT.md)
- [씬 Undo/Redo 사용 범위와 제약](EditorUndoRedo.md)
- [에디터 UX 검증 기준](EditorUXTestGuidelines.md)
- [Client 빌드·패키징 계획과 잔여 검증](BuildAndPackagingPlan.md)
- [구현 기록](ImplementationLog.md)
- [개발 우선순위](DevelopmentPriorities.md)

## 상태 표기

- **코드 구현**: 현재 소스에서 경로를 확인했다는 뜻이며, UI·GPU·배포 실행까지 확인했다는 뜻은 아닙니다.
- **자동 검증**: 해당 CTest 또는 자동 검사에서 확인한 범위입니다.
- **사용자 실행 확인**: 구현 기록에 사용자 환경의 실제 실행 결과가 남아 있는 범위입니다.
- **미검증 / 계획**: 실행 결과가 없거나 설계·개발 계획에만 있는 범위입니다.

2026-11-20 구현 기록에는 `x64-ninja-debug` 빌드 및 CTest 44/44 통과가 사용자 확인으로 기록되어 있습니다. 이는 그 시점의 기록이며 현재 워크트리의 모든 변경, GPU 렌더링, 전체 UI 동작 또는 독립 배포를 보증하지 않습니다. 각 기능의 최신 상태와 남은 검증은 연결된 계획·구현 기록을 확인하세요.

## English

[English documentation index](README.en.md)
