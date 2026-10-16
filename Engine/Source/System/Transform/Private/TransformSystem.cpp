#include "TransformSystem.h"
#include "CoreComponents.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <unordered_map>
#include <unordered_set>

namespace
{
    bool Finite(const glm::dmat4& value)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                if (!std::isfinite(value[c][r])) return false;
        return true;
    }

    bool Matches(const glm::dmat4& left, const glm::dmat4& right)
    {
        if (!Finite(left) || !Finite(right)) return false;
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                if (std::abs(left[c][r] - right[c][r]) >
                    1.0e-4 * std::max({ 1.0, std::abs(left[c][r]), std::abs(right[c][r]) })) return false;
        return true;
    }

    bool Affine(const glm::dmat4& value)
    {
        return Finite(value) && std::abs(value[0][3]) < 1.0e-8 && std::abs(value[1][3]) < 1.0e-8 &&
            std::abs(value[2][3]) < 1.0e-8 && std::abs(value[3][3] - 1.0) < 1.0e-8;
    }

    bool Has(Engine::ETransformAbsolute flags, Engine::ETransformAbsolute bit)
    {
        return (static_cast<unsigned>(flags) & static_cast<unsigned>(bit)) != 0;
    }

    // Basis = left * localBasis * right. Position always has its own parent/world policy.
    bool Factors(const glm::dmat4& parent, Engine::ETransformAbsolute flags, glm::dmat4& left, glm::dmat4& right)
    {
        using Flag = Engine::ETransformAbsolute;
        if ((static_cast<unsigned>(flags) & ~static_cast<unsigned>(Flag::All)) != 0 || !Affine(parent)) return false;
        const bool rotation = Has(flags, Flag::Rotation), scale = Has(flags, Flag::Scale);
        left = right = glm::dmat4(1.0);
        if (!rotation && !scale)
        {
            left = parent;
            left[3] = glm::dvec4(0, 0, 0, 1);
        }
        else if (rotation != scale)
        {
            glm::dvec3 parentScale, translation, skew;
            glm::dquat parentRotation;
            glm::dvec4 perspective;
            if (!glm::decompose(parent, parentScale, parentRotation, translation, skew, perspective)) return false;
            parentRotation = glm::normalize(parentRotation);
            const auto rebuilt = glm::translate(glm::dmat4(1.0), translation) *
                glm::mat4_cast(parentRotation) * glm::scale(glm::dmat4(1.0), parentScale);
            // A sheared/singular parent has no unambiguous rotation/scale separation in this TRS model.
            if (!Matches(parent, rebuilt)) return false;
            if (rotation) right = glm::scale(glm::dmat4(1.0), parentScale);
            else left = glm::mat4_cast(parentRotation);
        }
        return Finite(left) && Finite(right);
    }
}

EResult TransformSystem::Initialize(void*) { return EResult::Success; }
void TransformSystem::Free() {}
TransformSystem* TransformSystem::Create(void* arg)
{
    auto* instance = new TransformSystem();
    if (IsFailure(instance->Initialize(arg))) { instance->Free(); delete instance; return nullptr; }
    return instance;
}

void TransformSystem::OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*> activeScenes, f32)
{
    if (!UpdateHierarchy(globalRegistry)) ENGINE_LOG_WARN("Global transform update rejected.");
    for (Scene* scene : activeScenes)
        if (scene && !scene->IsPaused() && !UpdateHierarchy(scene->GetRegistry()))
            ENGINE_LOG_WARN("Scene transform update rejected: invalid hierarchy or unsupported absolute transform.");
}

bool TransformSystem::TryComposeWorld(const TransformComponent& transform, const glm::mat4& parent, glm::mat4& matrix)
{
    auto candidate = transform;
    static_cast<void>(candidate.SynchronizeRotation());
    glm::dmat4 left, right;
    if (!Factors(glm::dmat4(parent), candidate.absolute, left, right)) return false;
    auto result = left * glm::dmat4(candidate.GetLocalMatrix()) * right;
    result[3] = Has(candidate.absolute, ETransformAbsolute::Position) ?
        glm::dvec4(candidate.position, 1.0) : glm::dmat4(parent) * glm::dvec4(candidate.position, 1.0);
    if (!Affine(result) || !Finite(glm::dmat4(glm::mat4(result)))) return false;
    matrix = glm::mat4(result);
    return true;
}

bool TransformSystem::TryGetLocalMatrix(const glm::mat4& parent, ETransformAbsolute flags,
    const glm::mat4& desiredWorld, glm::dmat4& local)
{
    glm::dmat4 left, right;
    const glm::dmat4 parentMatrix(parent), desired(desiredWorld);
    if (!Affine(desired) || !Factors(parentMatrix, flags, left, right) ||
        glm::determinant(left) == 0.0 || glm::determinant(right) == 0.0) return false;
    auto result = glm::inverse(left) * desired * glm::inverse(right);
    if (Has(flags, ETransformAbsolute::Position)) result[3] = desired[3];
    else
    {
        if (glm::determinant(parentMatrix) == 0.0) return false;
        result[3] = glm::inverse(parentMatrix) * desired[3];
    }
    if (!Affine(result)) return false;
    local = result;
    return true;
}

bool TransformSystem::TryAssignLocalMatrix(TransformComponent& candidate, const glm::dmat4& local)
{
    if (!Affine(local)) return false;
    auto value = candidate;
    if (!value.SynchronizeRotation() || !value.SetLocalPosition(glm::vec3(local[3]))) return false;
    if (!Matches(glm::dmat4(value.GetLocalMatrix()), local))
    {
        glm::dvec3 scale, translation, skew;
        glm::dvec4 perspective;
        glm::dquat rotation;
        if (!glm::decompose(local, scale, rotation, translation, skew, perspective) ||
            !value.SetLocalPosition(glm::vec3(translation)) || !value.SetLocalScale(glm::vec3(scale)) ||
            !value.SetLocalRotation(glm::quat(rotation))) return false;
    }
    if (!Matches(glm::dmat4(value.GetLocalMatrix()), local)) return false;
    candidate = value;
    return true;
}

bool TransformSystem::TryGetWorldMatrix(const entt::registry& registry, entt::entity entity, glm::mat4& matrix)
{
    std::vector<entt::entity> chain;
    std::unordered_set<entt::entity> visited;
    for (auto current = entity; current != entt::null;)
    {
        if (!registry.valid(current) || !registry.all_of<TransformComponent>(current) ||
            !visited.insert(current).second || visited.size() > 256) return false;
        chain.push_back(current);
        const auto* link = registry.try_get<HierarchyComponent>(current);
        current = link ? link->parent : entt::null;
    }
    glm::mat4 result(1.0f);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
        if (!TryComposeWorld(registry.get<TransformComponent>(*it), result, result)) return false;
    matrix = result;
    return true;
}

bool TransformSystem::UpdateHierarchy(entt::registry& registry)
{
    std::unordered_map<entt::entity, TransformComponent> values;
    for (const auto entity : registry.view<TransformComponent>())
    {
        auto value = registry.get<TransformComponent>(entity);
        static_cast<void>(value.SynchronizeRotation());
        values.emplace(entity, value);
    }
    struct Resolved { glm::mat4 World{ 1.0f }; std::size_t Depth = 0; };
    std::unordered_map<entt::entity, Resolved> resolved;
    for (const auto& [entity, transform] : values)
    {
        if (resolved.contains(entity)) continue;
        std::vector<entt::entity> chain;
        std::unordered_set<entt::entity> visited;
        auto current = entity;
        while (current != entt::null && !resolved.contains(current))
        {
            if (!values.contains(current) || !visited.insert(current).second || chain.size() >= 256) return false;
            chain.push_back(current);
            const auto* link = registry.try_get<HierarchyComponent>(current);
            current = link ? link->parent : entt::null;
        }
        Resolved parent = current == entt::null ? Resolved{} : resolved.at(current);
        if (parent.Depth + chain.size() > 256) return false;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it)
        {
            if (!TryComposeWorld(values.at(*it), parent.World, parent.World)) return false;
            ++parent.Depth;
            resolved.emplace(*it, parent);
        }
    }
    for (const auto& [entity, value] : values) registry.get<TransformComponent>(entity) = value;
    for (const auto& [entity, value] : resolved)
        if (auto* cache = registry.try_get<WorldTransformComponent>(entity)) cache->worldMatrix = value.World;
    return true;
}

bool TransformSystem::TryCommitTransform(entt::registry& registry, entt::entity entity, TransformComponent candidate)
{
    auto* current = registry.valid(entity) ? registry.try_get<TransformComponent>(entity) : nullptr;
    if (!current || !candidate.HasValidAbsoluteFlags() || !candidate.SynchronizeRotation() ||
        !candidate.SetLocalPosition(candidate.position) || !candidate.SetLocalScale(candidate.scale)) return false;
    if (candidate.absolute != current->absolute)
    {
        glm::mat4 desired, parent(1.0f), rebuilt;
        const auto* link = registry.try_get<HierarchyComponent>(entity);
        if (!TryGetWorldMatrix(registry, entity, desired) ||
            (link && !TryGetWorldMatrix(registry, link->parent, parent))) return false;
        glm::dmat4 local;
        if (!TryGetLocalMatrix(parent, candidate.absolute, desired, local) || !TryAssignLocalMatrix(candidate, local) ||
            !TryComposeWorld(candidate, parent, rebuilt) || !Matches(glm::dmat4(rebuilt), glm::dmat4(desired))) return false;
    }
    const auto previous = *current;
    *current = candidate;
    if (UpdateHierarchy(registry)) return true;
    *current = previous;
    return false;
}

bool TransformSystem::TrySetAbsolute(entt::registry& registry, entt::entity entity, ETransformAbsolute flags)
{
    const auto* current = registry.valid(entity) ? registry.try_get<TransformComponent>(entity) : nullptr;
    if (!current) return false;
    auto candidate = *current;
    candidate.absolute = flags;
    return TryCommitTransform(registry, entity, candidate);
}