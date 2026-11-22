#include "SettingsPanel.h"
#include "AudioManager.h"
#include "EditorUI.h"
#include "NotificationManager.h"
#include "LocalizationManager.h"
#include <filesystem>
#include <cstdio>
#include <algorithm>

BEGIN(Editor)

void AudioMixerSettingsWidget::ReloadDraft()
{
    if (!Engine::AudioManager::IsCreated())
    {
        m_Dirty = false;
        m_Initialized = true;
        return;
    }

    const auto& mixer = Engine::AudioManager::Get().GetMixer();
    for (size_t i = 0; i < static_cast<size_t>(Engine::EAudioBus::Count); ++i)
    {
        const auto bus = static_cast<Engine::EAudioBus>(i);
        m_Drafts[i].volume = mixer.GetBusVolume(bus);
        m_Drafts[i].muted = mixer.IsBusMuted(bus);
    }
    m_Dirty = false;
    m_Initialized = true;
}

void AudioMixerSettingsWidget::ResetToDefaults()
{
    for (size_t i = 0; i < static_cast<size_t>(Engine::EAudioBus::Count); ++i)
    {
        m_Drafts[i].volume = 1.0f;
        m_Drafts[i].muted = false;
    }
    m_Dirty = true;

    if (Engine::AudioManager::IsCreated())
    {
        auto& mixer = Engine::AudioManager::Get().GetMixer();
        for (size_t i = 0; i < static_cast<size_t>(Engine::EAudioBus::Count); ++i)
        {
            const auto bus = static_cast<Engine::EAudioBus>(i);
            mixer.SetBusVolume(bus, 1.0f);
            mixer.SetBusMuted(bus, false);
        }
    }
}

void AudioMixerSettingsWidget::Apply()
{
    if (Engine::AudioManager::IsCreated())
    {
        auto& mixer = Engine::AudioManager::Get().GetMixer();
        for (size_t i = 0; i < static_cast<size_t>(Engine::EAudioBus::Count); ++i)
        {
            const auto bus = static_cast<Engine::EAudioBus>(i);
            mixer.SetBusVolume(bus, m_Drafts[i].volume);
            mixer.SetBusMuted(bus, m_Drafts[i].muted);
        }
        const std::filesystem::path configPath = "Config/AudioMixer.json";
        if (mixer.SaveSettings(configPath))
        {
            NotificationManager::Get().AddNotification(
                LOCAL("AudioPreview.Title"),
                "Audio mixer settings saved to Config/AudioMixer.json",
                ELogLevel::Info
            );
        }
    }
    m_Dirty = false;
}

void AudioMixerSettingsWidget::Draw()
{
    ImGui::TextUnformatted(LOCAL_CSTR("AudioPreview.Title"));
    ImGui::SameLine();
    ImGui::TextDisabled("— Audio Mixer Bus Controls");
    ImGui::Separator();
    ImGui::Spacing();

    if (!Engine::AudioManager::IsCreated())
    {
        ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.3f, 1.0f), "AudioManager is not initialized.");
        return;
    }

    if (!m_Initialized)
    {
        ReloadDraft();
    }

    auto& audioMgr = Engine::AudioManager::Get();
    auto& mixer = audioMgr.GetMixer();

    if (ImGui::CollapsingHeader("Bus Channels", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("##AudioBusTable", 4, tableFlags))
        {
            ImGui::TableSetupColumn("Bus", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Mute", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Effective", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableHeadersRow();

            constexpr Engine::EAudioBus buses[] = {
                Engine::EAudioBus::Master,
                Engine::EAudioBus::BGM,
                Engine::EAudioBus::SFX,
                Engine::EAudioBus::UI,
                Engine::EAudioBus::Voice
            };

            for (size_t i = 0; i < std::size(buses); ++i)
            {
                const auto bus = buses[i];
                const char* busName = Engine::AudioMixer::GetBusName(bus);
                const size_t busIdx = static_cast<size_t>(bus);

                ImGui::TableNextRow();

                // 1. Bus Name
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(busName);

                // 2. Volume Slider (0.0 ~ 1.0)
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-FLT_MIN);
                float vol = m_Drafts[busIdx].volume;
                char sliderId[32];
                snprintf(sliderId, sizeof(sliderId), "###Vol_%s", busName);
                if (ImGui::SliderFloat(sliderId, &vol, 0.0f, 1.0f, "%.2f"))
                {
                    m_Drafts[busIdx].volume = vol;
                    mixer.SetBusVolume(bus, vol);
                    m_Dirty = true;
                }

                // 3. Mute Checkbox
                ImGui::TableSetColumnIndex(2);
                bool muted = m_Drafts[busIdx].muted;
                char muteId[32];
                snprintf(muteId, sizeof(muteId), "###Mute_%s", busName);
                if (ImGui::Checkbox(muteId, &muted))
                {
                    m_Drafts[busIdx].muted = muted;
                    mixer.SetBusMuted(bus, muted);
                    m_Dirty = true;
                }

                // 4. Effective Volume Gauge
                ImGui::TableSetColumnIndex(3);
                const float effectiveVol = mixer.GetEffectiveVolume(bus);
                char gaugeText[32];
                snprintf(gaugeText, sizeof(gaugeText), "%.0f%%", effectiveVol * 100.0f);
                ImGui::ProgressBar(effectiveVol, ImVec2(-FLT_MIN, 0.0f), gaugeText);
            }

            ImGui::EndTable();
        }
        ImGui::Spacing();
    }

    // Actions: Reset to Default, Save Settings, Load Settings
    if (ImGui::CollapsingHeader("Mixer Profile Actions", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const float buttonWidth = 140.0f;
        const float buttonHeight = 28.0f;

        if (ImGui::Button("Reset to Default", ImVec2(buttonWidth, buttonHeight)))
        {
            ResetToDefaults();
            NotificationManager::Get().AddNotification(
                LOCAL("AudioPreview.Title"),
                "Audio mixer reset to default levels.",
                ELogLevel::Info
            );
        }

        ImGui::SameLine();
        if (ImGui::Button("Save Settings", ImVec2(buttonWidth, buttonHeight)))
        {
            const std::filesystem::path configPath = "Config/AudioMixer.json";
            if (mixer.SaveSettings(configPath))
            {
                m_Dirty = false;
                NotificationManager::Get().AddNotification(
                    LOCAL("AudioPreview.Title"),
                    "Audio mixer settings saved to Config/AudioMixer.json",
                    ELogLevel::Info
                );
            }
            else
            {
                NotificationManager::Get().AddNotification(
                    LOCAL("AudioPreview.Title"),
                    "Failed to save Config/AudioMixer.json",
                    ELogLevel::Error
                );
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Load Settings", ImVec2(buttonWidth, buttonHeight)))
        {
            const std::filesystem::path configPath = "Config/AudioMixer.json";
            if (mixer.LoadSettings(configPath))
            {
                ReloadDraft();
                NotificationManager::Get().AddNotification(
                    LOCAL("AudioPreview.Title"),
                    "Audio mixer settings loaded from Config/AudioMixer.json",
                    ELogLevel::Info
                );
            }
            else
            {
                NotificationManager::Get().AddNotification(
                    LOCAL("AudioPreview.Title"),
                    "Config/AudioMixer.json not found or failed to load",
                    ELogLevel::Warn
                );
            }
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Configuration file: Config/AudioMixer.json");
    }
}

void DrawAudioMixerSettings()
{
    static AudioMixerSettingsWidget s_Widget;
    s_Widget.Draw();
}

END
