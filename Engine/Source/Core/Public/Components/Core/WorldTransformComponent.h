#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "TransformComponent.h"

#include <glm/glm.hpp>

namespace Engine
{
    class TransformSystem;

    STRUCT(COMPONENT, TRANSIENT, DISPLAY_NAME("STRUCT_WORLD_TRANSFORM"), CATEGORY("CATEGORY_CORE"))
    struct WorldTransformComponent
    {
        REFLECT_BODY()

        PROPERTY(READONLY, DISPLAY_NAME("PROP_WORLD_MATRIX"))
        glm::mat4 worldMatrix = glm::identity<glm::mat4>();

    private:
        friend class TransformSystem;

        bool TryReuseWorld(const TransformComponent& transform, const glm::mat4& parent, glm::mat4& result) const
        {
            if (!m_CacheValid || transform.position != m_Position || transform.rotation != m_Rotation ||
                transform.scale != m_Scale || transform.absolute != m_Absolute || parent != m_ParentWorld)
                return false;

            result = m_CachedWorld;
            return true;
        }

        void CommitWorld(const TransformComponent& transform, const glm::mat4& parent, const glm::mat4& result)
        {
            m_Position = transform.position;
            m_Rotation = transform.rotation;
            m_Scale = transform.scale;
            m_Absolute = transform.absolute;
            m_ParentWorld = parent;
            m_CachedWorld = result;
            worldMatrix = result;
            m_CacheValid = true;
        }

        glm::vec3 m_Position{ 0.0f };
        glm::quat m_Rotation = glm::quat::wxyz(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 m_Scale{ 1.0f };
        ETransformAbsolute m_Absolute = ETransformAbsolute::None;
        glm::mat4 m_ParentWorld{ 1.0f };
        glm::mat4 m_CachedWorld{ 1.0f };
        bool m_CacheValid = false;
    };
}