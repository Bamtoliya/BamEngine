#include "Engine_Includes.h"
#include "RuntimeSettingsManager.h"

#include "AudioManager.h"
#include "LocalizationManager.h"
#include "Logger.h"
#include "RHI.h"
#include "ReflectionRegistry.h"
#include "TimeManager.h"

IMPLEMENT_LAZY_SINGLETON(RuntimeSettingsManager)

EResult RuntimeSettingsManager::Initialize(void*)
{
    m_AppliedSettings = {};
    return EResult::Success;
}

void RuntimeSettingsManager::Free()
{
    UnbindRuntime();
    m_OnSettingsChanged.Clear();
}

void RuntimeSettingsManager::BindRuntime(TimeManager* timeManager, LocalizationManager* localizationManager,
    AudioManager* audioManager, RHI* rhi, RuntimeSettingsHostCallbacks hostCallbacks)
{
    m_TimeManager = timeManager;
    m_LocalizationManager = localizationManager;
    m_AudioManager = audioManager;
    m_RHI = rhi;
    m_HostCallbacks = std::move(hostCallbacks);
}

void RuntimeSettingsManager::UnbindRuntime()
{
    m_TimeManager = nullptr;
    m_LocalizationManager = nullptr;
    m_AudioManager = nullptr;
    m_RHI = nullptr;
    m_HostCallbacks = {};
}

void RuntimeSettingsManager::SetHostCallbacks(RuntimeSettingsHostCallbacks hostCallbacks)
{
    m_HostCallbacks = std::move(hostCallbacks);
}

RuntimeSettingsCapabilities RuntimeSettingsManager::GetCapabilities() const noexcept
{
    RuntimeSettingsCapabilities caps;
    caps.CanApplyDisplay = static_cast<bool>(m_HostCallbacks.ApplyDisplay);
    caps.CanApplyVSync = (m_RHI != nullptr);
    caps.CanApplyTargetFPS = (m_TimeManager != nullptr);
    caps.CanApplyAudio = (m_AudioManager != nullptr);
    caps.CanApplyLanguage = (m_LocalizationManager != nullptr);
    caps.CanApplyQuality = static_cast<bool>(m_HostCallbacks.ApplyQuality);
    return caps;
}

RuntimeSettingsApplyResult RuntimeSettingsManager::ApplySettings(const RuntimeSettings& candidate, bool forceApply)
{
    RuntimeSettingsApplyResult result;
    if (!candidate.HasValidValues())
    {
        result.FailedFields = candidate.ValidateFields();
        if (result.FailedFields == ERuntimeSettingField::None)
            result.FailedFields = ERuntimeSettingField::All;
        result.FailureReason = "Invalid runtime settings values or unsupported SchemaVersion";
        BAM_LOG(Warn, "Engine", "Config", "Rejected runtime settings: {}", result.FailureReason);
        return result;
    }

    const ERuntimeSettingField diff = m_AppliedSettings.DiffFields(candidate);
    const ERuntimeSettingField targetFields = forceApply ? ERuntimeSettingField::All : diff;
    if (targetFields == ERuntimeSettingField::None)
    {
        result.Succeeded = true;
        return result;
    }

    RuntimeSettings nextApplied = m_AppliedSettings;
    ERuntimeSettingField actualChanged = ERuntimeSettingField::None;

    // Display (Resolution / Fullscreen)
    const ERuntimeSettingField displayMask = ERuntimeSettingField::Resolution | ERuntimeSettingField::Fullscreen;
    if ((targetFields & displayMask) != ERuntimeSettingField::None)
    {
        const ERuntimeSettingField requestedDisplay = targetFields & displayMask;
        if (m_HostCallbacks.ApplyDisplay)
        {
            const EResult displayRes = m_HostCallbacks.ApplyDisplay(
                candidate.ResolutionWidth, candidate.ResolutionHeight, candidate.Fullscreen);
            if (displayRes == EResult::Success)
            {
                nextApplied.ResolutionWidth = candidate.ResolutionWidth;
                nextApplied.ResolutionHeight = candidate.ResolutionHeight;
                nextApplied.Fullscreen = candidate.Fullscreen;
                result.AppliedFields |= requestedDisplay;
                actualChanged |= (diff & displayMask);
            }
            else if (displayRes == EResult::NotImplemented)
            {
                result.UnsupportedFields |= requestedDisplay;
            }
            else
            {
                result.FailedFields |= requestedDisplay;
            }
        }
        else
        {
            nextApplied.ResolutionWidth = candidate.ResolutionWidth;
            nextApplied.ResolutionHeight = candidate.ResolutionHeight;
            nextApplied.Fullscreen = candidate.Fullscreen;
            result.UnsupportedFields |= requestedDisplay;
            actualChanged |= (diff & displayMask);
        }
    }

    // VSync
    if (HasFlag(targetFields, ERuntimeSettingField::VSync))
    {
        if (m_RHI)
        {
            EResult vsyncRes = EResult::Success;
            if (m_RHI->IsVSyncEnabled() != candidate.VSync)
                vsyncRes = m_RHI->SupportsVSync(candidate.VSync) ? m_RHI->SetVSync(candidate.VSync) : EResult::NotImplemented;

            if (vsyncRes == EResult::Success && m_RHI->IsVSyncEnabled() == candidate.VSync)
            {
                nextApplied.VSync = m_RHI->IsVSyncEnabled();
                result.AppliedFields |= ERuntimeSettingField::VSync;
                if (m_AppliedSettings.VSync != nextApplied.VSync)
                    actualChanged |= ERuntimeSettingField::VSync;
            }
            else if (vsyncRes == EResult::NotImplemented)
            {
                result.UnsupportedFields |= ERuntimeSettingField::VSync;
            }
            else
            {
                result.FailedFields |= ERuntimeSettingField::VSync;
            }
        }
        else
        {
            nextApplied.VSync = candidate.VSync;
            result.UnsupportedFields |= ERuntimeSettingField::VSync;
            if (m_AppliedSettings.VSync != nextApplied.VSync)
                actualChanged |= ERuntimeSettingField::VSync;
        }
    }

    // TargetFPS
    if (HasFlag(targetFields, ERuntimeSettingField::TargetFPS))
    {
        if (m_TimeManager)
        {
            m_TimeManager->SetTargetFPS(static_cast<uint32>(candidate.TargetFPS));
            nextApplied.TargetFPS = candidate.TargetFPS;
            result.AppliedFields |= ERuntimeSettingField::TargetFPS;
        }
        else
        {
            nextApplied.TargetFPS = candidate.TargetFPS;
            result.UnsupportedFields |= ERuntimeSettingField::TargetFPS;
        }
        if (m_AppliedSettings.TargetFPS != nextApplied.TargetFPS)
            actualChanged |= ERuntimeSettingField::TargetFPS;
    }

    // Quality / RenderScale
    const ERuntimeSettingField qualityMask = ERuntimeSettingField::QualityPreset | ERuntimeSettingField::RenderScale;
    if ((targetFields & qualityMask) != ERuntimeSettingField::None)
    {
        const ERuntimeSettingField requestedQuality = targetFields & qualityMask;
        if (m_HostCallbacks.ApplyQuality)
        {
            const EResult qualityRes = m_HostCallbacks.ApplyQuality(candidate.QualityPreset, candidate.RenderScale);
            if (qualityRes == EResult::Success)
            {
                nextApplied.QualityPreset = candidate.QualityPreset;
                nextApplied.RenderScale = candidate.RenderScale;
                result.AppliedFields |= requestedQuality;
                actualChanged |= (diff & qualityMask);
            }
            else if (qualityRes == EResult::NotImplemented)
            {
                result.UnsupportedFields |= requestedQuality;
            }
            else
            {
                result.FailedFields |= requestedQuality;
            }
        }
        else
        {
            nextApplied.QualityPreset = candidate.QualityPreset;
            nextApplied.RenderScale = candidate.RenderScale;
            result.UnsupportedFields |= requestedQuality;
            actualChanged |= (diff & qualityMask);
        }
    }

    // Audio
    if (HasFlag(targetFields, ERuntimeSettingField::Audio))
    {
        if (m_AudioManager)
        {
            m_AudioManager->SetMasterVolume(candidate.MasterVolume);
            m_AudioManager->SetBGMVolume(candidate.BGMVolume);
            m_AudioManager->SetSFXVolume(candidate.SFXVolume);
            m_AudioManager->SetUIVolume(candidate.UIVolume);
            m_AudioManager->SetVoiceVolume(candidate.VoiceVolume);
            result.AppliedFields |= ERuntimeSettingField::Audio;
        }
        else
        {
            result.UnsupportedFields |= ERuntimeSettingField::Audio;
        }
        nextApplied.MasterVolume = candidate.MasterVolume;
        nextApplied.BGMVolume = candidate.BGMVolume;
        nextApplied.SFXVolume = candidate.SFXVolume;
        nextApplied.UIVolume = candidate.UIVolume;
        nextApplied.VoiceVolume = candidate.VoiceVolume;
        if (HasFlag(diff, ERuntimeSettingField::Audio))
            actualChanged |= ERuntimeSettingField::Audio;
    }

    // Language
    if (HasFlag(targetFields, ERuntimeSettingField::Language))
    {
        const auto lang = candidate.Language == "Korean"
            ? ELocalizationLanguage::Korean : ELocalizationLanguage::English;
        if (m_LocalizationManager)
        {
            if (m_LocalizationManager->SetCurrentLanguage(lang) == EResult::Success)
            {
                nextApplied.Language = candidate.Language;
                result.AppliedFields |= ERuntimeSettingField::Language;
                if (m_AppliedSettings.Language != nextApplied.Language)
                    actualChanged |= ERuntimeSettingField::Language;
            }
            else
            {
                result.FailedFields |= ERuntimeSettingField::Language;
            }
        }
        else
        {
            nextApplied.Language = candidate.Language;
            result.UnsupportedFields |= ERuntimeSettingField::Language;
            if (m_AppliedSettings.Language != nextApplied.Language)
                actualChanged |= ERuntimeSettingField::Language;
        }
    }

    m_AppliedSettings = std::move(nextApplied);
    result.ChangedFields = actualChanged;
    result.Succeeded = (result.FailedFields == ERuntimeSettingField::None);
    if (!result.Succeeded)
        result.FailureReason = "One or more runtime settings failed to apply";

    if (actualChanged != ERuntimeSettingField::None)
        m_OnSettingsChanged.Broadcast(m_AppliedSettings, actualChanged);

    return result;
}

RuntimeSettingsApplyResult RuntimeSettingsManager::ResetToDefaults()
{
    return ApplySettings(GetDefaultSettings(), true);
}

ConfigLoadResult RuntimeSettingsManager::LoadFromFile(const std::filesystem::path& path,
    const reflection::Registry* registry)
{
    const reflection::Registry* targetRegistry = registry;
    if (!targetRegistry && ReflectionRegistry::IsCreated())
        targetRegistry = &ReflectionRegistry::Get().GetRegistry();

    if (!targetRegistry)
    {
        return { EConfigLoadStatus::Failed,
            { archive::ArchiveErrorCode::UnsupportedType, "RuntimeSettings" }, {} };
    }

    RuntimeSettings candidate{};
    const ConfigLoadResult loadResult = ConfigManager::Load(candidate, path, *targetRegistry);
    if (loadResult.Status != EConfigLoadStatus::Loaded)
        return loadResult;

    if (!candidate.HasValidValues())
    {
        BAM_LOG(Warn, "Engine", "Config",
            "Rejected RuntimeSettings file '{}' due to invalid value or SchemaVersion ({})",
            path.string(), candidate.SchemaVersion);
        return { EConfigLoadStatus::Failed,
            { archive::ArchiveErrorCode::InvalidValue, "RuntimeSettings" }, {} };
    }

    const ERuntimeSettingField changed = m_AppliedSettings.DiffFields(candidate);
    m_AppliedSettings = std::move(candidate);
    if (changed != ERuntimeSettingField::None)
        m_OnSettingsChanged.Broadcast(m_AppliedSettings, changed);

    return loadResult;
}

ConfigSaveResult RuntimeSettingsManager::SaveToFile(const std::filesystem::path& path,
    const reflection::Registry* registry) const
{
    if (!m_AppliedSettings.HasValidValues())
        return { { archive::ArchiveErrorCode::InvalidValue, "RuntimeSettings" }, {} };

    const reflection::Registry* targetRegistry = registry;
    if (!targetRegistry && ReflectionRegistry::IsCreated())
        targetRegistry = &ReflectionRegistry::Get().GetRegistry();

    if (!targetRegistry)
        return { { archive::ArchiveErrorCode::UnsupportedType, "RuntimeSettings" }, {} };

    return ConfigManager::Save(m_AppliedSettings, path, *targetRegistry);
}

DelegateHandle RuntimeSettingsManager::AddChangeListener(SettingsChangedCallback callback)
{
    return m_OnSettingsChanged.Add(std::move(callback));
}

void RuntimeSettingsManager::RemoveChangeListener(DelegateHandle handle)
{
    m_OnSettingsChanged.Remove(handle);
}