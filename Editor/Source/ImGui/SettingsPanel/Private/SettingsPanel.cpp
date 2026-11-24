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
                LOCAL_FMT("AudioPreview.SavedNotice", configPath.generic_string()),
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
    ImGui::TextDisabled("— %s", LOCAL_CSTR("AudioPreview.MixerSubtitle"));
    ImGui::Separator();
    ImGui::Spacing();

    if (!Engine::AudioManager::IsCreated())
    {
        ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.3f, 1.0f), "%s", LOCAL_CSTR("AudioPreview.AudioManagerUninitialized"));
        return;
    }

    if (!m_Initialized)
    {
        ReloadDraft();
    }

    auto& audioMgr = Engine::AudioManager::Get();
    auto& mixer = audioMgr.GetMixer();

    if (ImGui::CollapsingHeader(LOCAL_LABEL("AudioPreview.BusChannels").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
        const ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("##AudioBusTable", 4, tableFlags))
        {
            ImGui::TableSetupColumn(LOCAL_CSTR("AudioPreview.ColumnBus"), ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn(LOCAL_CSTR("AudioPreview.ColumnVolume"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(LOCAL_CSTR("AudioPreview.ColumnMute"), ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn(LOCAL_CSTR("AudioPreview.ColumnEffective"), ImGuiTableColumnFlags_WidthFixed, 100.0f);
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
    if (ImGui::CollapsingHeader(LOCAL_LABEL("AudioPreview.MixerProfileActions").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
        const std::filesystem::path configPath = "Config/AudioMixer.json";

        if (EditorUI::TextButton("AudioResetToDefault", "AudioPreview.ResetToDefault"))
        {
            ResetToDefaults();
            NotificationManager::Get().AddNotification(
                LOCAL("AudioPreview.Title"),
                LOCAL("AudioPreview.ResetNotice"),
                ELogLevel::Info
            );
        }

        EditorUI::TextButtonOptions actionOpt;
        actionOpt.SameLine = true;
        if (EditorUI::TextButton("AudioSaveSettings", "AudioPreview.SaveSettings", actionOpt))
        {
            if (mixer.SaveSettings(configPath))
            {
                m_Dirty = false;
                NotificationManager::Get().AddNotification(
                    LOCAL("AudioPreview.Title"),
                    LOCAL_FMT("AudioPreview.SavedNotice", configPath.generic_string()),
                    ELogLevel::Info
                );
            }
            else
            {
                NotificationManager::Get().AddNotification(
                    LOCAL("AudioPreview.Title"),
                    LOCAL_FMT("AudioPreview.SaveFailedNotice", configPath.generic_string()),
                    ELogLevel::Error
                );
            }
        }

        if (EditorUI::TextButton("AudioLoadSettings", "AudioPreview.LoadSettings", actionOpt))
        {
            if (mixer.LoadSettings(configPath))
            {
                ReloadDraft();
                NotificationManager::Get().AddNotification(
                    LOCAL("AudioPreview.Title"),
                    LOCAL_FMT("AudioPreview.LoadedNotice", configPath.generic_string()),
                    ELogLevel::Info
                );
            }
            else
            {
                NotificationManager::Get().AddNotification(
                    LOCAL("AudioPreview.Title"),
                    LOCAL_FMT("AudioPreview.LoadFailedNotice", configPath.generic_string()),
                    ELogLevel::Warn
                );
            }
        }

        ImGui::Spacing();
        ImGui::TextDisabled("%s", LOCAL_FMT("AudioPreview.ConfigFileLabel", configPath.generic_string()).c_str());
    }
}

void DrawAudioMixerSettings()
{
    static AudioMixerSettingsWidget s_Widget;
    s_Widget.Draw();
}

END
