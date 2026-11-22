#pragma once

#include "Editor_Includes.h"
#include "AudioMixer.h"
#include <array>

BEGIN(Editor)

class UnifiedSettingsPanel;

class AudioMixerSettingsWidget
{
public:
    AudioMixerSettingsWidget() = default;
    ~AudioMixerSettingsWidget() = default;

    void ReloadDraft();
    void ResetToDefaults();
    void Apply();
    void Draw();

    [[nodiscard]] bool IsDirty() const { return m_Dirty; }

private:
    struct BusDraft
    {
        float volume = 1.0f;
        bool muted = false;
    };

    std::array<BusDraft, static_cast<size_t>(Engine::EAudioBus::Count)> m_Drafts{};
    bool m_Dirty = false;
    bool m_Initialized = false;
};

// Standalone entry point to render Audio Mixer controls in SettingsPanel
void DrawAudioMixerSettings();

// Forwarding alias so that SettingsPanel can be used to refer to UnifiedSettingsPanel
using SettingsPanel = UnifiedSettingsPanel;

END
