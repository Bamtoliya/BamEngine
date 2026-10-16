# SceneView 피킹

## 사용 방법

Scene Viewport의 `Picking` 메뉴에서 선택 방식을 변경한다.

- `GPU ID`: 기본값. Geometry 패스가 기록한 픽셀의 Entity를 선택한다.
- `Ray bounds (broad phase)`: 메시 경계 상자와 레이의 교차를 검사한다. 삼각형 정밀 검사를 추가하기 위한 기반이다.

왼쪽 클릭은 단일 선택, Ctrl + 클릭은 선택 추가·제거다. 빈 배경 클릭은 선택을 해제한다.
Ctrl + 배경 클릭은 현재 선택을 유지한다. 선택 잠금 중에는 변경하지 않는다.
이미지 밖의 여백, 메뉴, 변환 기즈모와 뷰 기즈모의 클릭은 피킹하지 않는다.

## GPU ID 처리

1. MeshRenderSystem과 SpriteRenderSystem이 제출하는 Entity에 프레임별 임시 ID를 부여한다.
2. 정적 메시의 `SceneUBO`는 worldMatrix 뒤 offset 64에 pickingData를 둔다. 총 크기는 80바이트다.
3. 스프라이트 ObjectData는 worldMatrix, color, uvTransform 뒤 offset 96에 pickingData를 둔다. 총 크기는 112바이트다.
4. VS에서 pickingData.x를 보간하지 않고 PS에 전달한다.
5. `GBuffer_Position`의 RGB는 월드 위치, A는 피킹 ID다. 포맷은 RGBA32F이고 배경 ID는 0이다.
6. SceneView가 그려지는 ImGui 프레임에서 이미지 좌표와 Ctrl 상태, 렌더 프레임 번호를 저장한다.
7. 다음 Update의 시작에서, RT 리사이즈 전에 해당 픽셀을 읽고 완료된 프레임의 ID 테이블로 Entity를 찾는다.

Float32가 정수를 정확히 보존하는 범위 안에서 ID를 발급한다. 사용 범위는 1~16,777,215다.
64비트 IDComponent나 EnTT 핸들을 float로 직접 변환하지 않는다. 발급 한도를 넘으면 ID 0을 사용한다.
같은 Entity가 여러 뷰포트로 제출되어도 해당 렌더 프레임에서는 같은 ID를 사용한다.

조회 테이블은 Scene 참조, EnTT 핸들, 당시 IDComponent 값을 보관한다.
현재 활성 씬 목록과 핸들의 유효성, IDComponent와 Active/Visible/Dead 플래그를 확인한 뒤 선택한다.
프레임 번호 불일치, 렌더/픽셀 다운로드 실패와 무효화된 ID에서는 기존 선택을 유지한다.
Runtime 종료 시에는 Scene/Resource 관리자를 해제하기 전에 피킹 테이블의 Scene 참조를 정리한다.

`RHI::ReadTexturePixel`은 제출된 RGBA32F 단일 샘플 텍스처의 한 픽셀을 동기적으로 내려받는다.
프레임 명령을 기록하는 도중에는 호출하지 않는다. SDL GPU와 네이티브 DirectX12가 구현되어 있다.
SDL GPU는 download transfer buffer와 fence를 사용하고, DirectX12는 readback heap과 COPY_SOURCE 전환 후 상태 복원을 사용한다.
두 경로 모두 GPU 완료를 기다리므로 클릭 시 지연이 발생할 수 있다. 비동기 readback은 후속 작업이다.
DirectX12 제출 이후 fence 동기화 자체가 실패하면, 기존 텍스처 업로드 경로와 같이 치명적 오류로 종료한다.

## 지원 범위와 제한

- 현재 ECS Geometry 경로의 StaticMeshRendererComponent와 Simple SpriteRendererComponent를 지원한다.
- 새 스프라이트는 SpriteMaterial을 사용한다. GBuffer 스프라이트는 Masked 모드로 제출하고 알파 0.01 미만을 버린다.
  부드러운 반투명 합성은 지원 범위가 아니다. GBuffer에 블렌딩을 적용하면 위치와 ID도 섞이므로 허용하지 않는다.
- 커스텀 Geometry 셰이더는 같은 ObjectData 레이아웃과 Position.A 출력 규약을 구현해야 GPU ID로 선택할 수 있다.
- 구형 GameObject, 스키닝, Forward/ForwardTransparent, UI 전용 객체와 메시가 없는 객체는 GPU ID 대상에 포함되지 않는다.
  기존 Hierarchy 선택 경로와 GameObject 콜라이더 조회 API는 유지한다.
- 디버그 RT를 표시하더라도 GPU ID는 해당 SceneView의 GBuffer Geometry를 기준으로 선택한다.
- 레이 모드는 로컬 메시 AABB에 역월드 변환한 레이를 검사한다. 스프라이트 투명 부분과 메시의 빈 부분도 상자에 포함된다.
  가림, 머티리얼 컬링과 삼각형 표면은 판단하지 않는다. 레이 모드 결과가 GPU ID와 항상 같지는 않다.
- 레이의 NDC 깊이는 엔진의 GLM 설정과 동일한 [0, 1]이다. 원근·직교 투영 모두 near~far 구간만 검사한다.
  비균일/음수 스케일과 회전을 반영하고, 축과 평행한 레이, 두께 0인 상자와 특이 행렬을 처리한다.

기존 Shader/BINDING_LAYOUT.md는 공통 바인딩 정책을 정의하지만 현재 HLSL의 register 배치와 차이가 있다.
이번 작업에서는 레지스터 번호를 이동하지 않고 기존 ObjectData에 멤버와 VS/PS 전달값만 추가했다.
HLSL에서 생성하는 DXIL/SPIR-V와 GLSL 대응 소스를 함께 수정했다. GLSL은 현재 CMake 셰이더 컴파일 대상이 아니다.

## 검증

코드와 호출 순서를 정적으로 확인했다. 빌드, 테스트 실행과 GPU 동작 검증은 아직 수행하지 않았다.
루트 AGENTS.md의 빌드·테스트 실행 지침에 따라 다음 명령은 사용자가 실행한다.

```powershell
cmake --preset x64-debug -DBAMENGINE_BUILD_REFLECTION_CHECK=ON
cmake --build out/build/x64-debug --config Debug --target Editor PickingMathCheck
ctest --test-dir out/build/x64-debug -C Debug -R '^PickingMathCheck$' --output-on-failure
```

PickingMathCheck의 확인 항목:

- 레터박스 좌표 변환, 이미지 경계·여백·NaN·크기 0 거부
- 원근 투영 near 깊이 0, 화면 Y 방향, 직교 투영의 평행 레이, far 거리 제한
- 정규화되지 않은 입력 레이의 월드 거리, 축 평행 경계와 내부 시작
- 회전, 비균일/음수 스케일, 스프라이트 두께 0과 특이 행렬

Editor 수동 확인 항목:

1. Hierarchy에서 Cube와 Sprite를 추가하고 SceneView 클릭으로 Inspector/Hierarchy 선택이 바뀌는지 확인한다.
2. 서로 다른 위치의 객체를 연속 선택하고 Ctrl + 클릭으로 다중 선택과 제거를 확인한다.
3. 겹친 객체의 앞쪽 표면을 선택하고, 스프라이트 투명 부분에서는 뒤의 객체 또는 배경이 선택되는지 확인한다.
4. 배경 클릭, 선택 잠금, 메뉴·기즈모 클릭, 레터박스 여백 클릭을 확인한다.
5. 고정 해상도, 패널 리사이즈, 원근/직교 전환, 씬 교체 직후 클릭을 확인한다.
6. Ray bounds로 전환해 회전·비균일 스케일과 겹친 경계 상자에서 가장 가까운 진입점을 확인한다.
7. DirectX12와 SDL GPU 백엔드에서 각각 확인한다. 현재 기본 RHI 선택은 Editor_Macros.h를 따른다.
8. RHI 검증 레이어에서 텍스처 상태, 복사 행 피치, GPU 자원 수명 오류가 없는지 확인한다.

후속 단계는 CPU 메시 삼각형 접근, 레이-삼각형 교차와 결과 거리·법선 반환이다.
대규모 씬에서 필요한 경우 공간 가속 구조를 추가한다.
