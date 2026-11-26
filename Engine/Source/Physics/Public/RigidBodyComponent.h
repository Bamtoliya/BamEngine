#pragma once

#include "ComponentAnnotations.h"

#include "PhysicsTypes.h"

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_RIGID_BODY_3D"), CATEGORY("CATEGORY_PHYSICS_3D"))
    struct RigidBodyComponent
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

        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_USE_CCD"), EDITCONDITION(BodyType == Dynamic))
        bool UseCCD = false;

        PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 1.0e12), DISPLAY_NAME("PROP_CCD_MOTION_THRESHOLD"), EDITCONDITION(BodyType == Dynamic && UseCCD))
        f32 CCDMotionThreshold = 0.25f;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LINEAR_VELOCITY"), EDITCONDITION(BodyType != Static))
        vec3 LinearVelocity = vec3(0.f);
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ANGULAR_VELOCITY"), EDITCONDITION(BodyType == Dynamic && !FreezeRotation))
        vec3 AngularVelocity = vec3(0.f);

        PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_ACCUMULATED_FORCE"), VISIBLE(false))
        vec3 AccumulatedForce = vec3(0.f);
        PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_ACCUMULATED_TORQUE"), VISIBLE(false))
        vec3 AccumulatedTorque = vec3(0.f);

        // PhysicsSystem이 질량과 충돌 형상으로 계산하는 로컬 역관성 텐서.
        PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_INVERSE_INERTIA_LOCAL"), VISIBLE(false))
        mat3 InverseInertiaLocal = mat3(0.f);
    };
}
