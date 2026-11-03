#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "TriggerEvent.h"
#include <functional>

namespace Engine
{
    using TriggerCallback = std::function<void(entt::entity self, entt::entity other, ETriggerEventType type)>;

    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_TRIGGER_CALLBACK"), CATEGORY("CATEGORY_PHYSICS"))
        struct TriggerCallbackComponent
    {
        REFLECT_BODY()

        // 런타임 콜백 델리게이트 (직렬화 제외)
        TriggerCallback OnEnter;
        TriggerCallback OnStay;
        TriggerCallback OnExit;

        void SetOnEnter(TriggerCallback callback) { OnEnter = std::move(callback); }
        void SetOnStay(TriggerCallback callback) { OnStay = std::move(callback); }
        void SetOnExit(TriggerCallback callback) { OnExit = std::move(callback); }
    };
}