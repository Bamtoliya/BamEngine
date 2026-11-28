# 문제 해결과 검증

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

## 문제 해결

| 증상 | 우선 확인 |
|---|---|
| CMake configure 실패 | vcpkg/Ninja 절대 경로, LLVM/Clang resource directory, Qt 및 SDK |
| FMOD 링크/실행 실패 | Debug/Release 파일 종류와 DLL 배치 |
| 프로젝트가 생성되지만 씬이 비어 있음 | 현재 템플릿 미반영은 코드상 확인된 제약. 카메라 등 수동 생성 |
| 프로젝트 전환 후 이전 에셋이 보임 | 실제 활성 프로젝트와 descriptor, Content Browser 루트, 로그 및 전환 실패 알림 |
| Scene은 보이나 Game View는 검음 | 실제 Camera 엔티티/주 카메라, 렌더러, 조명, 활성 상태, 렌더 타겟 |
| 물체가 떨어지지 않음 | Dynamic BodyType, GravityScale, 컴포넌트 Enable, PIE 상태 |
| 물체가 바닥을 통과함 | Collider 설정, 물리 2D/3D 구분, 크기, 빠른 물체의 CCD |
| 캐릭터 이동 입력이 없음 | Gameplay 맵 및 action ID, 포커스, InputEnabled, UseGraphMoveInput |
| 오토타일이 칠해지지 않음 | 마스크 0 할당, 유효한 Sprite, 선택 Tilemap, Autotile 브러시 |
| 오토타일 모양이 틀림 | +Y/+X/-Y/-X 비트, 일반 타일은 연결되지 않음 |
| UI 버튼이 반응하지 않음 | Canvas 계층, Interact/PIE 모드, raycastTarget, interactable, 겹친 이미지 |
| 프리팹 열기/복원 실패 | 확장자, 파일 내용, 참조 누락, PrefabMode 전환, Console/알림 |
| Graph 노드가 실행되지 않음 | 실행 핀, graphFile, Enabled, 이벤트, 타입 진단 |
| 프로젝트 DLL 로드 실패 | descriptor Modules, 출력 DLL 경로, ABI, 의존성, Build/HotReload 로그 |
| Client에서 에셋 누락 | Build Profile의 시작 씬, 참조 검사, Additional Assets, 마운트/패키지 |

## 기능 검증 체크리스트

- [ ] 최신 커밋의 Ninja Debug configure/build 및 CTest
- [ ] 새 2D/3D 프로젝트 생성 후 시작 씬 수동 구성
- [ ] 프로젝트 A/B 반복 전환, 프로젝트별 Assets·설정·모듈 확인
- [ ] 외부 마운트 읽기 전용 및 참조 경로 검증
- [ ] 씬 저장/재열기, Undo/Redo, 프리팹 복제 및 원본 편집
- [ ] 3D 낙하·충돌 및 캐릭터 이동/카메라
- [ ] 2D 스프라이트, 애니메이션, 16개 오토타일 마스크, 2D 충돌
- [ ] UI 클릭·드롭다운·해상도 변경
- [ ] 로직 그래프, 입력 액션, 프로젝트 C++ 모듈 빌드/리로드
- [ ] 렌더링 효과, 리소스 참조, GPU 비용
- [ ] PIE 반복 시작/종료와 편집 데이터 복원
- [ ] Windows Release 패키징 및 외부 환경 독립 실행

## 현재 사용법 문서에서 확정하지 않은 항목

- 2D/3D 프로젝트 템플릿의 실제 콘텐츠 분기: 현재 미반영 확인.
- 선택 영역 전용 스프라이트 자동 슬라이스: 조사 경로에서 확정하지 못함.
- 다중 선택 작업의 모든 드롭/메뉴 대상 지원, 프리팹 열기 및 UI 드롭다운 최신 수정 상태: 실환경 재검증 필요.
- 고급 애니메이션, 렌더링, 네트워크, RL 등의 완성도 및 정확한 설정 경로: 일부는 파일 목록만 확인함. 별도 시스템별 가이드와 실행 검증 필요.
- 외부 프로젝트별 Debug/Release 모듈 빌드, 리플렉션 연동, Hot Reload 및 `.bampak` 독립 배포: 연결 코드/문서가 있으나 이번 작업에서는 실행하지 않음.

## 관련 문서 및 소스

- [전체 근거 및 검토 범위](Sources.md)

