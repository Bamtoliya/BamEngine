#pragma once

#include "Engine_Includes.h"
#include "System.h"

BEGIN(Engine)
class Scene;

class ENGINE_API SpringArmSystem final : public ISystem
{
private:
    SpringArmSystem() = default;
    virtual ~SpringArmSystem() = default;
    EResult Initialize(void* arg = nullptr);

public:
    static SpringArmSystem* Create(void* arg = nullptr);
    virtual void Free() override;

    virtual void OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*>& activeScenes, f32 dt) override;

public:
    static void UpdateSpringArms(Scene& scene, f32 dt);
};
END