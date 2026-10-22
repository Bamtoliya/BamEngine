#include "TransformSystem.h"
#include "CoreComponents.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <array>
#include <algorithm>
#include <optional>
#include <utility>

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
    if (IsFailure(instance->Initialize(arg)))
    {
        instance->Free();
        delete instance;
        return nullptr;
    }
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
    std::optional<TransformComponent> candidate;
    if (!transform.IsRotationSynchronized())
    {
        candidate = transform;
        static_cast<void>(candidate->SynchronizeRotation());
    }
    const auto& value = candidate ? *candidate : transform;

    glm::dmat4 left, right;
    if (!Factors(glm::dmat4(parent), value.absolute, left, right)) return false;

    auto result = left * glm::dmat4(value.GetLocalMatrix()) * right;
    result[3] = Has(value.absolute, ETransformAbsolute::Position) ?
        glm::dvec4(value.position, 1.0) : glm::dmat4(parent) * glm::dvec4(value.position, 1.0);

    if (!Affine(result) || !Finite(glm::dmat4(glm::mat4(result)))) return false;

    matrix = glm::mat4(result);
    return true;
}

bool TransformSystem::TryGetLocalMatrix(const glm::mat4& parent, ETransformAbsolute flags, const glm::mat4& desiredWorld, glm::dmat4& local)
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
    std::array<entt::entity, 256> chain{};
    std::size_t chainLen = 0;
    for (auto current = entity; current != entt::null;)
    {
        if (!registry.valid(current) || !registry.all_of<TransformComponent>(current) || chainLen >= 256 ||
            std::find(chain.begin(), chain.begin() + chainLen, current) != chain.begin() + chainLen) return false;
        chain[chainLen++] = current;
        const auto* link = registry.try_get<HierarchyComponent>(current);
        current = link ? link->parent : entt::null;
    }
    glm::mat4 result(1.0f);
    for (std::size_t k = chainLen; k > 0; --k)
    {
        const auto current = chain[k - 1];
        const auto& source = registry.get<TransformComponent>(current);
        std::optional<TransformComponent> candidate;
        if (!source.IsRotationSynchronized())
        {
            candidate = source;
            static_cast<void>(candidate->SynchronizeRotation());
        }
        const auto& transform = candidate ? *candidate : source;

        glm::mat4 next;
        const auto* cache = registry.try_get<WorldTransformComponent>(current);
        if (!cache || !cache->TryReuseWorld(transform, result, next))
        {
            if (!TryComposeWorld(transform, result, next)) return false;
        }
        result = next;
    }

    matrix = result;
    return true;
}

bool TransformSystem::UpdateHierarchy(entt::registry& registry)
{
    const auto* storage = std::as_const(registry).storage<TransformComponent>();
    if (!storage || storage->empty()) return true;
    enum class VisitState : std::uint8_t { Unvisited = 0, Visiting = 1, Resolved = 2 };
    struct Node
    {
        entt::entity Entity = entt::null;
        const TransformComponent* Source = nullptr;
        std::optional<TransformComponent> PendingTransform;
        glm::mat4 World{ 1.0f };
        glm::mat4 ParentWorld{ 1.0f };
        std::size_t Depth = 0;
        VisitState State = VisitState::Unvisited;
        bool Recomputed = false;

        const TransformComponent& GetTransform() const
        {
            return PendingTransform ? *PendingTransform : *Source;
        }
    };
    const std::size_t count = storage->size();
    struct HierarchyState
    {
        std::vector<std::pair<entt::entity, entt::entity>> Links;
        bool Valid = false;
    };

    auto& hierarchyState = registry.ctx().emplace<HierarchyState>();
    const auto canReuseHierarchy = [&]() -> bool
        {
            if (!hierarchyState.Valid || hierarchyState.Links.size() != count) return false;

            for (auto [entity, transform] : registry.view<TransformComponent>().each())
            {
                const auto* link = registry.try_get<HierarchyComponent>(entity);
                const auto parent = link ? link->parent : entt::null;
                const auto& previous = hierarchyState.Links[storage->index(entity)];
                if (previous.first != entity || previous.second != parent || !transform.IsRotationSynchronized())
                    return false;

                const auto* cache = registry.try_get<WorldTransformComponent>(entity);
                if (!cache) return false;

                glm::mat4 parentWorld(1.0f);
                if (parent != entt::null)
                {
                    if (!registry.valid(parent)) return false;
                    const auto* parentCache = registry.try_get<WorldTransformComponent>(parent);
                    if (!parentCache) return false;
                    parentWorld = parentCache->worldMatrix;
                }

                glm::mat4 cachedWorld;
                if (!cache->TryReuseWorld(transform, parentWorld, cachedWorld) || cache->worldMatrix != cachedWorld)
                    return false;
            }
            return true;
        };

    if (canReuseHierarchy()) return true;
    thread_local std::vector<Node> nodes;
    nodes.resize(count);
    for (auto [entity, transform] : registry.view<TransformComponent>().each())
    {
        auto& node = nodes[storage->index(entity)];
        node.Entity = entity;
        node.Source = &transform;
        node.PendingTransform.reset();
        node.Depth = 0;
        node.State = VisitState::Unvisited;
        node.Recomputed = false;

        if (!transform.IsRotationSynchronized())
        {
            node.PendingTransform = transform;
            static_cast<void>(node.PendingTransform->SynchronizeRotation());
        }
    }
    std::array<std::size_t, 256> chain{};
    for (std::size_t i = 0; i < count; ++i)
    {
        if (nodes[i].State == VisitState::Resolved) continue;
        std::size_t chainLen = 0;
        entt::entity current = nodes[i].Entity;
        std::size_t currentIdx = i;
        while (true)
        {
            if (chainLen >= 256) return false;
            nodes[currentIdx].State = VisitState::Visiting;
            chain[chainLen++] = currentIdx;
            const auto* link = registry.try_get<HierarchyComponent>(current);
            const entt::entity parent = link ? link->parent : entt::null;
            if (parent == entt::null)
            {
                current = entt::null;
                break;
            }
            if (!storage->contains(parent)) return false;
            const std::size_t parentIdx = storage->index(parent);
            if (nodes[parentIdx].State == VisitState::Visiting) return false;
            if (nodes[parentIdx].State == VisitState::Resolved)
            {
                current = parent;
                currentIdx = parentIdx;
                break;
            }
            current = parent;
            currentIdx = parentIdx;
        }
        glm::mat4 parentWorld = (current == entt::null) ? glm::mat4(1.0f) : nodes[currentIdx].World;
        std::size_t depth = (current == entt::null) ? 0 : nodes[currentIdx].Depth;
        if (depth + chainLen > 256) return false;
        for (std::size_t k = chainLen; k > 0; --k)
        {
            const std::size_t idx = chain[k - 1];
            auto& node = nodes[idx];
            const auto& transform = node.GetTransform();
            const auto* cache = registry.try_get<WorldTransformComponent>(node.Entity);
            if (!cache || !cache->TryReuseWorld(transform, parentWorld, node.World))
            {
                node.ParentWorld = parentWorld;
                if (!TryComposeWorld(transform, parentWorld, node.World)) return false;
                node.Recomputed = true;
            }
            parentWorld = node.World;
            node.Depth = ++depth;
            node.State = VisitState::Resolved;
        }
    }
    hierarchyState.Valid = false;
    hierarchyState.Links.resize(count);

    for (std::size_t i = 0; i < count; ++i)
    {
        const auto& node = nodes[i];
        if (node.PendingTransform)
            registry.get<TransformComponent>(node.Entity) = *node.PendingTransform;

        if (auto* cache = registry.try_get<WorldTransformComponent>(node.Entity))
        {
            if (node.Recomputed)
                cache->CommitWorld(node.GetTransform(), node.ParentWorld, node.World);
            else if (cache->worldMatrix != node.World)
                cache->worldMatrix = node.World;
        }

        const auto* link = registry.try_get<HierarchyComponent>(node.Entity);
        hierarchyState.Links[i] = { node.Entity, link ? link->parent : entt::null };
    }

    hierarchyState.Valid = true;
    return true;
}

bool TransformSystem::TryCommitTransform(entt::registry& registry, entt::entity entity, TransformComponent candidate)
{
    auto* current = registry.valid(entity) ? registry.try_get<TransformComponent>(entity) : nullptr;
    if (!current || !candidate.HasValidAbsoluteFlags() || !candidate.SynchronizeRotation() ||
        !candidate.SetLocalPosition(candidate.position) || !candidate.SetLocalScale(candidate.scale)) return false;
    if (candidate.absolute != current->absolute)
    {
        auto canonical = *current;
        if (!canonical.SynchronizeRotation() || candidate.position != canonical.position ||
            candidate.scale != canonical.scale || candidate.rotation != canonical.rotation ||
            candidate.eulerRotation != canonical.eulerRotation) return false;
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
