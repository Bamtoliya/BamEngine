#include "Engine_Includes.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "ReflectionRegistry.h"
#include "RuntimeSettings.h"
#include "RuntimeSettingsManager.h"
#include "RuntimeSettingsMenuComponent.h"
#include "RuntimeUIInput.h"
#include "Scene.h"
#include "TimeManager.h"
#include "UIComponents.h"
#include "UISettingsMenuSystem.h"
#include "WorldComponents.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

int main()
{
    using namespace Engine;

    if (!ReflectionRegistry::Create())
        return 1;
    if (!TimeManager::Create())
    {
        ReflectionRegistry::Destroy();
        return 2;
    }
    if (!LocalizationManager::Create())
    {
        TimeManager::Destroy();
        ReflectionRegistry::Destroy();
        return 3;
    }
    if (!RuntimeSettingsManager::Create())
    {
        LocalizationManager::Destroy();
        TimeManager::Destroy();
        ReflectionRegistry::Destroy();
        return 4;
    }

    struct ScopeGuard
    {
        ~ScopeGuard()
        {
            RuntimeSettingsManager::Destroy();
            LocalizationManager::Destroy();
            TimeManager::Destroy();
            ReflectionRegistry::Destroy();
        }
    } guard;

    auto& mgr = RuntimeSettingsManager::Get();

    // 1. 기본값 검증
    const RuntimeSettings defaults = RuntimeSettingsManager::GetDefaultSettings();
    if (!defaults.HasValidValues() || defaults.SchemaVersion != RuntimeSettings::CurrentSchemaVersion ||
        defaults.ResolutionWidth != 1280 || defaults.ResolutionHeight != 720 ||
        defaults.Fullscreen || !defaults.VSync || defaults.TargetFPS != 0 ||
        defaults.RenderScale != 1.0f || defaults.QualityPreset != 2 ||
        defaults.MasterVolume != 1.0f || defaults.Language != "English")
    {
        std::cerr << "[RuntimeSettingsCheck] Default values mismatch.\n";
        return 5;
    }
    if (mgr.GetAppliedSettings() != defaults)
    {
        std::cerr << "[RuntimeSettingsCheck] Initial applied settings do not match defaults.\n";
        return 6;
    }

    // 2. 유효성 검사 (실패 시 기존 값 보존)
    {
        RuntimeSettings bad = defaults;
        bad.ResolutionWidth = 100;
        bad.TargetFPS = -5;
        bad.RenderScale = std::numeric_limits<float>::quiet_NaN();
        bad.QualityPreset = 99;
        bad.MasterVolume = 1.5f;
        bad.Language = "French";

        const ERuntimeSettingField invalidFields = bad.ValidateFields();
        if (!HasFlag(invalidFields, ERuntimeSettingField::Resolution) ||
            !HasFlag(invalidFields, ERuntimeSettingField::TargetFPS) ||
            !HasFlag(invalidFields, ERuntimeSettingField::RenderScale) ||
            !HasFlag(invalidFields, ERuntimeSettingField::QualityPreset) ||
            !HasFlag(invalidFields, ERuntimeSettingField::Audio) ||
            !HasFlag(invalidFields, ERuntimeSettingField::Language))
        {
            std::cerr << "[RuntimeSettingsCheck] ValidateFields missed invalid fields.\n";
            return 7;
        }

        const auto rejected = mgr.ApplySettings(bad);
        if (rejected.Succeeded || !rejected.HasFailures() || mgr.GetAppliedSettings() != defaults)
        {
            std::cerr << "[RuntimeSettingsCheck] Invalid settings were not rejected cleanly.\n";
            return 8;
        }

        RuntimeSettings badSchema = defaults;
        badSchema.SchemaVersion = 999;
        if (badSchema.HasValidValues() || mgr.ApplySettings(badSchema).Succeeded)
        {
            std::cerr << "[RuntimeSettingsCheck] Unsupported SchemaVersion was not rejected.\n";
            return 9;
        }
    }

    // 3. 적용 · 바인딩된 서브시스템 연동 · 변경 콜백 검증
    int displayCalls = 0;
    std::int32_t lastWidth = 0;
    std::int32_t lastHeight = 0;
    bool lastFullscreen = false;
    bool failDisplay = false;

    int qualityCalls = 0;
    std::int32_t lastQuality = -1;
    float lastRenderScale = 0.0f;

    RuntimeSettingsHostCallbacks callbacks;
    callbacks.ApplyDisplay = [&](std::int32_t w, std::int32_t h, bool fs) -> EResult
    {
        if (failDisplay)
            return EResult::Fail;
        ++displayCalls;
        lastWidth = w;
        lastHeight = h;
        lastFullscreen = fs;
        return EResult::Success;
    };
    callbacks.ApplyQuality = [&](std::int32_t quality, float scale) -> EResult
    {
        ++qualityCalls;
        lastQuality = quality;
        lastRenderScale = scale;
        return EResult::Success;
    };

    mgr.BindRuntime(&TimeManager::Get(), &LocalizationManager::Get(), nullptr, nullptr, callbacks);

    int listenerCalls = 0;
    ERuntimeSettingField listenerFields = ERuntimeSettingField::None;
    RuntimeSettings listenerSnapshot{};
    const DelegateHandle listenerHandle = mgr.AddChangeListener(
        [&](const RuntimeSettings& updated, ERuntimeSettingField changed)
        {
            ++listenerCalls;
            listenerFields = changed;
            listenerSnapshot = updated;
        });

    RuntimeSettings custom = defaults;
    custom.ResolutionWidth = 1920;
    custom.ResolutionHeight = 1080;
    custom.Fullscreen = true;
    custom.TargetFPS = 144;
    custom.RenderScale = 1.25f;
    custom.QualityPreset = static_cast<std::int32_t>(ERuntimeQualityPreset::Ultra);
    custom.MasterVolume = 0.6f;
    custom.BGMVolume = 0.4f;
    custom.Language = "Korean";

    const auto applyOk = mgr.ApplySettings(custom);
    if (!applyOk.Succeeded || applyOk.HasFailures() ||
        displayCalls != 1 || lastWidth != 1920 || lastHeight != 1080 || !lastFullscreen ||
        qualityCalls != 1 || lastQuality != 3 || std::abs(lastRenderScale - 1.25f) > 1e-5f ||
        TimeManager::Get().GetTargetFPS() != 144 ||
        LocalizationManager::Get().GetCurrentLanguage() != ELocalizationLanguage::Korean ||
        listenerCalls != 1 || listenerSnapshot != custom ||
        !HasFlag(listenerFields, ERuntimeSettingField::Resolution) ||
        !HasFlag(listenerFields, ERuntimeSettingField::TargetFPS) ||
        !HasFlag(listenerFields, ERuntimeSettingField::Language))
    {
        std::cerr << "[RuntimeSettingsCheck] ApplySettings or callbacks failed.\n";
        return 10;
    }

    // 동일 설정 재적용 시 변경 콜백 중복 호출 없음
    const auto noopApply = mgr.ApplySettings(custom);
    if (!noopApply.Succeeded || listenerCalls != 1 || noopApply.ChangedFields != ERuntimeSettingField::None)
    {
        std::cerr << "[RuntimeSettingsCheck] Idempotent ApplySettings should not fire listener.\n";
        return 11;
    }

    // 부분 실패 시 실패한 필드의 기존 값 보존
    {
        failDisplay = true;
        RuntimeSettings partialFail = custom;
        partialFail.ResolutionWidth = 2560;
        partialFail.ResolutionHeight = 1440;
        partialFail.TargetFPS = 60;
        const auto partialRes = mgr.ApplySettings(partialFail);
        failDisplay = false;

        if (partialRes.Succeeded ||
            !HasFlag(partialRes.FailedFields, ERuntimeSettingField::Resolution) ||
            mgr.GetAppliedSettings().ResolutionWidth != 1920 ||
            mgr.GetAppliedSettings().ResolutionHeight != 1080 ||
            mgr.GetAppliedSettings().TargetFPS != 60)
        {
            std::cerr << "[RuntimeSettingsCheck] Partial failure did not preserve failed field.\n";
            return 12;
        }
        // 복원
        (void)mgr.ApplySettings(custom);
    }

    // 4. 파일 저장/로드 왕복 검증
    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "BamEngine_RuntimeSettingsCheck";
    std::error_code fsEc;
    std::filesystem::remove_all(tempDir, fsEc);
    std::filesystem::create_directories(tempDir, fsEc);
    struct TempDirGuard
    {
        std::filesystem::path Dir;
        ~TempDirGuard()
        {
            std::error_code ec;
            std::filesystem::remove_all(Dir, ec);
        }
    } tempGuard{ tempDir };

    const std::filesystem::path settingsPath = tempDir / "RuntimeSettings.json";
    const auto saveRes = mgr.SaveToFile(settingsPath);
    if (!saveRes)
    {
        std::cerr << "[RuntimeSettingsCheck] SaveToFile failed.\n";
        return 13;
    }

    const auto resetRes = mgr.ResetToDefaults();
    if (!resetRes.Succeeded || mgr.GetAppliedSettings() != defaults)
    {
        std::cerr << "[RuntimeSettingsCheck] ResetToDefaults failed.\n";
        return 14;
    }

    const auto loadOk = mgr.LoadFromFile(settingsPath);
    if (loadOk.Status != EConfigLoadStatus::Loaded || mgr.GetAppliedSettings() != custom)
    {
        std::cerr << "[RuntimeSettingsCheck] LoadFromFile round-trip mismatch.\n";
        return 15;
    }

    // 5. 손상 파일 / 잘못된 값 파일 / 미존재 파일 시 기존 값 보존
    {
        const std::filesystem::path corruptPath = tempDir / "CorruptSettings.json";
        {
            std::ofstream ofs(corruptPath, std::ios::binary);
            ofs << "{ invalid json !!!";
        }
        const auto corruptLoad = mgr.LoadFromFile(corruptPath);
        if (corruptLoad.Status != EConfigLoadStatus::Failed || mgr.GetAppliedSettings() != custom)
        {
            std::cerr << "[RuntimeSettingsCheck] Corrupt file did not preserve existing settings.\n";
            return 16;
        }

        const std::filesystem::path invalidValuePath = tempDir / "InvalidValues.json";
        {
            std::ofstream ofs(invalidValuePath, std::ios::binary);
            ofs << R"({"Settings":{"SchemaVersion":1,"TargetFPS":-100,"Language":"English"}})";
        }
        const auto invalidLoad = mgr.LoadFromFile(invalidValuePath);
        if (invalidLoad.Status != EConfigLoadStatus::Failed || mgr.GetAppliedSettings() != custom)
        {
            std::cerr << "[RuntimeSettingsCheck] Invalid value file did not preserve existing settings.\n";
            return 17;
        }

        const auto missingLoad = mgr.LoadFromFile(tempDir / "MissingSettings.json");
        if (missingLoad.Status != EConfigLoadStatus::NotFound || mgr.GetAppliedSettings() != custom)
        {
            std::cerr << "[RuntimeSettingsCheck] Missing file did not preserve existing settings.\n";
            return 18;
        }
    }

    // 6. 인게임 설정 메뉴 UI 양방향 동기화 및 프리팹/팩토리 검증
    {
        struct SceneRelease { void operator()(Scene* s) const { if (s) s->Release(); } };
        std::unique_ptr<Scene, SceneRelease> uiScene(Scene::Create());
        if (!uiScene)
        {
            std::cerr << "[RuntimeSettingsCheck] Failed to create UI test scene.\n";
            return 19;
        }
        uiScene->SetActive(true);

        InGameSettingsMenuDesc menuDesc;
        menuDesc.settingsFilePath = (tempDir / "MenuRuntimeSettings.json").string();
        Entity* menuRoot = CreateInGameSettingsMenu(*uiScene, menuDesc);
        if (!menuRoot)
        {
            std::cerr << "[RuntimeSettingsCheck] CreateInGameSettingsMenu failed.\n";
            return 20;
        }
        const entt::entity menuHandle = menuRoot->GetEntityHandle();
        auto& uiWorld = uiScene->GetRegistry();
        const auto& menuComp = uiWorld.get<RuntimeSettingsMenuComponent>(menuHandle);

        auto resolve = [&](const SceneEntityReference& ref) -> entt::entity
        {
            Entity* e = uiScene->FindEntityById(ref.PersistentId);
            return e ? e->GetEntityHandle() : entt::null;
        };

        const entt::entity resDrop = resolve(menuComp.resolutionDropdown);
        const entt::entity qualDrop = resolve(menuComp.qualityDropdown);
        const entt::entity fsToggle = resolve(menuComp.fullscreenToggle);
        const entt::entity vsToggle = resolve(menuComp.vsyncToggle);
        const entt::entity masterSlider = resolve(menuComp.masterVolumeSlider);
        if (resDrop == entt::null || qualDrop == entt::null || fsToggle == entt::null ||
            vsToggle == entt::null || masterSlider == entt::null)
        {
            std::cerr << "[RuntimeSettingsCheck] Settings menu widget references missing.\n";
            return 21;
        }

        // 초기 동기화 검증 (현재 적용된 설정은 custom: 1920x1080, Ultra(3), Fullscreen=true, VSync=true, Master=0.6)
        if (uiWorld.get<UIDropdownComponent>(resDrop).selectedIndex != 2 ||
            uiWorld.get<UIDropdownComponent>(qualDrop).selectedIndex != 3 ||
            !uiWorld.get<UIToggleComponent>(fsToggle).checked ||
            !uiWorld.get<UIToggleComponent>(vsToggle).checked ||
            std::abs(uiWorld.get<UISliderComponent>(masterSlider).value - 0.6f) > 1e-4f)
        {
            std::cerr << "[RuntimeSettingsCheck] Initial UI sync mismatch.\n";
            return 22;
        }

        std::unique_ptr<UISettingsMenuSystem> menuSystem(UISettingsMenuSystem::Create());
        menuSystem->OnInitialize();

        RuntimeUIInput uiInput;
        const std::vector<Scene*> activeScenes{ uiScene.get() };
        const glm::vec2 refRes{ 1920.0f, 1080.0f };
        auto clickAt = [&](float px, float py)
        {
            const glm::vec2 norm = glm::vec2(px, py) / refRes;
            UIPointerInput down{ norm, true, true, true, true, false };
            uiInput.Update(activeScenes, down);
            menuSystem->OnUpdate(uiWorld, activeScenes, 0.016f);
            UIPointerInput up{ norm, true, true, false, false, true };
            uiInput.Update(activeScenes, up);
            menuSystem->OnUpdate(uiWorld, activeScenes, 0.016f);
        };

        // 해상도 드롭다운 -> 1280x720 (index 0)
        clickAt(1090.0f, 313.0f);
        clickAt(1090.0f, 348.0f);
        // 품질 드롭다운 -> Medium (index 1)
        clickAt(1090.0f, 363.0f);
        clickAt(1090.0f, 430.0f);
        // 전체화면 / VSync 토글 해제
        clickAt(896.0f, 413.0f);
        clickAt(896.0f, 463.0f);
        // 마스터 볼륨 슬라이더 0.25 위치 클릭
        clickAt(985.0f, 513.0f);

        // Apply 클릭 전에는 Manager 설정이 아직 변경되지 않아야 함
        if (mgr.GetAppliedSettings() != custom || !menuComp.hasPendingChanges)
        {
            std::cerr << "[RuntimeSettingsCheck] Pending UI edits mutated manager before Apply.\n";
            menuSystem->OnFree();
            return 23;
        }

        // Apply 버튼 클릭 -> Manager 반영 및 파일 저장
        clickAt(1074.0f, 820.0f);
        const auto& afterApply = mgr.GetAppliedSettings();
        if (afterApply.ResolutionWidth != 1280 || afterApply.ResolutionHeight != 720 ||
            afterApply.QualityPreset != 1 || afterApply.Fullscreen || afterApply.VSync ||
            std::abs(afterApply.MasterVolume - 0.25f) > 1e-4f ||
            !std::filesystem::exists(menuDesc.settingsFilePath))
        {
            std::cerr << "[RuntimeSettingsCheck] Apply button did not commit or save settings.\n";
            menuSystem->OnFree();
            return 24;
        }

        // 변경 후 Close 클릭 -> 미적용 변경 폐기 및 메뉴 숨김
        clickAt(896.0f, 413.0f);
        clickAt(1230.0f, 820.0f);
        if (uiWorld.get<UIToggleComponent>(fsToggle).checked ||
            uiWorld.get<FlagComponent>(menuHandle).flags != EEntityFlag::None)
        {
            std::cerr << "[RuntimeSettingsCheck] Close button did not discard changes or hide menu.\n";
            menuSystem->OnFree();
            return 25;
        }

        // 메뉴 재오픈 후 Defaults(Reset) 클릭 -> 기본값 복원
        UISettingsMenuSystem::OpenMenu(*uiScene, menuHandle);
        clickAt(690.0f, 820.0f);
        if (mgr.GetAppliedSettings() != defaults ||
            uiWorld.get<UIDropdownComponent>(qualDrop).selectedIndex != 2 ||
            !uiWorld.get<UIToggleComponent>(vsToggle).checked ||
            std::abs(uiWorld.get<UISliderComponent>(masterSlider).value - 1.0f) > 1e-4f)
        {
            std::cerr << "[RuntimeSettingsCheck] Reset button did not restore defaults.\n";
            menuSystem->OnFree();
            return 26;
        }

        uiInput.Clear();
        menuSystem->OnFree();
    }

    mgr.RemoveChangeListener(listenerHandle);
    mgr.UnbindRuntime();
    return 0;
}
