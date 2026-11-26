#include "EntityPool.h"
#include "Scene.h"
#include "FlagComponent.h"

#include <utility>

namespace Engine
{
    namespace Detail
    {
        struct PoolSceneLifetime
        {
            std::shared_ptr<bool> alive = std::make_shared<bool>(true);
        };
    }

    EntityPool::EntityPool(Scene* scene, FactoryFn onCreate, ResetFn onAcquire, ResetFn onRelease)
        : m_OnCreate(std::move(onCreate))
        , m_OnAcquire(std::move(onAcquire))
        , m_OnRelease(std::move(onRelease))
    {
        BindScene(scene);
    }

    EntityPool::~EntityPool()
    {
        Clear();
    }

    EntityPool::EntityPool(EntityPool&& other) noexcept
        : m_Scene(std::exchange(other.m_Scene, nullptr))
        , m_SceneAlive(std::move(other.m_SceneAlive))
        , m_OnCreate(std::move(other.m_OnCreate))
        , m_OnAcquire(std::move(other.m_OnAcquire))
        , m_OnRelease(std::move(other.m_OnRelease))
        , m_Inactive(std::move(other.m_Inactive))
        , m_Active(std::move(other.m_Active))
    {
    }

    EntityPool& EntityPool::operator=(EntityPool&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        Clear();

        m_Scene = std::exchange(other.m_Scene, nullptr);
        m_SceneAlive = std::move(other.m_SceneAlive);
        m_OnCreate = std::move(other.m_OnCreate);
        m_OnAcquire = std::move(other.m_OnAcquire);
        m_OnRelease = std::move(other.m_OnRelease);
        m_Inactive = std::move(other.m_Inactive);
        m_Active = std::move(other.m_Active);
        return *this;
    }

    void EntityPool::BindScene(Scene* scene)
    {
        if (m_Scene == scene && (scene == nullptr || !m_SceneAlive.expired()))
        {
            return;
        }

        Clear();
        m_Scene = scene;
        m_SceneAlive.reset();

        if (m_Scene != nullptr)
        {
            auto& ctx = m_Scene->GetRegistry().ctx();
            auto* lifetime = ctx.find<Detail::PoolSceneLifetime>();
            if (!lifetime)
            {
                lifetime = &ctx.emplace<Detail::PoolSceneLifetime>();
            }
            m_SceneAlive = lifetime->alive;
        }
    }

    bool EntityPool::IsSceneValid() const noexcept
    {
        return m_Scene != nullptr && !m_SceneAlive.expired();
    }

    Entity* EntityPool::ResolveValidEntity(const PooledEntry& entry) const
    {
        if (!IsSceneValid() || entry.handle == entt::null)
        {
            return nullptr;
        }

        Entity* entity = Entity::Resolve(entry.weakRef);
        if (!entity || entity->GetScene() != m_Scene)
        {
            return nullptr;
        }

        const auto& registry = m_Scene->GetRegistry();
        if (!registry.valid(entry.handle) || m_Scene->FindEntity(entry.handle) != entity)
        {
            return nullptr;
        }

        return entity;
    }

    void EntityPool::SetEntityActiveState(entt::entity handle, bool active) const
    {
        if (!IsSceneValid() || !m_Scene->GetRegistry().valid(handle))
        {
            return;
        }

        auto& flag = m_Scene->GetRegistry().get_or_emplace<FlagComponent>(handle);
        flag.SetActive(active);
        flag.SetVisible(active);
        flag.effectiveFlags = flag.flags;
    }

    Entity* EntityPool::CreatePooledEntity()
    {
        if (!IsSceneValid())
        {
            return nullptr;
        }

        Entity* entity = m_OnCreate ? m_OnCreate(*m_Scene) : &m_Scene->CreateEntity();
        if (!entity || entity->GetScene() != m_Scene)
        {
            return nullptr;
        }

        const entt::entity handle = entity->GetEntityHandle();
        if (handle == entt::null || !m_Scene->GetRegistry().valid(handle))
        {
            return nullptr;
        }

        return entity;
    }

    std::size_t EntityPool::Prewarm(std::size_t count)
    {
        if (!IsSceneValid() || count == 0)
        {
            return 0;
        }

        PruneInvalid();
        const std::size_t currentTotal = m_Active.size() + m_Inactive.size();
        if (currentTotal >= count)
        {
            return 0;
        }

        const std::size_t toCreate = count - currentTotal;
        m_Inactive.reserve(m_Inactive.size() + toCreate);

        std::size_t created = 0;
        for (std::size_t i = 0; i < toCreate; ++i)
        {
            Entity* entity = CreatePooledEntity();
            if (!entity)
            {
                break;
            }

            const entt::entity handle = entity->GetEntityHandle();
            SetEntityActiveState(handle, false);
            m_Inactive.push_back({ handle, entity->GetWeakRef() });
            ++created;
        }

        return created;
    }

    Entity* EntityPool::Acquire()
    {
        if (!IsSceneValid())
        {
            m_Inactive.clear();
            m_Active.clear();
            return nullptr;
        }

        Entity* entity = nullptr;
        while (!m_Inactive.empty())
        {
            const PooledEntry entry = m_Inactive.back();
            m_Inactive.pop_back();

            entity = ResolveValidEntity(entry);
            if (entity != nullptr)
            {
                break;
            }
        }

        if (entity == nullptr)
        {
            entity = CreatePooledEntity();
            if (entity == nullptr)
            {
                return nullptr;
            }
        }

        const entt::entity handle = entity->GetEntityHandle();
        SetEntityActiveState(handle, true);
        m_Active.insert_or_assign(handle, entity->GetWeakRef());

        if (m_OnAcquire)
        {
            m_OnAcquire(*entity);
        }

        return entity;
    }

    bool EntityPool::Release(Entity* entity)
    {
        if (!IsSceneValid() || entity == nullptr || entity->GetScene() != m_Scene)
        {
            return false;
        }

        const entt::entity handle = entity->GetEntityHandle();
        if (handle == entt::null || !m_Scene->GetRegistry().valid(handle) || m_Scene->FindEntity(handle) != entity)
        {
            m_Active.erase(handle);
            return false;
        }

        const auto activeIt = m_Active.find(handle);
        if (activeIt == m_Active.end())
        {
            return false;
        }

        m_Active.erase(activeIt);

        if (m_OnRelease)
        {
            m_OnRelease(*entity);
        }

        // 사용자 리셋 콜백 내부에서 엔티티나 씬이 파괴되었을 수 있으므로 재검증합니다.
        if (!IsSceneValid() || !m_Scene->GetRegistry().valid(handle) || m_Scene->FindEntity(handle) != entity)
        {
            return false;
        }

        SetEntityActiveState(handle, false);
        m_Inactive.push_back({ handle, entity->GetWeakRef() });
        return true;
    }

    bool EntityPool::Release(entt::entity handle)
    {
        if (!IsSceneValid() || handle == entt::null)
        {
            return false;
        }

        return Release(m_Scene->FindEntity(handle));
    }

    void EntityPool::Clear()
    {
        if (IsSceneValid())
        {
            for (const auto& [handle, weakRef] : m_Active)
            {
                if (Entity* entity = ResolveValidEntity({ handle, weakRef }))
                {
                    (void)m_Scene->DestroyEntity(*entity);
                }
            }

            for (const auto& entry : m_Inactive)
            {
                if (Entity* entity = ResolveValidEntity(entry))
                {
                    (void)m_Scene->DestroyEntity(*entity);
                }
            }
        }

        m_Active.clear();
        m_Inactive.clear();
    }

    std::size_t EntityPool::PruneInvalid()
    {
        if (!IsSceneValid())
        {
            const std::size_t removed = m_Active.size() + m_Inactive.size();
            m_Active.clear();
            m_Inactive.clear();
            return removed;
        }

        std::size_t removed = 0;

        for (auto it = m_Active.begin(); it != m_Active.end();)
        {
            if (ResolveValidEntity({ it->first, it->second }) == nullptr)
            {
                it = m_Active.erase(it);
                ++removed;
            }
            else
            {
                ++it;
            }
        }

        std::erase_if(m_Inactive, [this, &removed](const PooledEntry& entry) {
            if (ResolveValidEntity(entry) == nullptr)
            {
                ++removed;
                return true;
            }
            return false;
        });

        return removed;
    }

    std::size_t EntityPool::GetActiveCount() const
    {
        if (!IsSceneValid())
        {
            return 0;
        }

        std::size_t count = 0;
        for (const auto& [handle, weakRef] : m_Active)
        {
            if (ResolveValidEntity({ handle, weakRef }) != nullptr)
            {
                ++count;
            }
        }
        return count;
    }

    std::size_t EntityPool::GetAvailableCount() const
    {
        if (!IsSceneValid())
        {
            return 0;
        }

        std::size_t count = 0;
        for (const auto& entry : m_Inactive)
        {
            if (ResolveValidEntity(entry) != nullptr)
            {
                ++count;
            }
        }
        return count;
    }

    std::size_t EntityPool::GetTotalCount() const
    {
        return GetActiveCount() + GetAvailableCount();
    }

    bool EntityPool::IsInUse(entt::entity handle) const
    {
        if (!IsSceneValid() || handle == entt::null)
        {
            return false;
        }

        const auto it = m_Active.find(handle);
        return it != m_Active.end() && ResolveValidEntity({ it->first, it->second }) != nullptr;
    }
}
