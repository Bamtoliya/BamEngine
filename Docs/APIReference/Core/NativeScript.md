# Native Script

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 스크립트 수명
`ScriptableEntity`의 `OnCreate`, `OnDestroy`, `OnUpdate(float dt)`, `OnTriggerEnter/Stay/Exit(entt::entity other)`를 override한다. `GetScene()`과 `GetEntityHandle()`로 컨텍스트를 얻는다. `NativeScriptComponent::Bind<T>()`는 T 생성 factory를 저장하며 즉시 인스턴스가 생성된다고 가정하지 않는다.

```cpp
#include "NativeScriptComponent.h"
#include "Scene.h"
#include "TransformComponent.h"
class Spin final : public Engine::ScriptableEntity {
public:
    void OnUpdate(float dt) override {
        auto* scene = GetScene();
        if (!scene) return;
        auto& world = scene->GetRegistry();
        if (auto* t = world.try_get<Engine::TransformComponent>(GetEntityHandle()))
            (void)t->RotateLocal({0.f, 1.f, 0.f}, 90.f * dt);
    }
};
void Attach(Engine::Entity& entity) {
    auto& ns = entity.AddComponent<Engine::NativeScriptComponent>();
    ns.Bind<Spin>();
}
```
이 코드는 기존 컴포넌트가 없는 엔티티를 가정한다. 실제 script 실행 시스템 및 include/link 조건을 모듈 프로젝트에서 검증해야 한다.

## 제한
Bind factory와 Instance는 `std::function`, `std::unique_ptr`이며 헤더상 직렬화 가능한 안정적인 script type ID 필드가 없다. 따라서 씬 저장 후 자동 복원/Inspector에서 C++ 타입 선택을 보장하지 않는다(A06). `OnFixedUpdate` virtual hook은 현재 ScriptableEntity 인터페이스에 없다. 고정 물리 제어는 시스템을 통해 구현한다. 인스턴스/factory를 DLL 언로드 전에 해제한다.

## 관련 소스
- [ScriptableEntity.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Entity/Public/ScriptableEntity.h) — `Engine/Source/World/Entity/Public/ScriptableEntity.h`
- [NativeScriptComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Entity/Public/NativeScriptComponent.h) — `Engine/Source/World/Entity/Public/NativeScriptComponent.h`
- [Scene.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Scene/Public/Scene.h) — `Engine/Source/World/Scene/Public/Scene.h`

## 검증 및 참고 예제
- 이번 조사에서 이 API에 직접 대응하는 테스트를 확인하지 못함.
