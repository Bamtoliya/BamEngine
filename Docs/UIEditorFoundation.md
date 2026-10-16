# ECS UI 편집기 첫 단계

후속 UI 버튼·포인터 상호작용은 [UIInteraction.md](UIInteraction.md)를 따른다.
Z-Index 편집과 그리기·선택 순서는 [UIRenderOrder.md](UIRenderOrder.md)를 따른다.

## 사용 흐름

1. 씬을 열거나 만든다.
2. UI View의 `Canvas → New Canvas` 또는 Hierarchy의 `Add UI Canvas`로 Canvas Entity를 만든다.
3. `Canvas → Add Image`로 Image를 추가한다. 선택한 UI 요소가 있으면 그 아래에, 없으면 Canvas 아래에 만든다.
4. Sprite가 없는 Image는 단색 사각형이다. Inspector의 UIImageComponent에서 Sprite·색상·UI용 Material을 지정한다.
5. UI View에서 Image를 클릭하고 드래그해 이동한다. 우하단 핸들은 반대 모서리를 유지하며 크기를 조절한다.
6. Ctrl + 클릭은 선택 추가·제거, 빈 영역 클릭은 선택 해제다. 이동·크기 조절은 조작 대상 하나에 적용한다.
7. `Snap`에서 Grid 또는 Pixel을 선택한다. Alt를 누르는 동안 스냅을 해제한다.
8. 휠은 마우스 지점을 유지하는 줌, 가운데 버튼은 팬, `Fit` 또는 F는 캔버스 전체 보기다.
9. `Canvas` 메뉴에서 대상과 디자인 해상도를 바꾼다. RT 리사이즈는 다음 Update에서 수행한다.

선택 잠금은 선택 및 드래그 변경을 막는다. Escape는 진행 중인 드래그를 시작 값으로 되돌린다.
View의 안전 영역은 고정 5% 가이드이며 기기별 실제 safe-area 데이터가 아니다.
100%는 ImGui 화면 단위 하나당 캔버스 픽셀 하나다. 고밀도 디스플레이의 물리 픽셀과는 다를 수 있다.

## 데이터와 책임

컴포넌트는 `Engine/Source/Core/Public/Components/UI`에 파일별로 둔다.

- CanvasComponent.h: 디자인 기준 해상도. 첫 단계의 지원 범위는 최상위 ScreenSpace Canvas다.
- RectTransformComponent.h: AnchorMin/Max, AnchoredPosition, SizeDelta, Pivot, Scale과 Rotation.
- UIImageComponent.h: Sprite·Material 참조와 색상. 기본 UIMaterial은 NonPremultiplied 블렌딩을 사용한다.
- UIRenderOrderComponent.h: Canvas 또는 같은 부모 아래 형제 그룹의 zIndex.

UI 계층은 기존 HierarchyComponent의 부모와 자식 순서를 사용한다.
UI Entity에는 위치 계산을 위해 3D TransformComponent를 추가하지 않는다.
Engine의 UILayout이 실제 사각형과 캔버스 변환을 계산하며, Editor와 UIRenderSystem이 같은 결과를 사용한다.
계산 결과는 임시 UIElementLayout 목록이고 직렬화되는 컴포넌트 저장소에 넣지 않는다.

부모 크기를 P, 앵커를 aMin/aMax, Pivot을 p라고 하면 다음과 같이 계산한다.

```text
size = P * (aMax - aMin) + sizeDelta
anchor = aMin + (aMax - aMin) * p
localPivotPosition = P * (anchor - parentPivot) + anchoredPosition
worldTransform = parentTransform * T(localPivotPosition) * R(rotation) * S(scale)
renderMatrix = worldTransform * S(size) * T(-p)
```

캔버스 원점은 좌상단이고 +Y는 아래다. 정규화된 단위 쿼드를 renderMatrix로 변환한다.
부모의 회전과 비균일/음수 스케일을 반영한다. 영 크기·영 스케일은 그릴 면적이 없고 선택하지 않는다.
해상도는 1~8192의 정수 픽셀이고, 앵커·Pivot·수치와 최종 크기를 검사한다.
Inspector와 드래그 편집은 잘못된 레이아웃을 만들면 기존 값을 유지한다.

## 표시와 렌더링

UIViewportPanel은 카메라 기반 패널 대신 BaseViewportPanel을 사용한다.
표시·입력 변환은 Editor의 CanvasView2D 하나가 담당한다.

```text
screen = viewportOrigin + viewportSize / 2 + (canvasPoint - viewCenter) * zoom
canvasPoint = viewCenter + (screen - viewportOrigin - viewportSize / 2) / zoom
```

Canvas 디자인 해상도와 편집기의 화면 확대율은 독립적이다. 팬·줌으로 씬의 Canvas 설정을 바꾸지 않는다.
그리드는 확대율에 따라 기본 간격의 2배 단위로 표시를 생략하지만 실제 스냅 간격은 유지한다.
이동은 캔버스의 Pivot 좌표에 스냅하고 부모 역변환으로 AnchoredPosition에 반영한다.
크기 조절은 요소 로컬 크기를 스냅한다. 회전·확대한 부모 아래에서는 크기 스냅과 캔버스 격자가 다를 수 있다.
핸들의 표시 크기와 클릭 허용 반경은 확대율과 무관한 화면 단위다.

UIRenderSystem은 같은 Scene의 Canvas를 Z-Index와 ID 순으로 제출한다.
요소는 각 부모의 형제를 Z-Index로 안정 정렬한 깊이 우선 순서로 제출한다.
같은 값일 때는 기존 Hierarchy 자식 순서를 유지하고, 부모와 하위 트리는 한 묶음으로 유지한다.
뒤에 제출된 Image가 앞의 Image 위에 합성된다. 클릭 검사는 같은 목록을 역순으로 탐색한다.
각 UI 패스는 특정 Scene/Canvas로 필터링할 수 있다. 미리보기는 선택한 Canvas만 제출한다.
필터 없는 UI 패스는 활성 씬의 Canvas를 동일한 Engine 코드로 그린다.
첫 단계의 다른 해상도 출력은 기준 해상도를 대상 RT 전체에 매핑한다. CanvasScaler/종횡비 맞춤 정책은 후속 작업이다.

UI 이미지 명령은 Texture와 Material 핸들을 렌더 종료까지 보유한다.
Runtime 종료 시에는 ResourceManager보다 먼저 명령을 정리한다.
UI View의 RT는 불투명한 캔버스 배경을 사용하며 UIMaterial의 알파 합성은 One/InvSrcAlpha로 alpha를 계산한다.
이는 캔버스 RT를 ImGui 이미지로 다시 합성할 때 알파가 중복 적용되는 문제를 피한다.
드래그 편집은 다음 Update에서 적용하므로 새 값으로 렌더 명령을 제출한다.
동시에 Inspector가 값을 변경했으면 이전 드래그 값으로 덮어쓰지 않고 조작을 취소한다.

UI VS는 CPU의 GLM 행렬과 동일하게 행렬×벡터 순서를 사용한다. 상수 데이터는 기존 UIUBO의 112바이트 배치를 유지한다.
HLSL과 GLSL UI 소스에 UV Offset과 같은 행렬 계산을 반영했다. 기존 register/set 배치는 이동하지 않았다.
기존 BINDING_LAYOUT.md의 정책과 현재 RHI 바인딩 배치의 차이는 이번 작업에서 일괄 변경하지 않았다.

## Sprite 2D 기반

월드 Sprite는 영역 픽셀 크기 / Sprite PPU로 크기를 계산하고, Pivot을 반영해 공유 [-1, 1] QuadMesh를 변환한다.
픽셀 크기가 없는 영역은 텍스처 전체를 사용한다. 월드 Sprite의 Pivot은 좌하단 기준이다.
ECS SpriteRenderSystem과 레이 경계 검사는 같은 Sprite::TryGetQuadMatrix 결과를 사용한다.
UI Image는 RectTransform 크기를 사용하므로 Sprite의 PPU가 UI 배치 크기를 바꾸지 않는다.
이번 작업은 Sprite 배칭·반투명 월드 정렬·Sprite 에셋 편집기 전체 구현을 포함하지 않는다.

## 저장과 현재 제한

UI 전용 계층은 3D Transform 없이도 씬에 저장·복원할 수 있도록 계층 검사를 보완했다.
vec2/vec4는 GLM 코덱으로, Sprite 및 MaterialInterface 핸들은 공통 리소스 참조 코덱으로 처리한다.
핸들 코덱은 Null 또는 AssetKey/AssetPath를 저장하고 복원 시 타입을 검사한다. 런타임 핸들 숫자는 저장하지 않는다.
참조가 없으면 보존하고, 참조가 지정되어 있는데 복원하지 못하면 씬 로드가 실패한다.
에디터 시작 시 생성하는 기본 UIMaterial 등은 같은 키로 먼저 로드되어 있어야 한다.
Client의 씬·리소스 로딩과 패스·시스템 등록까지 이 변경만으로 완성된 것은 아니다.

아직 지원하지 않는 기능:

- UI 부모 변경: 3D ReparentEntity로 옮기려 하면 거부한다. 앵커·크기를 보존하는 전용 처리 필요.
- 캔버스 중첩, WorldSpace UI, 마스크/자식 클리핑, 레이아웃 그룹과 텍스트.
- 버튼과 포인터 이벤트는 후속 구현에 추가했다. Client/Game View 입력 연결과 키보드·터치 입력은 남아 있다.
- 다중 요소 동시 변형, 네 모서리 리사이즈, 회전 핸들과 정렬 가이드.
- Sprite 투명 픽셀을 제외하는 UI 피킹. 현재 선택은 변환된 사각형 기준이다.
- 임의의 Shader/Material 레이아웃. UIImage에는 기존 UIUBO 및 UI VS/PS 출력 규약에 맞는 Material이 필요하다.

## 검증 상태

UI 생성·이동·크기·해상도와 Inspector 속성의 Undo/Redo 연결 및 그 검증은
[EditorUndoRedo.md](EditorUndoRedo.md)를 따른다. 아래 내용은 UI 기반 작업 당시의 검증 기록이다.

구현과 호출부를 정적으로 점검했다. 빌드·테스트 실행과 GPU 검증은 수행하지 않았다.
AGENTS.md에 따라 다음 명령은 사용자가 실행한다.

```powershell
cmake --preset x64-debug -DBAMENGINE_BUILD_REFLECTION_CHECK=ON
cmake --build out/build/x64-debug --config Debug
ctest --test-dir out/build/x64-debug -C Debug -R 'UILayoutCheck|CanvasViewCheck|PickingMathCheck|SceneArchiveCheck|HierarchyEditorCheck' --output-on-failure
```

UILayoutCheck: 고정/스트레치 앵커, Pivot, 부모 회전·스케일, 가시성 상속, 특이 변환, 잘못된 입력과 순환,
UI 전용 계층의 JSON 저장·복원과 자식 순서를 확인한다.
CanvasViewCheck: 좌표 왕복, 마우스 중심 줌, 팬과 확대율에 독립적인 스냅을 확인한다.

수동으로 확인할 항목:

1. Canvas와 단색 Image를 생성하고 Inspector의 RectTransform과 색상이 반영되는지 확인한다.
2. Sprite를 지정해 UV 방향·영역을 확인하고, 투명 Image 두 개의 합성 순서를 확인한다.
3. 12.5%, 100%, 800%에서 이동·픽셀/그리드 스냅, 우하단 크기 조절과 Escape 복원을 확인한다.
4. 자식 Image를 추가하고 부모의 회전·스케일을 변경해 표시·선택·이동을 확인한다.
5. 디자인 해상도·패널 크기 변경, Fit·줌·팬, 씬 교체와 Canvas 선택 변경을 확인한다.
6. 씬을 저장하고 다시 열어 UI 계층과 Sprite/Material 참조가 유지되는지 확인한다.
7. 월드 Sprite의 PPU·영역·Pivot에 따른 크기와 SceneView의 GPU ID/레이 경계 선택이 맞는지 확인한다.
8. 기본 DirectX12와 SDL GPU 각각에서 셰이더·합성 결과 및 검증 레이어 오류를 확인한다.
