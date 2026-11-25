#pragma once

#include "Base.h"
#include "ConfigManager.h"
#include "Delegate.h"
#include "RuntimeSettings.h"

#include <filesystem>
#include <functional>

namespace Engine
{
    class AudioManager;
    class LocalizationManager;
    class RHI;
    class TimeManager;

    class ENGINE_API RuntimeSettingsManager final : public Base
    {
        DECLARE_SINGLETON(RuntimeSettingsManager)

    private:
        RuntimeSettingsManager() = default;
        ~RuntimeSettingsManager() override = default;
        EResult Initialize(void* arg = nullptr);

    public:
        void Free() override;

        void BindRuntime(TimeManager* timeManager, LocalizationManager* localizationManager,
            AudioManager* audioManager, RHI* rhi, RuntimeSettingsHostCallbacks hostCallbacks = {});
        void UnbindRuntime();
        void SetHostCallbacks(RuntimeSettingsHostCallbacks hostCallbacks);

        [[nodiscard]] RuntimeSettingsCapabilities GetCapabilities() const noexcept;
        [[nodiscard]] const RuntimeSettings& GetAppliedSettings() const noexcept { return m_AppliedSettings; }
        [[nodiscard]] static RuntimeSettings GetDefaultSettings() noexcept { return RuntimeSettings{}; }

        [[nodiscard]] RuntimeSettingsApplyResult ApplySettings(const RuntimeSettings& candidate, bool forceApply = false);
        [[nodiscard]] RuntimeSettingsApplyResult ResetToDefaults();

        [[nodiscard]] ConfigLoadResult LoadFromFile(const std::filesystem::path& path,
            const reflection::Registry* registry = nullptr);
        [[nodiscard]] ConfigSaveResult SaveToFile(const std::filesystem::path& path,
            const reflection::Registry* registry = nullptr) const;

        using SettingsChangedCallback = std::function<void(const RuntimeSettings&, ERuntimeSettingField)>;
        DelegateHandle AddChangeListener(SettingsChangedCallback callback);
        void RemoveChangeListener(DelegateHandle handle);

    private:
        RuntimeSettings m_AppliedSettings{};
        TimeManager* m_TimeManager = nullptr;
        LocalizationManager* m_LocalizationManager = nullptr;
        AudioManager* m_AudioManager = nullptr;
        RHI* m_RHI = nullptr;
        RuntimeSettingsHostCallbacks m_HostCallbacks{};
        MulticastDelegate<const RuntimeSettings&, ERuntimeSettingField> m_OnSettingsChanged;
    };
}
