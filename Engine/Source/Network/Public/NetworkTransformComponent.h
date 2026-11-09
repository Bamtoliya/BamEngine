#pragma once

#include "ComponentAnnotations.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_NETWORK_TRANSFORM"), CATEGORY("CATEGORY_NETWORK"))
        struct NetworkTransformComponent
    {
        REFLECT_BODY()

            // 초당 동기화 횟수 (기본 20Hz)
            PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SYNC_RATE"), STEP(1.0), SNAP_STEP(5.0))
            float SyncRate = 20.0f;

        // 위치 변화 감지 임계치 (단위: 미터, 1cm 이상 이동 시 전송)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_POS_THRESHOLD"), STEP(0.01))
            float PositionThreshold = 0.01f;

        // 회전 변화 감지 임계치 (단위: 라디안, 약 0.5도)
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ROT_THRESHOLD"), STEP(0.01))
            float RotationThreshold = 0.0087f;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SYNC_POSITION"))
            bool SyncPosition = true;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SYNC_ROTATION"))
            bool SyncRotation = true;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SYNC_SCALE"))
            bool SyncScale = false; // 기본적으로 스케일은 고정 에셋이 많으므로 대역폭 절약

        // 클라이언트 수신측 LERP 보간 추종 속도
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_INTERPOLATION_SPEED"), STEP(1.0))
            float InterpolationSpeed = 15.0f;

        // --- 런타임 비직렬화 상태 필드 ---
        glm::vec3 LastSentPosition{ 0.0f };
        glm::quat LastSentRotation = glm::identity<glm::quat>();
        glm::vec3 LastSentScale{ 1.0f };

        glm::vec3 TargetPosition{ 0.0f };
        glm::quat TargetRotation = glm::identity<glm::quat>();
        glm::vec3 TargetScale{ 1.0f };

        float SyncAccumulator = 0.0f;
        bool HasInitialState = false;
    };
}