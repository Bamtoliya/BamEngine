#pragma once

#include <reflection/Annotations.h>
#include "ColliderSettings.h"

#include <cmath>
#include <functional>
#include <utility>
#include <variant>

#include "ResourceHandle.h"

namespace Engine
{
    class Mesh;

    ENUM()
    enum class EColliderShapeType : uint8
    {
        Box,
        Sphere,
        Capsule,
        Mesh
    };

    STRUCT()
    struct BoxShape
    {
        REFLECT_BODY()
        static constexpr EColliderShapeType Type = EColliderShapeType::Box;

        // 로컬 공간의 전체 크기.
        vec3 Size = vec3(1.f);

        bool HasValidParameters() const
        {
            return std::isfinite(Size.x) && std::isfinite(Size.y) && std::isfinite(Size.z) &&
                Size.x > 0.f && Size.y > 0.f && Size.z > 0.f;
        }
    };

    STRUCT()
    struct SphereShape
    {
        static constexpr EColliderShapeType Type = EColliderShapeType::Sphere;

        f32 Radius = 0.5f;

        bool HasValidParameters() const
        {
            return std::isfinite(Radius) && Radius > 0.f;
        }
    };

    STRUCT()
    struct CapsuleShape
    {
        static constexpr EColliderShapeType Type = EColliderShapeType::Capsule;

        f32 Radius = 0.5f;

        // 양 끝 반구를 포함하는 전체 높이. 로컬 Y축을 사용합니다.
        f32 Height = 2.f;

        bool HasValidParameters() const
        {
            return std::isfinite(Radius) && std::isfinite(Height) &&
                Radius > 0.f && Height >= 2.f * Radius;
        }
    };

    STRUCT()
    struct MeshShape
    {
        static constexpr EColliderShapeType Type = EColliderShapeType::Mesh;

        ResourceHandle<Mesh> MeshHandle;

        // Mesh 미지정도 허용합니다. 물리 처리 시에는 유효한 리소스가 필요합니다.
        bool HasValidParameters() const { return true; }

        bool HasMesh() const { return MeshHandle.IsValid(); }
    };

    using ColliderShape = std::variant<BoxShape, SphereShape, CapsuleShape, MeshShape>;

    STRUCT()
    struct ColliderComponent
    {
        REFLECT_BODY()
        const vec3& GetOffset() const { return m_Offset; }
        const ColliderSettings& GetSettings() const { return m_Settings; }
        const ColliderShape& GetShape() const { return m_Shape; }

        EColliderShapeType GetShapeType() const
        {
            return std::visit([](const auto& shape) { return shape.Type; }, m_Shape);
        }

        [[nodiscard]]
        bool SetOffset(const vec3& offset)
        {
            if (!std::isfinite(offset.x) || !std::isfinite(offset.y) || !std::isfinite(offset.z))
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
        bool SetShape(ColliderShape shape)
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
        vec3 m_Offset = vec3(0.f);
        ColliderSettings m_Settings;
        ColliderShape m_Shape = BoxShape{};
        bool m_Dirty = true;
    };
}