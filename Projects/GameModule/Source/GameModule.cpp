#include "Module/ModuleInterface.h"
#include "Logger.h"
#include "SystemManager.h"
#include "System.h"
#include "CoreComponents.h"

class SpinSystem final : public Engine::ISystem
{
public:
    static SpinSystem* Create() { return new SpinSystem(); }

    void OnUpdate(entt::registry& globalRegistry, const std::vector<Engine::Scene*> activeScenes, f32 dt) override
    {
        // 초당 회전 각도 (도 단위)
        constexpr f32 rotateSpeed = 60.0f;

        // 실제 씬에 배치된 엔티티들을 순회
        for (Engine::Scene* scene : activeScenes)
        {
            if (!scene) continue;
            auto& sceneRegistry = scene->GetRegistry();
            auto view = sceneRegistry.view<Engine::TransformComponent, Engine::NameComponent>();

            for (auto entity : view)
            {
                const auto& nameComp = view.get<Engine::NameComponent>(entity);
                if (nameComp.name.find(L"Sphere") != std::wstring::npos || nameComp.name.find(L"Cube") != std::wstring::npos)
                {
                    auto& transform = view.get<Engine::TransformComponent>(entity);
                    glm::vec3 euler = transform.eulerRotation;
                    euler.y += rotateSpeed * dt;
                    transform.SetEulerRotation(euler);
                }
            }
        }
    }
};

class GameModule final : public Engine::IModule
{
public:
    void OnLoad() override
    {
        BAM_LOG(Info, "GameModule", "[GameModule] Registering SpinSystem...");
        Engine::SystemManager::Get().AddSystem<SpinSystem>();
    }

    void OnUnload() override
    {
        BAM_LOG(Info, "GameModule", "[GameModule] Unregistering SpinSystem...");
        Engine::SystemManager::Get().RemoveSystem<SpinSystem>();
    }

    void OnPreHotReload() override
    {
        BAM_LOG(Info, "GameModule", "[GameModule] PreHotReload triggered.");
    }

    void OnPostHotReload() override
    {
        BAM_LOG(Info, "GameModule", "[GameModule] PostHotReload finished.");
    }
};

IMPLEMENT_MODULE(GameModule)