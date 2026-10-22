# ECS UI 포인터 상호작용

## 에디터에서 확인하기

1. UI View의 `Canvas → New Canvas`, `Canvas → Add Button`으로 버튼을 만든다.
2. 편집 모드에서 배치하고 Inspector의 UIButtonComponent에서 interactable과 상태별 색상을 설정한다.
3. 메뉴 바의 `Interact`를 켠다. 이 모드의 왼쪽 버튼 입력은 요소 편집 대신 UIEventSystem으로 전달된다.
4. 버튼에 마우스를 올리고 누르거나 놓는다. 아래 상태 줄에 마지막 이벤트와 Click 누적 횟수가 표시된다.
5. 버튼 밖에서 놓기, Escape, 다른 창으로 포커스 이동, 버튼 비활성화를 확인한다.
6. `Interact`를 끄면 선택·이동·크기 조절로 돌아간다.

팬·줌·Fit, 캔버스/해상도 변경, 편집 이력 변경과 패널 숨김은 진행 중인 누름을 취소한다.
처음 켜거나 취소한 뒤에는 버튼이 놓인 입력을 한 번 받아야 새 누름을 시작한다.
편집용 선택 잠금은 Interact 입력을 막지 않는다. 버튼의 interactable이 실행 중 상호작용을 제어한다.
미리보기는 PIE 실행과 별개이며 프로젝트 동작을 자동으로 실행하지 않는다.

현재 패널의 입력 처리는 ImGui 렌더 패스에서 이루어진다. UI 이미지 패스는 그보다 먼저 실행되므로
호버·누름 색상은 다음 렌더 프레임에 표시된다. 아래 Engine API를 렌더 명령 제출 전에 호출하면
새 상태를 같은 프레임의 UI 렌더링에 사용할 수 있다.

## 데이터와 실행 상태

- `Core/Public/Components/UI/UIButtonComponent.h`: interactable과 Normal/Hovered/Pressed/Disabled 색상 배율.
- `Core/Public/Components/UI/UIInteractionStateComponent.h`: 입력 소유자가 계산한 임시 hovered/pressed 상태.
- UIImageComponent의 raycastTarget: 해당 이미지가 포인터 검사에 참여하고 뒤의 UI를 차단하는지 여부.

UIButton은 같은 Entity의 RectTransform, UIImage와 Canvas 아래의 유효한 계층을 사용한다.
Add Button 팩토리는 Image 생성 경로에 UIButtonComponent를 추가한다.
기존 Image에 Inspector에서 UIButtonComponent를 추가해도 된다.

상태 색상은 UIImage.color에 곱해 렌더 명령으로 전달한다. 원본 Image 색상은 변경하지 않는다.
UIInteractionStateComponent는 TRANSIENT/HIDDEN이며 씬 저장과 에디터 편집 이력의 스냅샷에서 제외한다.
포인터가 떠나거나 입력 소유자가 종료되면 불필요한 임시 상태를 제거한다.
이력 복원으로 임시 상태가 제거되면 다음 입력 처리에서 현재 상태를 다시 계산한다.
콜백은 컴포넌트에 저장하거나 직렬화하지 않는다.

raycastTarget은 이전 씬을 읽을 때 누락을 허용하며 기본값은 true다.
색상의 alpha가 0인 이미지도 raycastTarget=true이면 입력을 차단한다.
장식용 이미지가 입력을 통과시켜야 하면 해당 값을 false로 설정한다.
Sprite 픽셀의 alpha를 읽는 검사는 아직 없으며 변환된 사각형으로 검사한다.

## 포인터 처리 규칙

UIEventSystem은 Scene과 Canvas에 묶인 일반 객체이며 전역 싱글턴이 아니다.
Scene 참조를 유지하므로 원래 소유자가 참조를 해제해도 입력 처리 중 Scene이 소멸하지 않는다.
하나의 Canvas에는 하나의 UIEventSystem이 포인터 상태를 기록하도록 한다.
여러 Canvas 중 우선순위를 정하고 입력을 배분하는 일은 호출자의 책임이다.

UILayout의 Z-Index가 반영된 자식 그리기 순서를 역순으로 검사하여 가장 위의 raycastTarget 이미지를 찾는다.
Z-Index 편집과 형제/Canvas 그룹의 비교 범위는 [UIRenderOrder.md](UIRenderOrder.md)를 따른다.
일반 이미지와 비활성 버튼도 뒤의 요소를 차단한다. 이미지를 찾은 뒤 가장 가까운 부모 버튼으로
대상을 결정하므로 버튼의 자식 이미지를 눌러도 부모 버튼이 이벤트를 받는다.
이는 버튼 대상 결정 규칙이며 캡처/버블링 단계가 있는 일반 이벤트 전파 구현은 아니다.

- Enter/Exit: 상호작용 가능한 버튼 대상의 변경.
- Down: 해당 버튼에서 시작한 누름. 버튼이 포인터 캡처 대상이 된다.
- Up: 누른 버튼에 전달한다. 버튼 밖이나 뷰포트 밖에서 놓아도 전달하지만 Click은 발생하지 않는다.
- Click: 같은 버튼을 누르고 그 버튼을 대상으로 놓았을 때 전달한다.
- Cancel: 포커스 상실, 명시적 취소, 비활성화, 삭제, 잘못된 좌표/계층 또는 놓음 상태만 감지했을 때 전달한다.

누른 채 밖으로 이동하면 캡처는 유지하고 호버만 해제한다. 다시 들어오면 눌림 표시로 돌아간다.
Enter 뒤에 Down, Up 뒤에 Click 순으로 통지한다. 한 입력 샘플에 Down/Up이 모두 있으면 한 번 클릭할 수 있다.
취소 후 버튼을 계속 누르고 있는 동안에는 새로운 누름이나 클릭을 만들지 않는다.
키보드 포커스·Submit, 드래그 이벤트, 휠, 오른쪽 버튼, 터치/다중 포인터는 후속 범위다.

이벤트 대상은 EnTT 실행 핸들과 영구 ID로 확인한다. 콜백에서 대상이나 컴포넌트를 제거해도
다음 이벤트를 전달하기 전에 계층·가시성·상호작용 가능 여부를 다시 검사한다.
Up 콜백이 버튼을 비활성화하거나 다른 이미지로 덮으면 이어질 Click을 생략한다.
Cancel/Exit에는 이미 삭제된 대상의 ID가 포함될 수 있다. 대상 접근에는 ResolveTarget을 사용한다.

## 프로젝트 코드에서 사용하기

다음은 Engine API 연결 예시다. 프로젝트의 UI 소유자가 system과 구독 핸들을 보관한다.
플랫폼 입력이나 ImGui 좌표를 UIEventSystem 내부에서 직접 읽지 않는다.

```cpp
#include "UIEventSystem.h"
#include "Scene.h"

Engine::UIEventSystem system(scene, canvasEntity);
const auto subscription = system.OnPointerEvent().AddLambda([&](const Engine::UIPointerEvent& event)
{
    if (event.type != Engine::EUIPointerEventType::Click) return;
    auto* target = system.ResolveTarget(event);
    if (!target) return;
    // target/event.targetId를 사용해 프로젝트의 버튼 동작을 처리한다.
});

// 매 입력 프레임: 출력 영역을 Canvas 좌표로 변환한 후 렌더 명령 제출 전에 호출한다.
Engine::UIPointerInput input;
input.position = canvasPosition;
input.insideViewport = pointerInsideOutput;
input.focused = outputHasFocus;
input.held = leftHeld;
input.down = leftDown;
input.up = leftUp;
system.Update(input);

// 게임 월드 입력과 조합할 때 UI 차단 여부와 캡처를 함께 확인한다.
const bool blocksWorldInput = system.IsPointerOverUI() || system.HasPointerCapture();

// 콜백이 캡처한 객체가 종료되기 전에 구독을 해제한다.
system.OnPointerEvent().Remove(subscription);
system.Cancel();
```

위 코드는 연결 예시이며 canvasPosition 등의 입력 값은 호출자가 제공한다.
출력 사각형의 원점 O, 크기 S와 Canvas 기준 해상도 C를 사용하면 `(pointer - O) / S * C`로 변환한다.
레터박스 여백은 insideViewport에서 제외한다. UI View는 CanvasView2D의 역변환을 사용한다.
Client/Game View의 입력·패스 등록까지 자동 연결한 것은 아니며 동일 API를 명시적으로 연결해야 한다.

구독 해제는 핸들로 수행한다. 수신 객체의 수명을 자동 추적하지 않으므로 수신 객체보다 먼저 해제한다.
system이 종료되면 남은 구독은 제거한다. system은 Scene/ResourceManager 종료 전에 정리한다.
콜백 안에서 같은 system의 Update/Cancel을 재호출하면 false를 반환한다.
system 자체를 콜백 도중 파괴하지 않는다. 이 API와 MulticastDelegate는 메인 스레드에서 사용한다.
콜백 예외는 호출자에게 전파되며 처리 중 플래그는 복구된다. 이미 수행한 상태 변경은 되돌리지 않는다.

MulticastDelegate는 실행 시작 시 구독 목록을 보관한다. 실행 중 제거한 구독은 이후 호출을 건너뛰며,
추가한 구독은 다음 Broadcast부터 호출한다. 자기 자신을 제거하거나 Clear해도 현재 callable은
호출 종료까지 유지된다. mutable lambda의 내부 상태도 호출 간에 유지한다.

## 검증 상태

정적 코드·호출부와 공백 검사를 수행했다. 빌드·테스트 실행과 Editor/GPU 조작은 수행하지 않았다.
UIInteractionCheck는 정상 클릭, 밖에서 놓기, 입력 차단/통과, 자식 이미지, 포커스 상실,
비활성화·삭제·식별자 변경, 콜백 중 변경, 재진입 거부, 임시 상태 저장 제외, Scene 수명과 구독 변경을 검사한다.

```powershell
cmake --preset x64-debug -DBAMENGINE_BUILD_REFLECTION_CHECK=ON
cmake --build out/build/x64-debug --config Debug
ctest --test-dir out/build/x64-debug -C Debug -R 'UIInteractionCheck|UILayoutCheck|CanvasViewCheck|SceneArchiveCheck' --output-on-failure
```

실제 Editor에서는 Interact 전환, 창 포커스/숨김, 캔버스 교체, 팬·줌 중 취소,
버튼 tint, 투명 이미지의 입력 차단 및 클릭 횟수를 별도로 확인해야 한다.

## Add Button 이후 파이프라인 생성 실패

2026-10-16의 기존 DXIL 검사에서 UI VS는 TEXCOORD0/1을 레지스터 1/2로 출력하고,
UI PS는 같은 의미의 입력을 레지스터 0/1로 읽고 있었다. PS에 SV_POSITION 선언이 빠진 상태였다.
UI VS/PS의 입출력 선언을 uiCommon.hlsli로 통합했다. 리소스 register/space와 상수 버퍼 배치는 변경하지 않았다.
인접 셰이더의 시그니처 연결 규칙은 [Microsoft 문서](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-signatures)를 따른다.

수정한 소스는 다음 빌드에서 바이너리에 반영해야 한다. 아래 빌드 폴더는 현재 사용 중인 폴더로 맞춘다.

```powershell
cmake --build out/build/x64-debug --config Debug --target CompileShaders Editor
```

다시 실행해 Canvas → Add Button을 확인한다. 수정 후 컴파일·PSO 생성 성공과 화면 표시는 아직 검증하지 않았다.
여전히 실패하면 Logs/Editor.log의 CreateGraphicsPipelineState HRESULT와 D3D12 PSO validation을 확인한다.
Debug layer가 활성화된 실행에서는 해당 PSO 생성 호출 중 발생한 상세 검증 메시지를 로그에 함께 기록한다.
