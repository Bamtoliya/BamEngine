#pragma once

#include "ComponentAnnotations.h"

#include "PhysicsTypes.h"

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_RIGID_BODY_2D"), CATEGORY("CATEGORY_PHYSICS_2D"))
    struct RigidBody2DComponent
    {
        REFLECT_BODY()
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_BODY_TYPE"))
        ERigidBodyType BodyType = ERigidBodyType::Static;

        PROPERTY(EDITABLE, RANGE(0.0, 1.0e12), DISPLAY_NAME("PROP_MASS"), EDITCONDITION(BodyType == Dynamic))
        f32 Mass = 1.f;
        PROPERTY(EDITABLE, RANGE(0.0, 1.0e12), DISPLAY_NAME("PROP_LINEAR_DAMPING"), EDITCONDITION(BodyType == Dynamic))
        f32 LinearDamping = 0.f;
        PROPERTY(EDITABLE, RANGE(0.0, 1.0e12), DISPLAY_NAME("PROP_ANGULAR_DAMPING"), EDITCONDITION(BodyType == Dynamic && !FreezeRotation))
        f32 AngularDamping = 0.f;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_GRAVITY_SCALE"), EDITCONDITION(BodyType == Dynamic))
        f32 GravityScale = 1.f;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FREEZE_ROTATION"), EDITCONDITION(BodyType == Dynamic))
        bool FreezeRotation = false;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LINEAR_VELOCITY"), EDITCONDITION(BodyType != Static))
        vec2 LinearVelocity = vec2(0.f);
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ANGULAR_VELOCITY"), EDITCONDITION(BodyType == Dynamic && !FreezeRotation))
        f32 AngularVelocity = 0.f;

        PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_ACCUMULATED_FORCE"), VISIBLE(false))
        vec2 AccumulatedForce = vec2(0.f);
        PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_ACCUMULATED_TORQUE"), VISIBLE(false))
        f32 AccumulatedTorque = 0.f;

        // PhysicsSystem이 질량과 충돌 형상으로 계산하는 역관성 모멘트.
        PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_INVERSE_INERTIA"), VISIBLE(false))
        f32 InverseInertia = 0.f;
    };
}
