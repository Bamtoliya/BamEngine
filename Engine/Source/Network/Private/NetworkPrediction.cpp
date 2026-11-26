#include "NetworkPrediction.h"
#include <algorithm>
#include <cmath>

namespace Engine::Network
{
    NetworkTransformSnapshot LerpTransformSnapshot(
        const NetworkTransformSnapshot& a,
        const NetworkTransformSnapshot& b,
        float alpha)
    {
        const float t = std::clamp(alpha, 0.0f, 1.0f);

        NetworkTransformSnapshot result;
        result.position = glm::mix(a.position, b.position, t);
        result.rotation = glm::normalize(glm::slerp(a.rotation, b.rotation, t));
        result.velocity = glm::mix(a.velocity, b.velocity, t);
        return result;
    }

    NetworkTransformSnapshot ExtrapolateTransformSnapshot(
        const NetworkTransformSnapshot& prev,
        const NetworkTransformSnapshot& curr,
        double spanSeconds,
        double clampedExtrapSeconds)
    {
        NetworkTransformSnapshot result = curr;
        const float dt = static_cast<float>(clampedExtrapSeconds);
        if (dt <= 0.0f)
        {
            return result;
        }

        // 속도가 명시된 경우 우선 사용하고, 아니면 두 스냅샷 간 위치 차분으로 속도 추정
        glm::vec3 vel = curr.velocity;
        if (glm::dot(vel, vel) <= 1e-8f && spanSeconds > 1e-6)
        {
            vel = (curr.position - prev.position) / static_cast<float>(spanSeconds);
        }

        result.position = curr.position + vel * dt;
        result.velocity = vel;
        return result;
    }

    float ComputeTransformPositionError(
        const NetworkTransformSnapshot& predicted,
        const NetworkTransformSnapshot& authoritative)
    {
        return glm::distance(predicted.position, authoritative.position);
    }

    float ComputeTransformRotationErrorRadians(
        const NetworkTransformSnapshot& predicted,
        const NetworkTransformSnapshot& authoritative)
    {
        const float d = std::clamp(std::abs(glm::dot(glm::normalize(predicted.rotation), glm::normalize(authoritative.rotation))), 0.0f, 1.0f);
        return 2.0f * std::acos(d);
    }
}
