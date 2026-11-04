#pragma once

#include "Reflection/ComponentAnnotations.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_SPRING_ARM"), CATEGORY("CATEGORY_CAMERA"))
        struct SpringArmComponent
    {
        REFLECT_BODY()

            // 추종할 대상 엔티티 (비어있고 부모가 있으면 부모 엔티티를 자동 추종)
            PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TARGET_ENTITY"))
            entt::entity TargetEntity = entt::null;

        // 목표 암 길이 (m)
        PROPERTY(EDITABLE, RANGE(0.1, 100.0), DISPLAY_NAME("PROP_TARGET_ARM_LENGTH"))
            f32 TargetArmLength = 4.0f;

        // 타깃 오브젝트(캐릭터) 중심 기준점 오프셋 (머리/상체 높이)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TARGET_OFFSET"))
            vec3 TargetOffset = vec3(0.0f, 1.6f, 0.0f);

        // 암 끝단에서 카메라 소켓 오프셋 (어깨 너머 숄더 뷰 등)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SOCKET_OFFSET"))
            vec3 SocketOffset = vec3(0.0f, 0.3f, 0.0f);

        // 벽 충돌 감지용 구체 프로브 반지름
        PROPERTY(EDITABLE, RANGE(0.01, 1.0), DISPLAY_NAME("PROP_PROBE_RADIUS"))
            f32 ProbeRadius = 0.25f;

        // 벽 가림 시 카메라를 앞으로 당길지 여부
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_DO_COLLISION_TEST"))
            bool DoCollisionTest = true;

        // 카메라 위치 지연 스무딩
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLE_CAMERA_LAG"))
            bool EnableCameraLag = true;

        // 위치 지연 추종 속도
        PROPERTY(EDITABLE, RANGE(0.1, 50.0), DISPLAY_NAME("PROP_CAMERA_LAG_SPEED"))
            f32 CameraLagSpeed = 12.0f;

        // 상하 회전 각도 제한 (Pitch Min / Max)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MIN_PITCH"))
            f32 MinPitch = -75.0f;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MAX_PITCH"))
            f32 MaxPitch = 75.0f;

        // 회전 각도 (도 단위)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PITCH"))
            f32 Pitch = -15.0f;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_YAW"))
            f32 Yaw = 0.0f;

        // 충돌 검사 대상 레이어 마스크
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_COLLISION_MASK"))
            uint32 CollisionMask = 0xFFFFFFFFu;

        // ── 런타임 상태 (직렬화 제외) ──
        PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_CURRENT_ARM_LENGTH"))
            f32 CurrentArmLength = 4.0f;

        vec3 PreviousDesiredPosition = vec3(0.0f);
        vec3 AnchorPosition = vec3(0.0f);
        bool HasAnchor = false;
        bool IsInitialized = false;

        // ── 회전 제어 헬퍼 ──
        void AddPitch(f32 deltaDegrees)
        {
            Pitch = std::clamp(Pitch + deltaDegrees, MinPitch, MaxPitch);
        }

        void AddYaw(f32 deltaDegrees)
        {
            Yaw += deltaDegrees;
            while (Yaw > 180.0f) Yaw -= 360.0f;
            while (Yaw < -180.0f) Yaw += 360.0f;
        }

        void SetTargetEntity(entt::entity target) { TargetEntity = target; }
    };
}