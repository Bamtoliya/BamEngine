#include "LightSystem.h"
#include "LightComponent.h"
#include "WorldTransformComponent.h"

#include <cmath>
#include <cstdio>
#include <entt/entt.hpp>
#include <glm/ext/matrix_transform.hpp>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "SkyLightDirectionCheck failed: %s\n", message);
        ++g_Failures;
    }

    bool Near(const glm::vec3& a, const glm::vec3& b) { return glm::length(a - b) < 0.0001f; }

    bool HasDirection(ELightType type, const glm::mat4& world, const glm::vec3& expected)
    {
        entt::registry registry;
        const auto entity = registry.create();
        auto& light = registry.emplace<LightComponent>(entity);
        light.LightType = type;
        registry.emplace<WorldTransformComponent>(entity).worldMatrix = world;
        LightSystem::UpdateLights(registry);
        const auto gpuLight = registry.get<LightComponent>(entity).GetGPULightDesc();
        return gpuLight && Near(gpuLight->Direction, expected);
    }
}

int main()
{
    const glm::mat4 identity(1.0f);
    const glm::mat4 yaw = glm::rotate(identity, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 roll = glm::rotate(identity, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::mat4 scaled = glm::scale(identity, glm::vec3(2.0f, 5.0f, 3.0f));
    glm::mat4 flat = identity;
    flat[1] = glm::vec4(0.0f);

    Expect(HasDirection(ELightType::Sky, identity, glm::vec3(0.0f, 1.0f, 0.0f)), "an unrotated sky light must point up");
    Expect(HasDirection(ELightType::Sky, yaw, glm::vec3(0.0f, 1.0f, 0.0f)), "yaw must not tilt the sky");
    Expect(HasDirection(ELightType::Sky, scaled, glm::vec3(0.0f, 1.0f, 0.0f)), "scale must not change the sky direction");
    Expect(HasDirection(ELightType::Sky, roll, glm::vec3(0.0f, -1.0f, 0.0f)), "rotation must still be able to flip the sky");
    Expect(HasDirection(ELightType::Sky, flat, glm::vec3(0.0f, 1.0f, 0.0f)), "a degenerate up axis must fall back to world up");

    // 다른 광원은 계속 전방을 사용한다.
    Expect(HasDirection(ELightType::Directional, identity, glm::vec3(0.0f, 0.0f, 1.0f)), "directional lights must keep the forward axis");
    Expect(HasDirection(ELightType::Directional, yaw, glm::vec3(yaw[2])), "directional lights must follow yaw");
    Expect(HasDirection(ELightType::Spot, yaw, glm::vec3(yaw[2])), "spot lights must keep the forward axis");

    return g_Failures == 0 ? 0 : 1;
}
