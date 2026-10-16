# ECS UI Z-Index와 렌더 순서

## 사용법

UI 요소를 선택하고 Inspector의 UIRenderOrderComponent.zIndex를 수정한다.
UI View의 `Order` 메뉴에서도 같은 값을 편집할 수 있다.

- `Z-Index`: 직접 숫자 입력.
- `Z +1 / Z -1`: 값을 한 단계 변경.
- `Bring to Front`: 같은 그룹의 가장 큰 값보다 1 크게 설정.
- `Send to Back`: 같은 그룹의 가장 작은 값보다 1 작게 설정.

새 Canvas, Image와 Button에는 기본값 0인 UIRenderOrderComponent를 추가한다.
이전 씬의 요소에 컴포넌트가 없으면 값 0으로 취급한다.
Inspector에서 UI Render Order 컴포넌트를 추가하거나 Order 메뉴를 사용하면 해당 요소에 저장할 수 있다.
숫자가 클수록 나중에 그려져 앞에 보이고, 클릭 검사도 먼저 받는다.
같은 값이면 기존 Hierarchy의 자식 배열 순서를 유지한다.

Order 메뉴는 편집 모드에서 선택 잠금이 해제되어 있을 때 사용할 수 있다.
Interact를 끄고 순서를 편집한 다음 다시 켜서 버튼의 입력 우선순위를 확인한다.
Inspector 값 편집과 Order 메뉴의 변경은 기존 SceneEditHistory로 Undo/Redo한다.
추가한 컴포넌트가 없던 상태로 Undo하면 다시 기본값 0으로 동작한다.

## 비교 범위와 계층

Z-Index는 같은 부모 아래의 형제 그룹에서 비교한다. 부모는 자신의 자식보다 먼저 그리며,
부모와 자식의 하위 트리는 한 묶음으로 유지한다.

예를 들어 Canvas 아래 A(z=0), B(z=1)가 있고 A의 자식 C(z=100)가 있으면 A → C → B로 그린다.
C의 숫자가 크더라도 A의 묶음에서 벗어나 B를 덮지 않는다. A 전체를 앞으로 보내려면 A의 값을 높인다.
별도 Canvas로 나눈 UI 그룹은 각 Canvas의 값을 조정한다. 중첩 Canvas는 아직 지원하지 않는다.

같은 Scene의 최상위 Canvas끼리도 UIRenderOrderComponent.zIndex를 비교한다.
Canvas 값이 같으면 영구 ID 순서가 기준이다. 필터 없는 UI 패스에서 Canvas 전체 묶음의 순서를 바꾼다.
UI View는 지정한 Canvas 하나를 미리보기하므로 Canvas 간 겹침을 합성하는 화면은 아니다.
여러 Scene 사이의 순서는 기존 활성 Scene 순서를 따른다. 전역 Canvas 정렬 정책은 별도 범위다.

여러 Canvas의 입력 배분은 호출자가 처리한다는 기존 UIEventSystem 계약을 유지한다.
이 변경이 서로 다른 Canvas의 이벤트 소유자를 자동으로 선택하는 것은 아니다.

UI 순서는 3D 깊이 좌표나 RectTransform의 위치·크기를 바꾸지 않는다.
UILayout이 자식 목록의 복사본을 안정 정렬하고, 원본 Hierarchy 관계와 자식 배열은 유지한다.
정렬된 레이아웃은 UIRenderSystem의 제출 순서, UI View의 편집용 선택과
UIEventSystem의 포인터 검사에서 함께 사용한다.
기본 UI 머티리얼의 깊이 테스트는 기존과 같이 비활성화되어 있다.

Z-Index는 부호 있는 32비트 정수다. 음수도 사용할 수 있다.
최댓값/최솟값을 넘는 메뉴 연산은 비활성화한다. 경계에서 다른 요소의 값을 자동 재배치하지 않는다.
Z +1은 숫자 증가이며, 다른 형제의 값까지 큰 간격이 있으면 즉시 그 형제를 넘지 않을 수 있다.
전체 형제 앞뒤로 이동할 때는 Bring to Front / Send to Back을 사용한다.

## 검증 상태

정적 코드·호출부와 공백 검사를 수행했다. 이번 변경의 빌드·테스트 실행과 GPU 조작은 수행하지 않았다.

- UILayoutCheck: 기본 순서, 값 변경, 같은 값의 안정성, 하위 트리 유지, 정수 경계, 원본 계층 보존과 저장·복원.
- UIInteractionCheck: 겹치는 버튼의 Z-Index 변경과 동률, 음수 값, 앞 이미지의 입력 차단·통과.
- EditorHistoryCheck: 컴포넌트가 없던 요소의 순서 추가·제거와 값 변경 Undo/Redo.

```powershell
cmake --preset x64-debug -DBAMENGINE_BUILD_REFLECTION_CHECK=ON
cmake --build out/build/x64-debug --config Debug
ctest --test-dir out/build/x64-debug -C Debug -R 'UILayoutCheck|UIInteractionCheck|EditorHistoryCheck' --output-on-failure
```

사용 중인 빌드 폴더가 다르면 경로를 맞춘다. 새 컴포넌트의 Reflection 코드와 언어 JSON은 빌드에서 갱신한다.
수동으로는 Image/Button 두 개를 겹쳐 Z-Index를 바꾸고, 표시·편집용 선택·Interact 클릭 대상이 같은지 확인한다.
자식이 있는 패널의 전체 순서, 저장 후 다시 열기와 Undo/Redo도 함께 확인한다.
