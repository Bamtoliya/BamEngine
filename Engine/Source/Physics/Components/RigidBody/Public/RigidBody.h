#pragma once

#include "Component.h"
#include "ReflectionMacro.h"

BEGIN(Engine)

struct tagRigidBodyDesc : public tagComponentDesc
{
    f32 Mass = 1.f;
    bool UseGravity = true;
    f32 GravityScale = 1.f;
    f32 LinearDamping = 0.05f;
    bool IsKinematic = false;
    vec3 InitialVelocity = vec3(0.f);
};

CLASS()
class ENGINE_API RigidBody : public Component
{
    REFLECT_CLASS()
        using DESC = tagRigidBodyDesc;

#pragma region Constructor&Destructor
private:
    RigidBody() = default;
    virtual ~RigidBody() = default;
    virtual EResult Initialize(void* arg = nullptr) override;
public:
    static RigidBody* Create(void* arg = nullptr);
    virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
    virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
    virtual void FixedUpdate(f32 dt) override;
#pragma endregion

#pragma region API
public:
    void AddForce(const vec3& force) { m_AccumulatedForce += force; }
    void AddImpulse(const vec3& impulse) { if (m_Mass > 0.f) m_Velocity += impulse / m_Mass; }

    void SetVelocity(const vec3& velocity) { m_Velocity = velocity; }
    const vec3& GetVelocity() const { return m_Velocity; }

    void SetMass(f32 mass) { m_Mass = glm::max(0.0001f, mass); }
    f32 GetMass() const { return m_Mass; }

    void SetUseGravity(bool useGravity) { m_UseGravity = useGravity; }
    bool GetUseGravity() const { return m_UseGravity; }

    void SetGravityScale(f32 scale) { m_GravityScale = scale; }
    f32 GetGravityScale() const { return m_GravityScale; }

    void SetLinearDamping(f32 damping) { m_LinearDamping = glm::max(0.f, damping); }
    f32 GetLinearDamping() const { return m_LinearDamping; }

    void SetKinematic(bool isKinematic) { m_IsKinematic = isKinematic; }
    bool IsKinematic() const { return m_IsKinematic; }
#pragma endregion

#pragma region Members
private:
    PROPERTY(EDITABLE)
        f32 m_Mass = 1.f;

    PROPERTY(EDITABLE)
        bool m_UseGravity = true;

    PROPERTY(EDITABLE)
        f32 m_GravityScale = 1.f;

    PROPERTY(EDITABLE)
        f32 m_LinearDamping = 0.05f;

    PROPERTY(EDITABLE)
        bool m_IsKinematic = false;

    PROPERTY(READONLY)
        vec3 m_Velocity = vec3(0.f);

    vec3 m_AccumulatedForce = vec3(0.f);
#pragma endregion
};

END