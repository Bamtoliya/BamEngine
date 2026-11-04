#pragma once

#include "Reflection/ComponentAnnotations.h"
#include <glm/glm.hpp>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_THIRD_PERSON_CONTROLLER"), CATEGORY("CATEGORY_PHYSICS"), REQUIRE_COMPONENTS(TransformComponent, WorldTransformComponent, CharacterControllerComponent))
        struct ThirdPersonControllerComponent
    {
        REFLECT_BODY()

            // 조작할 스프링 암 엔티티 (비어있으면 동일 엔티티 또는 자식/부모에서 자동 탐색)
            PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPRING_ARM_ENTITY"))
            entt::entity SpringArmEntity = entt::null;

        // 마우스 회전 감도
        PROPERTY(EDITABLE, RANGE(0.01, 2.0), DISPLAY_NAME("PROP_MOUSE_SENSITIVITY"))
            f32 MouseSensitivity = 0.15f;

        // 마우스 휠 줌 감도
        PROPERTY(EDITABLE, RANGE(0.1, 5.0), DISPLAY_NAME("PROP_ZOOM_SENSITIVITY"))
            f32 ZoomSensitivity = 0.5f;

        // 마우스 우클릭을 누르고 있을 때만 카메라 회전 (false면 상시 회전)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_REQUIRE_RIGHT_CLICK_ROTATE"))
            bool RequireRightClickToRotate = true;

        // Pitch(상하) 회전 반전 여부
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_INVERT_PITCH"))
            bool InvertPitch = false;

        // 캐릭터가 이동하는 방향을 바라보도록 자동 회전
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ORIENT_ROTATION_TO_MOVEMENT"))
            bool OrientRotationToMovement = true;

        // 캐릭터 회전 보간 속도 (deg/s)
        PROPERTY(EDITABLE, RANGE(1.0, 30.0), DISPLAY_NAME("PROP_ROTATION_SMOOTH_SPEED"))
            f32 RotationSmoothSpeed = 12.0f;

        // 입력 활성화 여부
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_INPUT_ENABLED"))
            bool InputEnabled = true;

        // Shift(달리기) 시 속도 배율
        PROPERTY(EDITABLE, RANGE(1.0, 3.0), DISPLAY_NAME("PROP_SPRINT_MULTIPLIER"))
            f32 SprintMultiplier = 1.6f;
    };
}