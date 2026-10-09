#pragma once

#include "ColliderSettings.h"

#include <cmath>
#include <utility>
#include <variant>

namespace Engine
{
    ENUM()
    enum class ECollider2DShapeType : uint8
    {
        Box,
        Circle,
        Capsule
    };

    STRUCT()
    struct Box2DShape
    {
		REFLECT_BODY()
        static constexpr ECollider2DShapeType Type = ECollider2DShapeType::Box;

        // 로컬 공간의 전체 크기.
        vec2 Size = vec2(1.f);

        bool HasValidParameters() const
        {
            return std::isfinite(Size.x) && std::isfinite(Size.y) && Size.x > 0.f && Size.y > 0.f;
        }
    };

    STRUCT()
    struct Circle2DShape
    {
        REFLECT_BODY()
        static constexpr ECollider2DShapeType Type = ECollider2DShapeType::Circle;

        f32 Radius = 0.5f;

        bool HasValidParameters() const
        {
            return std::isfinite(Radius) && Radius > 0.f;
        }
    };

    STRUCT()
    struct Capsule2DShape
    {
        REFLECT_BODY()
        static constexpr ECollider2DShapeType Type = ECollider2DShapeType::Capsule;

        f32 Radius = 0.5f;

        // 양 끝 반원을 포함하는 전체 높이. 로컬 Y축을 사용합니다.
        f32 Height = 2.f;

        bool HasValidParameters() const
        {
            return std::isfinite(Radius) && std::isfinite(Height) &&
                Radius > 0.f && Height >= 2.f * Radius;
        }
    };

    using Collider2DShape = std::variant<Box2DShape, Circle2DShape, Capsule2DShape>;

    STRUCT()
    struct Collider2DComponent
    {
		REFLECT_BODY()
        const vec2& GetOffset() const { return m_Offset; }
        const ColliderSettings& GetSettings() const { return m_Settings; }
        const Collider2DShape& GetShape() const { return m_Shape; }

        ECollider2DShapeType GetShapeType() const
        {
            return std::visit([](const auto& shape) { return shape.Type; }, m_Shape);
        }

        [[nodiscard]]
        bool SetOffset(const vec2& offset)
        {
            if (!std::isfinite(offset.x) || !std::isfinite(offset.y))
            {
                return false;
            }

            m_Offset = offset;
            m_Dirty = true;
            return true;
        }

        [[nodiscard]]
        bool SetSettings(const ColliderSettings& settings)
        {
            if (!std::isfinite(settings.Friction) || settings.Friction < 0.f ||
                !std::isfinite(settings.Restitution) ||
                settings.Restitution < 0.f || settings.Restitution > 1.f)
            {
                return false;
            }

            m_Settings = settings;
            m_Dirty = true;
            return true;
        }

        [[nodiscard]]
        bool SetShape(Collider2DShape shape)
        {
            const bool valid = std::visit([](const auto& value) { return value.HasValidParameters(); }, shape);

            if (!valid)
            {
                return false;
            }

            m_Shape = std::move(shape);
            m_Dirty = true;
            return true;
        }

        bool IsDirty() const { return m_Dirty; }
        void MarkDirty() { m_Dirty = true; }

        // PhysicsSystem이 관련 데이터 갱신을 완료한 뒤 호출합니다.
        void ClearDirty() { m_Dirty = false; }

    private:
        vec2 m_Offset = vec2(0.f);
        PROPERTY()
        ColliderSettings m_Settings;
        Collider2DShape m_Shape = Box2DShape{};
        bool m_Dirty = true;
    };
}