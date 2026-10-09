#pragma once

#include "PhysicsTypes.h"

namespace Engine
{
    struct RigidBody2DComponent
    {
        ERigidBodyType BodyType = ERigidBodyType::Static;

        f32 Mass = 1.f;
        f32 LinearDamping = 0.f;
        f32 AngularDamping = 0.f;
        f32 GravityScale = 1.f;
        bool FreezeRotation = false;

        vec2 LinearVelocity = vec2(0.f);
        f32 AngularVelocity = 0.f;

        vec2 AccumulatedForce = vec2(0.f);
        f32 AccumulatedTorque = 0.f;

        // PhysicsSystem이 질량과 충돌 형상으로 계산하는 역관성 모멘트.
        f32 InverseInertia = 0.f;
    };
}