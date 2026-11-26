#pragma once

#include "Delegate.h"
#include "System.h"
#include "UIEventSystem.h"
#include <cstdint>
#include <span>
#include <string>
#include <utility>

namespace Engine
{
    struct InGameSettingsMenuDesc
    {
        std::string name = "InGameSettingsMenu";
        glm::vec2 referenceResolution{ 1920.0f, 1080.0f };
        std::int32_t zIndex = 100;
        bool startVisible = true;
        bool saveOnApply = true;
        std::string settingsFilePath = "Config/RuntimeSettings.json";
        std::string bindingsFilePath = "Config/InputBindings.json";
    };

    // RuntimeSettingsMenuComponent가 붙은 루트 Canvas 엔티티와 설정 UI 위젯 계층을 생성한다.
    ENGINE_API Entity* CreateInGameSettingsMenu(Scene& scene, const InGameSettingsMenuDesc& desc = {});

    class ENGINE_API UISettingsMenuSystem final : public ISystem
    {
    public:
        static UISettingsMenuSystem* Create(void* arg = nullptr);

        void OnInitialize() override;
        void OnFree() override;
        void OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;

        static void SyncFromManager(Scene& scene, entt::entity menuEntity);
        static void SyncAllFromManager(Scene& scene);
        static bool HandlePointerEvent(Scene& scene, const UIPointerEvent& event);

        static void OpenMenu(Scene& scene, entt::entity menuEntity);
        static void CloseMenu(Scene& scene, entt::entity menuEntity);
        static void OpenControlsPanel(Scene& scene, entt::entity menuEntity);
        static void CloseControlsPanel(Scene& scene, entt::entity menuEntity);
        static void RefreshKeyBindings(Scene& scene, entt::entity menuEntity);

        static std::span<const std::pair<std::int32_t, std::int32_t>> GetSupportedResolutions() noexcept;

    private:
        bool HandleInstancePointerEvent(Scene& scene, const UIPointerEvent& event);
        void BeginRebind(Scene& scene, entt::entity menuEntity, std::string_view mapId, std::string_view actionId);
        void CancelRebind(Scene* scene = nullptr);
        void PollRebindInput(Scene& scene);

        DelegateHandle m_SettingsListenerHandle{};
        bool m_ExternalSettingsDirty = false;
        std::string m_AwaitingRebindMapId;
        std::string m_AwaitingRebindActionId;
        entt::entity m_AwaitingRebindMenuEntity = entt::null;
    };
}
