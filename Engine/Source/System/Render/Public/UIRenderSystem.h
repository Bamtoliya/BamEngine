#pragma once

#include "System.h"

namespace Engine
{
    class ENGINE_API UIRenderSystem final : public ISystem
    {
    public:
        static UIRenderSystem* Create(void* arg = nullptr) { return new UIRenderSystem(); }
        void OnSubmit(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt) override;
    };
}
