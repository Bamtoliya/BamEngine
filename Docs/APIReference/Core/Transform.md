# Transform / 좌표계

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 좌표와 회전
프로젝트 CMake 소비 인터페이스는 `GLM_FORCE_LEFT_HANDED`, `GLM_FORCE_DEPTH_ZERO_TO_ONE`을 정의한다. 이를 좌표 변환과 프로젝션 코드에서 일관되게 유지한다. Transform은 local 값과 world 계산 결과를 구분한다.

| API | 의미 |
|---|---|
| `bool SetLocalPosition(const glm::vec3&)` | 부모 좌표계 위치 설정 |
| `bool SetLocalScale(const glm::vec3&)` | 로컬 스케일 설정 |
| `bool SetEulerRotation(const glm::vec3& degrees)` | 각도 단위 degree |
| `bool SetLocalRotation(const glm::quat&)` | quaternion 정규화 및 Euler 동기화 경로 |
| `TranslateLocal(delta)` / `TranslateParentSpace(delta)` | 이동 기준 좌표계 구분 |
| `RotateLocal(axis, deltaDegrees)` / `RotateParentSpace(axis, deltaDegrees)` | 회전 기준 좌표계 구분 |
| `GetLocalMatrix()` / `GetLocalForward()` | 로컬 계산 결과 |

```cpp
#include "TransformComponent.h"
bool MoveForward(Engine::TransformComponent& t, float speed, float dt) {
    return t.TranslateLocal({0.f, 0.f, speed * dt});
}
```

## 호출 순서
입력 -> 이동 계산 -> setter -> 엔진 transform/hierarchy 갱신 -> world 값 소비. 물리 객체는 물리/캐릭터 시스템의 이동 경로를 우선 사용한다. setter 반환값 false의 상세 의미는 메서드별 구현을 확인하고, 무조건 성공이나 오류로 동일 해석하지 않는다. 직접 position/rotation 필드를 고쳐 회전 동기화를 우회하지 않는다. 부모 변경 이후 즉시 world 값이 최신이라는 가정은 금지한다.

## 검증
부모 회전/비균일 스케일/절대 상속 플래그, 180도 경계 및 Euler↔quaternion 왕복, 저장 재열기에서 동일한 자세 확인. 카메라, collider, 스키닝이 같은 좌표계 계약을 따르는지 검사한다.

## 관련 소스
- [TransformComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Transform/Public/TransformComponent.h) — `Engine/Source/World/Transform/Public/TransformComponent.h`
- [WorldTransformComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Transform/Public/WorldTransformComponent.h) — `Engine/Source/World/Transform/Public/WorldTransformComponent.h`

## 검증 및 참고 예제
- [TransformEditorCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/examples/TransformEditorCheck.cpp)
- [TransformInheritanceCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/examples/TransformInheritanceCheck.cpp)
