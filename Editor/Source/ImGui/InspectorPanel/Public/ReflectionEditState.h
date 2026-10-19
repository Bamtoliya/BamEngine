#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <entt/entity/registry.hpp>

namespace Editor
{
    enum class TransformEditMode
    {
        Translation,
        Rotation,
        Scale
    };

    struct ReflectionVectorEditSettings
    {
        bool Locked = false;
        std::array<bool, 4> LockedAxes{};
        bool SnapEnabled = false;

        // 0이면 프로퍼티의 SnapStep 메타데이터를 사용합니다.
        float SnapStep = 0.0f;
    };

    struct ReflectionObjectEditSettings
    {
        std::unordered_set<std::string> ReadOnlyProperties;
        std::unordered_map<std::string, ReflectionVectorEditSettings> Vectors;
        std::unordered_map<std::string, std::string> TextBuffers;
    };

    // 에디터 편집 상태는 저장할 ECS 컴포넌트와 분리합니다.
    struct ReflectionEntityEditState
    {
        std::unordered_map<std::string, ReflectionObjectEditSettings> Components;
        std::vector<std::string> ComponentOrder; // 인스펙터 표시 순서 (QualifiedName 목록)
    };

    struct ReflectionSceneEditState
    {
        std::unordered_map<entt::entity, ReflectionEntityEditState> Entities;
    };

    // 호출자는 해당 월드에서 유효한 엔티티를 전달해야 합니다.
    inline ReflectionEntityEditState& GetReflectionEntityEditState(entt::registry& world, entt::entity entity)
    {
        auto& state = world.ctx().emplace<ReflectionSceneEditState>();
        std::erase_if(state.Entities, [&](const auto& entry) { return !world.valid(entry.first); });
        return state.Entities[entity];
    }
}
