#pragma once

#include "RigidBody.h"
#include "ComponentRegistry.h"
#include "GameObject.h"
#include "Transform.h"

REGISTER_COMPONENT(RigidBody)

#pragma region Constructor&Destructor
EResult RigidBody::Initialize(void* arg)
{
    if (arg)
    {
        CAST_DESC
            m_Mass = glm::max(0.0001f, desc->Mass);
        m_UseGravity = desc->UseGravity;
        m_GravityScale = desc->GravityScale;
        m_LinearDamping = glm::max(0.f, desc->LinearDamping);
        m_IsKinematic = desc->IsKinematic;
        m_Velocity = desc->InitialVelocity;
    }
    return __super::Initialize(arg);
}

RigidBody* RigidBody::Create(void* arg)
{
    RigidBody* instance = new RigidBody();
    if (IsFailure(instance->Initialize(arg)))
    {
        Safe_Release(instance);
        return nullptr;
    }
    return instance;
}

Component* RigidBody::Clone(GameObject* owner, void* arg)
{
    RigidBody* instance = new RigidBody();

    DESC desc;
    desc.Owner = owner;
    desc.Active = m_Active;
    desc.Tag = m_Tag;
    desc.Mass = m_Mass;
    desc.UseGravity = m_UseGravity;
    desc.GravityScale = m_GravityScale;
    desc.LinearDamping = m_LinearDamping;
    desc.IsKinematic = m_IsKinematic;
    desc.InitialVelocity = m_Velocity;

    if (IsFailure(instance->Initialize(&desc)))
    {
        Safe_Release(instance);
        return nullptr;
    }
    return instance;
}

void RigidBody::Free()
{
    __super::Free();
}
#pragma endregion

#pragma region Loop
void RigidBody::FixedUpdate(f32 dt)
{
    if (!m_Active || !m_Owner || dt <= 0.f)
        return;

    if (m_IsKinematic)
    {
        m_AccumulatedForce = vec3(0.f);
        return;
    }

    Transform* transform = m_Owner->GetComponent<Transform>();
    if (!transform)
    {
        m_AccumulatedForce = vec3(0.f);
        return;
    }

    vec3 acceleration = m_AccumulatedForce / glm::max(0.0001f, m_Mass);

    if (m_UseGravity)
        acceleration += vec3(0.f, -9.81f * m_GravityScale, 0.f);

    m_Velocity += acceleration * dt;

    const f32 dampingFactor = glm::clamp(1.f - m_LinearDamping * dt, 0.f, 1.f);
    m_Velocity *= dampingFactor;

    const vec3 newPos = transform->GetLocalPosition() + m_Velocity * dt;
    transform->SetPosition(newPos);

    m_AccumulatedForce = vec3(0.f);
}
#pragma endregion