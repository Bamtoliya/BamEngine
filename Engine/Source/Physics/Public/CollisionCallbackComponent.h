#pragma once

#include "Engine_Includes.h"
#include "ComponentAnnotations.h"
#include <functional>
#include <utility>

BEGIN(Engine)

enum class ECollisionEventType : uint8_t
{
    Enter,
    Stay,
    Exit
};

struct ENGINE_API CollisionEvent
{
    entt::entity entityA = entt::null;
    entt::entity entityB = entt::null;
    vec3 contactPoint = vec3(0.f);
    vec3 contactNormal = vec3(0.f);
    f32 penetrationDepth = 0.f;
    vec3 relativeVelocity = vec3(0.f);
    ECollisionEventType type = ECollisionEventType::Enter;
};

using CollisionCallback = std::function<void(entt::entity self, entt::entity other, const CollisionEvent& event)>;

STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_COLLISION_CALLBACK"), CATEGORY("CATEGORY_PHYSICS"))
struct ENGINE_API CollisionCallbackComponent
{
    REFLECT_BODY()

    // 런타임 콜백 델리게이트 (직렬화 제외)
    CollisionCallback OnCollisionEnter;
    CollisionCallback OnCollisionStay;
    CollisionCallback OnCollisionExit;

    void SetOnCollisionEnter(CollisionCallback callback) { OnCollisionEnter = std::move(callback); }
    void SetOnCollisionStay(CollisionCallback callback) { OnCollisionStay = std::move(callback); }
    void SetOnCollisionExit(CollisionCallback callback) { OnCollisionExit = std::move(callback); }
};

END
