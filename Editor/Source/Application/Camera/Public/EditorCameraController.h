#pragma once

#include "Base.h"
#include "Components/Render/CameraComponent.h"

#include <entt/entt.hpp>
#include <string>

namespace Engine
{
    class Scene;
    class Entity;
    struct TransformComponent;
}

namespace Editor
{
    class EditorCameraController final : public Engine::Base
    {
    private:
        EditorCameraController() = default;
        ~EditorCameraController() override = default;

        bool Initialize(const std::wstring& name, const Engine::CameraComponent& settings);

    public:
        static EditorCameraController* Create(const std::wstring& name, const Engine::CameraComponent& settings);
        void Free() override;

        Engine::Entity* GetEntity() const;
        Engine::TransformComponent* GetTransformComponent() const;
        Engine::CameraComponent* GetCameraComponent() const;
        std::optional<CameraBuffer> GetCameraBuffer() const;

        bool SetAspectRatio(Engine::f32 aspectRatio);
        void RefreshMatrices();

    private:
        Engine::Scene* m_EditorScene = nullptr;
        entt::entity m_CameraEntity = entt::null;
    };
}