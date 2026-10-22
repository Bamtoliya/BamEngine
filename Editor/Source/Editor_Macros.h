#pragma once

#define LOCAL(key) LocalizationManager::Get().GetText(key)
#define LOCAL_CSTR(key) LocalizationManager::Get().GetTextForDisplay(key).c_str()
// Owns the translated label; the suffix keeps ImGui identity independent of the language.
#define LOCAL_LABEL(key) (LOCAL(key) + "###" + (key))
#define LOCAL_FMT(key, ...) fmt::format(fmt::runtime(LOCAL(key)), __VA_ARGS__)

#define RHI_TYPE ERHIType::DirectX12
#define GRAPHICS_BACKEND EGraphicsBackend::Vulkan
#define RESOURCE_PATH L"Resources/"
#define LOG_PATH L"Logs/Editor.log"

// Configuration Paths
#define CONFIG_PATH "Resources/Config/"
#define STARTUP_MANIFEST_PATH "Resources/Config/Startup/StartupManifest.json"
#define STARTUP_CONFIG_DIR "Resources/Config/Startup/"

#define ACTION_MENU_ITEM(label, actionId) \
    do { \
        auto& _sm = ShortcutManager::Get(); \
        if (ImGui::MenuItem((label), _sm.GetShortcutHint(actionId), false, _sm.CanExecuteAction(actionId))) \
        { \
            _sm.ExecuteAction(actionId); \
        } \
    } while (0)

#include "Logger.h"
#define EDITOR_LOG_TRACE(Format, ...) BAM_LOG(Trace, "Editor", "General", Format __VA_OPT__(,) __VA_ARGS__)
#define EDITOR_LOG_INFO(Format, ...) BAM_LOG(Info, "Editor", "General", Format __VA_OPT__(,) __VA_ARGS__)
#define EDITOR_LOG_WARN(Format, ...) BAM_LOG(Warn, "Editor", "General", Format __VA_OPT__(,) __VA_ARGS__)
#define EDITOR_LOG_ERROR(Format, ...) BAM_LOG(Error, "Editor", "General", Format __VA_OPT__(,) __VA_ARGS__)
#define EDITOR_LOG_FATAL(Format, ...) BAM_LOG(Fatal, "Editor", "General", Format __VA_OPT__(,) __VA_ARGS__)