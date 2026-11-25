#pragma once

#include "Engine_API.h"
#include "EnumBit.h"
#include "Result.h"
#include <reflection/Annotations.h>

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace Engine
{
    enum class ERuntimeSettingField : std::uint32_t
    {
        None          = 0,
        Resolution    = 1u << 0,
        Fullscreen    = 1u << 1,
        VSync         = 1u << 2,
        TargetFPS     = 1u << 3,
        RenderScale   = 1u << 4,
        QualityPreset = 1u << 5,
        Audio         = 1u << 6,
        Language      = 1u << 7,
        All           = Resolution | Fullscreen | VSync | TargetFPS | RenderScale | QualityPreset | Audio | Language
    };

    ENABLE_BITMASK_OPERATORS(ERuntimeSettingField)

    enum class ERuntimeQualityPreset : std::int32_t
    {
        Low    = 0,
        Medium = 1,
        High   = 2,
        Ultra  = 3
    };

    STRUCT()
    struct ENGINE_API RuntimeSettings
    {
        static constexpr std::uint32_t CurrentSchemaVersion = 1;
        static constexpr std::int32_t MinResolutionWidth = 320;
        static constexpr std::int32_t MaxResolutionWidth = 16384;
        static constexpr std::int32_t MinResolutionHeight = 240;
        static constexpr std::int32_t MaxResolutionHeight = 16384;
        static constexpr std::int32_t MaxTargetFPS = 1000;
        static constexpr float MinRenderScale = 0.5f;
        static constexpr float MaxRenderScale = 2.0f;
        static constexpr std::int32_t MinQualityPreset = static_cast<std::int32_t>(ERuntimeQualityPreset::Low);
        static constexpr std::int32_t MaxQualityPreset = static_cast<std::int32_t>(ERuntimeQualityPreset::Ultra);

        PROPERTY()
        std::uint32_t SchemaVersion = CurrentSchemaVersion;

        PROPERTY(META("Optional", true))
        std::int32_t ResolutionWidth = 1280;

        PROPERTY(META("Optional", true))
        std::int32_t ResolutionHeight = 720;

        PROPERTY(META("Optional", true))
        bool Fullscreen = false;

        PROPERTY(META("Optional", true))
        bool VSync = true;

        // 0: 무제한, 1..MaxTargetFPS: 목표 프레임 제한
        PROPERTY(META("Optional", true))
        std::int32_t TargetFPS = 0;

        PROPERTY(META("Optional", true))
        float RenderScale = 1.0f;

        // 0: Low, 1: Medium, 2: High, 3: Ultra (ERuntimeQualityPreset)
        PROPERTY(META("Optional", true))
        std::int32_t QualityPreset = static_cast<std::int32_t>(ERuntimeQualityPreset::High);

        PROPERTY(META("Optional", true))
        float MasterVolume = 1.0f;

        PROPERTY(META("Optional", true))
        float BGMVolume = 1.0f;

        PROPERTY(META("Optional", true))
        float SFXVolume = 1.0f;

        PROPERTY(META("Optional", true))
        float UIVolume = 1.0f;

        PROPERTY(META("Optional", true))
        float VoiceVolume = 1.0f;

        PROPERTY(META("Optional", true))
        std::string Language = "English";

        [[nodiscard]] static constexpr bool IsValidResolution(std::int32_t width, std::int32_t height) noexcept
        {
            return width >= MinResolutionWidth && width <= MaxResolutionWidth &&
                height >= MinResolutionHeight && height <= MaxResolutionHeight;
        }

        [[nodiscard]] static constexpr bool IsValidTargetFPS(std::int32_t fps) noexcept
        {
            return fps >= 0 && fps <= MaxTargetFPS;
        }

        [[nodiscard]] static bool IsValidRenderScale(float scale) noexcept
        {
            return std::isfinite(scale) && scale >= MinRenderScale && scale <= MaxRenderScale;
        }

        [[nodiscard]] static constexpr bool IsValidQualityPreset(std::int32_t preset) noexcept
        {
            return preset >= MinQualityPreset && preset <= MaxQualityPreset;
        }

        [[nodiscard]] static bool IsValidVolume(float volume) noexcept
        {
            return std::isfinite(volume) && volume >= 0.0f && volume <= 1.0f;
        }

        [[nodiscard]] static bool IsValidLanguage(std::string_view language) noexcept
        {
            return language == "English" || language == "Korean";
        }

        [[nodiscard]] ERuntimeSettingField ValidateFields() const noexcept
        {
            ERuntimeSettingField invalid = ERuntimeSettingField::None;
            if (!IsValidResolution(ResolutionWidth, ResolutionHeight))
                invalid |= ERuntimeSettingField::Resolution;
            if (!IsValidTargetFPS(TargetFPS))
                invalid |= ERuntimeSettingField::TargetFPS;
            if (!IsValidRenderScale(RenderScale))
                invalid |= ERuntimeSettingField::RenderScale;
            if (!IsValidQualityPreset(QualityPreset))
                invalid |= ERuntimeSettingField::QualityPreset;
            if (!IsValidVolume(MasterVolume) || !IsValidVolume(BGMVolume) || !IsValidVolume(SFXVolume) ||
                !IsValidVolume(UIVolume) || !IsValidVolume(VoiceVolume))
                invalid |= ERuntimeSettingField::Audio;
            if (!IsValidLanguage(Language))
                invalid |= ERuntimeSettingField::Language;
            return invalid;
        }

        [[nodiscard]] bool HasValidValues() const noexcept
        {
            return SchemaVersion == CurrentSchemaVersion && ValidateFields() == ERuntimeSettingField::None;
        }

        [[nodiscard]] ERuntimeSettingField DiffFields(const RuntimeSettings& other) const noexcept
        {
            ERuntimeSettingField changed = ERuntimeSettingField::None;
            if (ResolutionWidth != other.ResolutionWidth || ResolutionHeight != other.ResolutionHeight)
                changed |= ERuntimeSettingField::Resolution;
            if (Fullscreen != other.Fullscreen)
                changed |= ERuntimeSettingField::Fullscreen;
            if (VSync != other.VSync)
                changed |= ERuntimeSettingField::VSync;
            if (TargetFPS != other.TargetFPS)
                changed |= ERuntimeSettingField::TargetFPS;
            if (RenderScale != other.RenderScale)
                changed |= ERuntimeSettingField::RenderScale;
            if (QualityPreset != other.QualityPreset)
                changed |= ERuntimeSettingField::QualityPreset;
            if (MasterVolume != other.MasterVolume || BGMVolume != other.BGMVolume ||
                SFXVolume != other.SFXVolume || UIVolume != other.UIVolume || VoiceVolume != other.VoiceVolume)
                changed |= ERuntimeSettingField::Audio;
            if (Language != other.Language)
                changed |= ERuntimeSettingField::Language;
            return changed;
        }

        bool operator==(const RuntimeSettings&) const = default;
    };

    struct RuntimeSettingsCapabilities
    {
        bool CanApplyDisplay = false;
        bool CanApplyVSync = false;
        bool CanApplyTargetFPS = false;
        bool CanApplyAudio = false;
        bool CanApplyLanguage = false;
        bool CanApplyQuality = false;
    };

    struct RuntimeSettingsHostCallbacks
    {
        std::function<EResult(std::int32_t width, std::int32_t height, bool fullscreen)> ApplyDisplay;
        std::function<EResult(std::int32_t qualityPreset, float renderScale)> ApplyQuality;
    };

    struct RuntimeSettingsApplyResult
    {
        bool Succeeded = false;
        ERuntimeSettingField ChangedFields = ERuntimeSettingField::None;
        ERuntimeSettingField AppliedFields = ERuntimeSettingField::None;
        ERuntimeSettingField FailedFields = ERuntimeSettingField::None;
        ERuntimeSettingField UnsupportedFields = ERuntimeSettingField::None;
        std::string FailureReason;

        [[nodiscard]] bool HasFailures() const noexcept
        {
            return FailedFields != ERuntimeSettingField::None;
        }

        [[nodiscard]] bool HasUnsupported() const noexcept
        {
            return UnsupportedFields != ERuntimeSettingField::None;
        }
    };
}

ENABLE_BITMASK_OPERATORS(Engine::ERuntimeSettingField)
