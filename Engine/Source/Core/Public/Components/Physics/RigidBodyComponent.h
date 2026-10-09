#pragma once

#include "PhysicsTypes.h"

namespace Engine
{
    struct RigidBodyComponent
    {
        ERigidBodyType BodyType = ERigidBodyType::Static;

        f32 Mass = 1.f;
        f32 LinearDamping = 0.f;
        f32 AngularDamping = 0.f;
        f32 GravityScale = 1.f;
        bool FreezeRotation = false;

        vec3 LinearVelocity = vec3(0.f);
        vec3 AngularVelocity = vec3(0.f);

        vec3 AccumulatedForce = vec3(0.f);
        vec3 AccumulatedTorque = vec3(0.f);

        // PhysicsSystem이 질량과 충돌 형상으로 계산하는 로컬 역관성 텐서.
        mat3 InverseInertiaLocal = mat3(0.f);
    };
}