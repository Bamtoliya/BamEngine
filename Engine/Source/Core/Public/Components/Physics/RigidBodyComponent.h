#pragma once

#include "Reflection/ComponentAnnotations.h"

#include "PhysicsTypes.h"

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_RIGID_BODY_3D"), CATEGORY("CATEGORY_PHYSICS_3D"))
    struct RigidBodyComponent
    {
        REFLECT_BODY()
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_BODY_TYPE"))
        ERigidBodyType BodyType = ERigidBodyType::Static;

        PROPERTY(EDITABLE, RANGE(0.0, 1.0e12), DISPLAY_NAME("PROP_MASS"))
        f32 Mass = 1.f;
        PROPERTY(EDITABLE, RANGE(0.0, 1.0e12), DISPLAY_NAME("PROP_LINEAR_DAMPING"))
        f32 LinearDamping = 0.f;
        PROPERTY(EDITABLE, RANGE(0.0, 1.0e12), DISPLAY_NAME("PROP_ANGULAR_DAMPING"))
        f32 AngularDamping = 0.f;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_GRAVITY_SCALE"))
        f32 GravityScale = 1.f;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FREEZE_ROTATION"))
        bool FreezeRotation = false;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LINEAR_VELOCITY"))
        vec3 LinearVelocity = vec3(0.f);
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ANGULAR_VELOCITY"))
        vec3 AngularVelocity = vec3(0.f);

        PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_ACCUMULATED_FORCE"))
        vec3 AccumulatedForce = vec3(0.f);
        PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_ACCUMULATED_TORQUE"))
        vec3 AccumulatedTorque = vec3(0.f);

        // PhysicsSystem이 질량과 충돌 형상으로 계산하는 로컬 역관성 텐서.
        PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_INVERSE_INERTIA_LOCAL"))
        mat3 InverseInertiaLocal = mat3(0.f);
    };
}
