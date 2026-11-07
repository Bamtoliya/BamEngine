#pragma once

#include "ComponentAnnotations.h"
#include "Structs/Capsule.h"
#include "Structs/BoundingBox.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_CHARACTER_CONTROLLER"), CATEGORY("CATEGORY_PHYSICS_3D"))
    struct CharacterControllerComponent
    {
        REFLECT_BODY()

        // 캡슐 전체 높이 (발바닥부터 머리 꼭대기까지)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_KCC_HEIGHT"))
        f32 Height = 2.0f;

        // 캡슐 반지름
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_KCC_RADIUS"))
        f32 Radius = 0.5f;

        // 발바닥 기준 캡슐 중심 오프셋 (기본 2m 높이 기준 Y=1.0m)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_KCC_CENTER"))
        vec3 Center = vec3(0.0f, 1.0f, 0.0f);

        // 충돌 관통 및 터널링 방지 여유 두께
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_KCC_SKIN_WIDTH"))
        f32 SkinWidth = 0.05f;

        // 보행 속도 (m/s)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_KCC_WALK_SPEED"))
        f32 WalkSpeed = 5.0f;

        // 질주 속도 (m/s)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_KCC_RUN_SPEED"))
        f32 RunSpeed = 8.0f;

        // 점프 초기 상승 속도 (m/s)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_KCC_JUMP_FORCE"))
        f32 JumpForce = 7.0f;

        // 중력 가속도 (m/s^2)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_KCC_GRAVITY"))
        f32 Gravity = -19.6f;

        // 걸어서 오를 수 있는 최대 경사면 각도 (도)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_KCC_SLOPE_LIMIT"))
        f32 SlopeLimitDegrees = 45.0f;

        // 걸어서 넘을 수 있는 계단/턱의 최대 높이 (m)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_KCC_STEP_OFFSET"))
        f32 StepOffset = 0.3f;

        // 공중에서의 수평 방향 조작 반영 비율 (0.0: 제어 불가, 1.0: 지상과 동일)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_KCC_AIR_CONTROL"))
        f32 AirControl = 0.2f;

        // 충돌 대상 레이어 마스크
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_COLLISION_MASK"))
        uint32 CollisionMask = 0xFFFFFFFFu;

        // ── 런타임 상태 (직렬화 제외) ──
        PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_KCC_VELOCITY"))
        vec3 Velocity = vec3(0.0f);

        PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_KCC_IS_GROUNDED"))
        bool IsGrounded = false;

        // ── 애니메이터 연동 설정 ──
        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_KCC_AUTO_UPDATE_ANIMATOR"))
        bool AutoUpdateAnimator = true;
        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_KCC_ANIMATOR_ENTITY"))
        entt::entity TargetAnimatorEntity = entt::null;
        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_KCC_PARAM_SPEED"))
        std::string SpeedParamName = "Speed";
        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_KCC_PARAM_IS_GROUNDED"))
        std::string IsGroundedParamName = "IsGrounded";
        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_KCC_PARAM_JUMP"))
        std::string JumpParamName = "Jump";
        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_SCALE_PLAY_RATE_WITH_SPEED"))
        bool ScalePlayRateWithSpeed = true;
        PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.1, 1.0), DISPLAY_NAME("PROP_MIN_PLAY_RATE"))
        f32 MinPlayRate = 0.4f;
        PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(1.0, 3.0), DISPLAY_NAME("PROP_MAX_PLAY_RATE"))
        f32 MaxPlayRate = 1.2f;

        // 런타임 제어용 비직렬화 필드
        vec3 MoveInput = vec3(0.0f);
        bool IsRunning = false;
        bool JumpRequested = false;
        bool IsStepping = false;
        vec3 GroundNormal = vec3(0.0f, 1.0f, 0.0f);
        f32 GroundAngle = 0.0f;

        // 무빙 플랫폼(Moving Platform) 연동 런타임 상태
        entt::entity GroundEntity = entt::null;
        vec3 LastGroundPos = vec3(0.0f);
        quat LastGroundRot = quat(1.0f, 0.0f, 0.0f, 0.0f);

        // ── 외부 제어 인터페이스 ──
        void Move(const vec3& inputDirection, bool run = false)
        {
            MoveInput = inputDirection;
            IsRunning = run;
        }

        void Jump()
        {
            if (IsGrounded)
                JumpRequested = true;
        }

        void SetVelocity(const vec3& vel) { Velocity = vel; }

        Capsule GetWorldCapsule(const vec3& worldPos) const
        {
            const f32 halfHeight = Height * 0.5f;
            const f32 effectiveHalf = (halfHeight > Radius) ? (halfHeight - Radius) : 0.0f;
            const vec3 centerPos = worldPos + Center;
            return Capsule(centerPos - vec3(0.0f, effectiveHalf, 0.0f), centerPos + vec3(0.0f, effectiveHalf, 0.0f), Radius);
        }

        BoundingBox GetWorldAABB(const vec3& worldPos) const
        {
            const vec3 centerPos = worldPos + Center;
            const vec3 halfExtents(Radius, Height * 0.5f, Radius);
            return BoundingBox(centerPos - halfExtents, centerPos + halfExtents);
        }
    };
}
