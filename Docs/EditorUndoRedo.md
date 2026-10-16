# 에디터 씬 Undo/Redo

## 사용법과 지원 범위

Edit 메뉴의 실행 취소·다시 실행을 사용한다. 씬 편집 패널에 포커스가 있을 때
Ctrl+Z는 Undo, Ctrl+Y와 Ctrl+Shift+Z는 Redo다.
텍스트 입력 중에는 ImGui의 텍스트 편집 이력을 사용한다.
드래그 중에는 문서 Undo/Redo 단축키를 실행하지 않는다.

현재 연결한 작업:

- Inspector의 등록 컴포넌트 속성, 리소스 핸들과 컴포넌트 추가.
  Camera 같은 컴포넌트가 함께 추가한 Transform·WorldTransform·Flag도 되돌린다.
- Scene View의 위치·회전·크기 기즈모.
- Hierarchy의 드래그 부모 변경·루트 분리, 씬 이름 변경.
- EntityFactory를 통한 빈 엔티티·Primitive·Sprite·Canvas·Image·Button 생성.
- Hierarchy 엔티티 우클릭 메뉴의 엔티티와 자식 삭제.
- UI View의 이동·크기 조절과 Canvas 디자인 해상도.
- UI Render Order의 Z-Index 편집 및 Order 메뉴의 앞/뒤 배치.

속성 드래그와 기즈모 조작은 시작 값과 마지막 값을 이력 한 건으로 묶는다.
Button이 내부에서 Image를 만드는 것처럼 중첩된 생성 범위도 가장 바깥 작업 한 건으로 기록한다.
선택, 카메라 이동, 패널 배치, UI View의 팬·줌·격자 설정은 씬 이력에 넣지 않는다.
지원하지 않는 일반 컨테이너 요소 편집이 이 기능으로 추가되는 것은 아니다.

## 책임과 연결 방식

CommandHistory는 EditorCommand를 unique_ptr로 소유한다.
새 명령을 기록하면 커서 뒤의 Redo를 폐기하고, 기본 100건을 넘으면 가장 오래된 명령을 해제한다.
Execute·Undo는 성공 여부를 반환한다. 일반 검증 실패에서는 이력 커서를 움직이지 않는다.
중복 실행과 알림 콜백을 통한 재진입도 막는다.
Engine의 기존 Command 인터페이스와는 별개인 Editor 전용 명령이다.

SceneEditHistory는 편집 전후의 SceneEditSnapshot을 명령으로 기록한다.
이미 적용한 UI 변경은 Record로 넣으며, 이때 변경을 다시 실행하지 않는다.
Inspector·기즈모·UI 드래그는 대상 엔티티의 등록 컴포넌트를 보관한다.
생성·삭제·부모 변경·씬 이름 변경은 엔티티 집합과 씬 이름을 보관한다.
이력은 파일 저장 형식이나 씬 로딩을 사용하지 않는 메모리 값 복사다.

ReflectionRegistry의 컴포넌트 등록 경로가 공통 Copy·Remove 연산도 생성한다.
일반적인 복사 가능한 등록 컴포넌트는 Undo용 멤버 목록이나 타입별 명령을 추가하지 않는다.
복사는 C++ 타입의 복사 의미를 따르며 리소스 핸들도 참조를 보유한다.
등록된 타입의 올바른 복사 생성·대입 구현은 필요하다.
Engine에는 범용 ECS 연산만 두고 이력·병합·선택·단축키 정책은 Editor가 소유한다.
Reflection·Archive·ReflectionArchive 독립 모듈에는 Editor 의존성을 추가하지 않았다.

생성·삭제 복원은 영구 ID로 엔티티를 찾으며, 새로 생성된 실행 핸들을 원래 핸들로 가정하지 않는다.
Hierarchy의 양쪽 연결·형제 순서와 현재 등록 컴포넌트의 직접 entt::entity 프로퍼티를
영구 ID로 다시 연결한다. Joint·Joint2D의 ConnectedEntity도 이 경로를 사용한다.
Scene::DestroyEntity는 하위 트리를 삭제하고 외부의 직접 리플렉션 엔티티 참조를 null로 정리한다.
일반 포인터나 임의 컨테이너·중첩 객체 내부의 엔티티 참조를 자동 해석하는 기능은 아니다.
그런 참조를 새로 도입하면 별도의 참조 정책을 먼저 연결해야 한다.

복원 전에 예상 엔티티 집합·컴포넌트 구성·관계·참조와 현재 구조가 일치하는지 확인한다.
기록 밖에서 추가한 엔티티나 컴포넌트를 전체 복원으로 지우지 않도록, 차이가 있으면 거부한다.
대상·스토리지와 최종 계층의 양방향 관계·순환·깊이도 검사한다.
복원 후 Transform 계층을 갱신하고 Camera·Light 캐시와 Collider Dirty 상태를 갱신한다.
Inspector의 임시 텍스트·축 편집 상태는 비운다.
선택은 적용 전에 ID로 보관하고 적용 후 살아 있는 엔티티로 다시 찾는다.
UI의 아직 적용하지 않은 드래그 값은 이력 Revision이 바뀌면 폐기한다.

## 수명·실패·제약

- 스냅샷은 원본 Scene 참조를 보유한다. 이력 폐기·한도 초과·종료 시 값과 참조를 함께 해제한다.
- 씬 전환, 성공한 파일 로딩과 편집·실행 상태 전환은 기록을 비운다.
  동일 씬의 새 수정은 Redo 분기를 폐기한다. 파일 저장은 이력을 비우지 않는다.
- 엔티티 집합을 복원하는 작업은 미등록 컴포넌트나 raw EnTT 엔티티가 있으면
  해당 상태를 조용히 버리지 않고 캡처·복원을 거부한다.
  일부 엔티티의 속성 복원은 그 엔티티의 미등록 컴포넌트를 건드리지 않는다.
- 변경 후 캡처에 실패하면 잘못된 이력을 남기지 않도록 기존 이력을 비우고 Edit 메뉴에 오류를 표시한다.
  Editor 밖에서 엔티티·ID·관계를 직접 수정하는 코드는 기존 이력의 유효성을 보장하지 않는다.
  그런 변경이나 플러그인·타입 등록 교체 전에는 GetEditHistory().Clear()를 호출해야 한다.
- 일반 대상·계층 검사 실패는 변경 전에 반환한다.
  메모리 할당 예외, 사용자 복사·소멸 코드의 부작용과 동시 변경을 포함한 완전한 트랜잭션은 보장하지 않는다.
- 최대 건수는 메모리 바이트 제한이 아니다. 큰 씬의 생성·삭제·부모 변경은 전체 엔티티 값을 보관하므로
  비용 측정 후 필요하면 영향을 받는 엔티티만 보관하는 명령으로 줄인다.
- 그래프 편집, 에셋·파일 작업, 기존 GameObject/Component 편집 경로에는 아직 연결하지 않았다.
  그래프 등 다른 문서 패널의 Ctrl+Z가 씬 이력을 변경하지 않도록 단축키 포커스를 제한한다.
- 선택 자체의 Undo, 다중 엔티티 일괄 조작, 저장 여부/Dirty 표시와 이력 영속화는 제공하지 않는다.
  시작 값으로 돌아온 드래그의 빈 이력 항목을 제거하는 최적화는 아직 없다.
  PIE 시작·복원은 기존 구현 범위이며 이 기능으로 완성되지 않는다.

## 검증

EditorHistoryCheck는 명령 해제·이력 한도·Redo 분기 폐기·실패 시 커서 보존,
드래그 기록 병합, 값·컴포넌트 추가와 자동 의존 컴포넌트 복원,
계층 순서·생성·하위 트리 삭제·씬 이름·실행 핸들 재할당,
Joint 참조·UI 레이아웃·Scene 수명과 미등록 컴포넌트 보존을 검사한다.

Debug·Release Editor 빌드와 관련 CTest 각각 10/10 통과했다.
Button 중첩 생성 보완 후 EditorHistoryCheck도 두 구성에서 각각 1/1 통과했다.
로그는 out/audit/editor-history-20261016에 보관한다.

의존성이 구성된 저장소 빌드 디렉터리에서:

```powershell
cmake --build out/build/engine-reflection-unified --config Debug --target EditorHistoryCheck Editor
ctest --test-dir out/build/engine-reflection-unified -C Debug -R '^EditorHistoryCheck$' --output-on-failure
```

Release는 구성 인자를 Release로 바꾼다.
실제 창에서 숫자를 드래그한 뒤 한 번에 취소되는지, 텍스트 Ctrl+Z와 충돌하지 않는지,
생성·삭제·부모 변경·UI 크기 조절을 반복해 Redo 분기가 정확히 폐기되는지는 수동 확인 항목이다.
GPU 렌더링과 입력 포커스 전체를 이 검사 하나로 검증했다고 해석하지 않는다.
