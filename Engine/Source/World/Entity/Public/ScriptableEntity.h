#pragma once

#include "Engine_Includes.h"

BEGIN(Engine)
class Scene;

class ENGINE_API ScriptableEntity
{
    friend class PhysicsSystem;
    friend struct NativeScriptComponent;
public:
    virtual ~ScriptableEntity() = default;

    virtual void OnCreate() {}
    virtual void OnDestroy() {}
    virtual void OnUpdate(float dt) {}
    virtual void OnTriggerEnter(entt::entity other) {}
    virtual void OnTriggerStay(entt::entity other) {}
    virtual void OnTriggerExit(entt::entity other) {}

public:
    entt::entity GetEntityHandle() const { return m_Entity; }
    Scene* GetScene() const { return m_Scene; }

private:
    entt::entity m_Entity = entt::null;
    Scene* m_Scene = nullptr;
};
END