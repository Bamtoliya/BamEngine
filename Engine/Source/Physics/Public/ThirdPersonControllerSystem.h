#pragma once

#include "System.h"

namespace Engine
{
    class ENGINE_API ThirdPersonControllerSystem : public ISystem
    {
    public:
        ThirdPersonControllerSystem() = default;
        ~ThirdPersonControllerSystem() override = default;

        static ThirdPersonControllerSystem* Create(void* arg = nullptr);
        EResult Initialize(void* arg = nullptr);
        void Free() override;

        void OnUpdate(entt::registry& globalRegistry, const vector<Scene*>& activeScenes, f32 dt) override;

    private:
        void UpdateControllers(Scene& scene, f32 dt);
    };
}