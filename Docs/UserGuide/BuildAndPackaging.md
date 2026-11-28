# PIE, 빌드 및 패키징

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

## PIE

상단 Play Bar의 소스에서 확인한 조작:

| 버튼 | 동작 |
|---|---|
| Play | 편집 상태에서 실행 시작 |
| Pause / Resume | 일시정지 / 재개 |
| Step | 일시정지 상태에서 한 스텝 요청 |
| Stop | PIE 종료 |

권장 순서: 씬 저장 → Play → Game View 입력 테스트 → Pause/Step → Stop → 편집 씬 복원 확인. 실행 중 수정이 편집 씬에 자동 보존된다고 가정하지 않는다.

## Build Profile

`File → Build Profile`에서 패널을 연다. `BuildProfilePanel.cpp`에 Windows Release, Build Client, Build and Export, Check Scene References 등의 경로가 있다.

1. General에서 게임 이름 등의 설정을 확인한다.
2. Scenes 카테고리에서 현재 씬 또는 씬 파일을 추가한다. 시작 씬이 지정되어 있는지 확인한다.
3. Assets 카테고리에서 추가로 포함할 파일/폴더를 지정한다. 런타임 코드에서 문자열로만 로드하는 에셋은 자동 참조 수집에서 누락될 수 있으므로 별도 추가한다.
4. Packaging에서 출력 디렉터리, 실행 이름, 패키지 폴더 이름을 설정한다.
5. 필요 시 BamPak Packaging 및 Only Referenced Assets 옵션을 사용한다.
6. Check Scene References로 누락 경로를 확인한 다음 Build Client 또는 Build and Export를 실행한다.
7. Build Log와 알림에서 성공 여부를 확인한다.
8. 내보낸 폴더를 열고 독립 Client 실행을 검증한다.

마운트 리소스 패킹, 모듈 DLL 포함 등은 기존 멀티 프로젝트 설계에 반영되어 있지만 실제로 외부 마운트 원본이 없는 PC에서 성공하는지 확인해야 한다. Debug PIE 결과만으로 Windows Release 배포가 성공한 것으로 보지 않는다.

## 독립 실행 검사

- 내보낸 폴더를 엔진 저장소 밖으로 복사하고 실행한다.
- 시작 씬, 모델/텍스처/폰트, 입력, 사운드, 프로젝트 모듈, 그래프, 씬 전환을 검사한다.
- 원래 에셋/외부 마운트 경로가 없는 환경에서도 필요한 리소스가 로드되는지 확인한다.
- 프로젝트에 사용한 동적 로드 에셋과 FMOD 등 필요한 런타임 의존성이 포함되어 있는지 확인한다.
- 실패 시 패키지 실행 로그 및 Build Log를 보존한다.

## 관련 문서 및 소스

- [MultiProjectArchitecturePlan.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/MultiProjectArchitecturePlan.md)
- [`ToolBar.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/ToolBar/Private/ToolBar.cpp) — `Editor/Source/ImGui/ToolBar/Private/ToolBar.cpp`
- [`BuildProfilePanel.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/BuildProfilePanel/Private/BuildProfilePanel.cpp) — `Editor/Source/ImGui/BuildProfilePanel/Private/BuildProfilePanel.cpp`
- [전체 근거 및 검토 범위](Sources.md)

