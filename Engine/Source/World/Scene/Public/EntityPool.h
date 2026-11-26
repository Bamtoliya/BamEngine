#pragma once

#include "Engine_API.h"
#include "Entity.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Engine
{
    class Scene;

    /**
     * @brief 탄환, 적, 이펙트 등 반복 생성/파괴되는 엔티티를 재사용하기 위한 경량 오브젝트 풀.
     *
     * Scene의 lifetime을 weak_ptr 토큰으로 추적하고 Entity::WeakRef로 핸들 유효성을 검증하여,
     * 씬 파괴 또는 외부 DestroyEntity 호출 이후에도 안전하게 방어합니다.
     */
    class ENGINE_API EntityPool
    {
    public:
        using FactoryFn = std::function<Entity*(Scene&)>;
        using ResetFn = std::function<void(Entity&)>;

        EntityPool() = default;
        explicit EntityPool(Scene* scene, FactoryFn onCreate = nullptr,
            ResetFn onAcquire = nullptr, ResetFn onRelease = nullptr);
        ~EntityPool();

        EntityPool(const EntityPool&) = delete;
        EntityPool& operator=(const EntityPool&) = delete;
        EntityPool(EntityPool&& other) noexcept;
        EntityPool& operator=(EntityPool&& other) noexcept;

        // 풀을 대상 씬에 바인딩합니다. 기존 씬에 묶인 풀 엔티티는 먼저 정리합니다.
        void BindScene(Scene* scene);

        void SetOnCreate(FactoryFn fn) { m_OnCreate = std::move(fn); }
        void SetOnAcquire(ResetFn fn) { m_OnAcquire = std::move(fn); }
        void SetOnRelease(ResetFn fn) { m_OnRelease = std::move(fn); }

        // 전체 풀 용량(활성 + 대기)이 최소 count개가 되도록 비활성 엔티티를 미리 생성합니다.
        std::size_t Prewarm(std::size_t count);

        // 대기 중인 엔티티를 활성화하여 꺼내거나, 없으면 새로 생성해 반환합니다.
        [[nodiscard]] Entity* Acquire();

        // 사용이 끝난 엔티티를 비활성화하고 대기 풀로 반납합니다.
        bool Release(Entity* entity);
        bool Release(entt::entity handle);

        // 풀에서 관리 중인 모든 엔티티를 씬에서 파괴하고 내부 상태를 비웁니다.
        void Clear();

        // 씬에서 외부 요인으로 파괴된 무효 핸들을 풀 추적 목록에서 제거합니다.
        std::size_t PruneInvalid();

        [[nodiscard]] std::size_t GetActiveCount() const;
        [[nodiscard]] std::size_t GetAvailableCount() const;
        [[nodiscard]] std::size_t GetTotalCount() const;
        [[nodiscard]] bool IsInUse(entt::entity handle) const;
        [[nodiscard]] bool IsSceneValid() const noexcept;
        [[nodiscard]] Scene* GetScene() const noexcept { return m_Scene; }

    private:
        struct PooledEntry
        {
            entt::entity handle = entt::null;
            Entity::WeakRef weakRef{};
        };

        [[nodiscard]] Entity* CreatePooledEntity();
        [[nodiscard]] Entity* ResolveValidEntity(const PooledEntry& entry) const;
        void SetEntityActiveState(entt::entity handle, bool active) const;

        Scene* m_Scene = nullptr;
        std::weak_ptr<bool> m_SceneAlive{};
        FactoryFn m_OnCreate{};
        ResetFn m_OnAcquire{};
        ResetFn m_OnRelease{};
        std::vector<PooledEntry> m_Inactive{};
        std::unordered_map<entt::entity, Entity::WeakRef> m_Active{};
    };
}
